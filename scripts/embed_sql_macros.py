#!/usr/bin/env python3
"""
Generate a C++ header file with embedded SQL macros.
This script reads SQL files and creates a header with raw string literals.
Handles MSVC's 16KB string literal limit by splitting large files.
"""

import os
import re
import sys
from pathlib import Path

# MSVC has a 16380 byte limit per string literal - use 14000 to be safe
MAX_CHUNK_SIZE = 14000

def escape_raw_string_delimiter(content):
    """Ensure the content doesn't contain )SQLMACRO" which would break our delimiter."""
    if ')SQLMACRO"' in content:
        raise ValueError("SQL content contains raw string delimiter - please use a different delimiter")
    return content

def split_content(content, max_size=MAX_CHUNK_SIZE):
    """Split content into chunks, breaking at newlines."""
    if len(content) <= max_size:
        return [content]

    chunks = []
    current_chunk = ""

    for line in content.split('\n'):
        line_with_newline = line + '\n'
        if len(current_chunk) + len(line_with_newline) > max_size and current_chunk:
            chunks.append(current_chunk)
            current_chunk = line_with_newline
        else:
            current_chunk += line_with_newline

    if current_chunk:
        # Remove trailing newline from last chunk
        chunks.append(current_chunk.rstrip('\n'))

    return chunks


# -----------------------------------------------------------------------------
# Macro documentation (#170)
# -----------------------------------------------------------------------------
# sitting_duck's 96 SQL macros reached duckdb_functions() with no description
# and no examples, while its C++ functions had both. That is the half of #170
# that was never done, and it matters because duckdb_functions() is the only
# thing an agent connected to a database can read.
#
# The descriptions are NOT written here. They are lifted from the convention the
# .sql files ALREADY use, so a macro's documentation lives beside the macro and
# cannot drift from it:
#
#     -- =========================================================
#     -- ast_callers: Who calls a given function?
#     -- =========================================================
#     --
#     -- ...prose...
#     --
#     -- Usage:
#     --   SELECT * FROM ast_callers('src/**/*.py');
#
#     CREATE OR REPLACE MACRO ast_callers(...)
#
# A `-- <name>: <summary>` banner becomes the description; indented lines under
# `-- Usage:` become examples. DELIBERATELY STRICT: a macro without a banner
# naming it gets no description rather than inheriting the nearest comment.
# Several comment blocks are section headers covering a GROUP of macros
# ("AST-anchored source patching: ast_node_edit / ast_patch / ..."), and
# attributing those to one macro would populate the field while misdescribing
# the function -- passing the metric and failing the purpose.

MACRO_DEF_RE = re.compile(
    r'CREATE\s+(?:OR\s+REPLACE\s+)?(?:TEMP\s+)?MACRO\s+([A-Za-z_][A-Za-z0-9_]*)', re.I)


def _cpp_escape(text):
    return text.replace('\\', '\\\\').replace('"', '\\"')


def extract_macro_docs(sql_dir, sql_files):
    """Return [(macro_name, description, [examples])] for macros that document themselves."""
    docs = {}
    order = []
    for sql_file in sql_files:
        path = os.path.join(sql_dir, sql_file)
        if not os.path.exists(path):
            continue
        lines = open(path, 'r', errors='replace').read().splitlines()
        for idx, line in enumerate(lines):
            m = MACRO_DEF_RE.search(line)
            if not m:
                continue
            name = m.group(1)
            if name in docs:
                continue

            description = None
            examples = []

            # Look back over a bounded window: far enough to clear a prose block,
            # short enough that an unrelated macro's banner cannot be claimed.
            window = range(idx - 1, max(-1, idx - 80), -1)

            for j in window:
                stripped = lines[j].strip()
                b = re.match(r'--\s*' + re.escape(name) + r'\s*[:\u2014-]\s*(.+)', stripped)
                if b:
                    summary = b.group(1).strip()
                    # A banner naming several macros describes the group, not this one.
                    if '/' in summary.split(':')[0]:
                        break
                    # Banners wrap. Join the continuation lines, or the description
                    # ends mid-sentence ("convenience wrapper that parses source
                    # files and then runs").
                    parts = [summary]
                    for k in range(j + 1, min(len(lines), j + 6)):
                        nxt = lines[k].strip()
                        if not nxt.startswith('--'):
                            break
                        cont = nxt[2:].strip()
                        if not cont or set(cont) <= set('=- '):
                            break
                        # Stop at a section marker OR a maintainer note. A
                        # `TODO(v2.0.0): ...` line sits under some banners to
                        # record that the macro is slated for replacement by the
                        # C++ selector compiler; that is roadmap, not something
                        # an agent reading duckdb_functions() should be told.
                        if re.match(r'(Usage|Returns|Example)s?\s*:', cont, re.I):
                            break
                        if re.match(r'(TODO|FIXME|NOTE|XXX|WARNING)\b', cont, re.I):
                            break
                        parts.append(cont)
                    description = ' '.join(parts).strip().rstrip('.')
                    break

            for j in window:
                if re.match(r'--\s*Usage\s*:', lines[j].strip(), re.I):
                    for k in range(j + 1, idx):
                        t = lines[k].strip()
                        if not t.startswith('--'):
                            continue
                        t = t[2:].strip()
                        if not t.upper().startswith(('SELECT', 'WITH', 'CREATE', 'FROM', 'COPY', 'PRAGMA')):
                            continue
                        # A Usage: block is often shared by a family of macros.
                        # Keep only the lines that actually exercise THIS one,
                        # otherwise ast_select_from inherits ast_select's examples.
                        if name in t:
                            examples.append(t)
                    break

            if description:
                docs[name] = (description, examples)
                order.append(name)

    return [(n, docs[n][0], docs[n][1]) for n in order]


def generate_header(sql_dir, output_file):
    """Generate C++ header with embedded SQL macros."""

    sql_files = [
        'semantic_predicates.sql',
        'file_utilities.sql',
        'tree_navigation.sql',
        'pattern_matching.sql',
        'relational_operators.sql',
        'parse_ast_list_table.sql',
        'css_selectors.sql',
        'selector_for.sql',
        'ast_select_rules.sql',
        'scope_resolution.sql',
        'duck_blocks.sql',
        'ast_patch.sql',
        'ast_unparse.sql',
    ]

    header_content = """// Auto-generated file - DO NOT EDIT
// Generated from SQL macro files in src/sql_macros/
// Run: python scripts/embed_sql_macros.py
// Note: Large files are split into chunks to handle MSVC's 16KB string literal limit

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace duckdb {

// SQL macro definitions embedded at compile time
static const std::vector<std::pair<std::string, std::string>> EMBEDDED_SQL_MACROS = {
"""

    for sql_file in sql_files:
        filepath = os.path.join(sql_dir, sql_file)
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                content = f.read()

            # Verify content doesn't break our delimiter
            escape_raw_string_delimiter(content)

            # Split into chunks for MSVC compatibility
            chunks = split_content(content)

            if len(chunks) == 1:
                # Small file - use simple format
                header_content += f'''    {{"{sql_file}", R"SQLMACRO(
{content}
)SQLMACRO"}},
'''
            else:
                # Large file - concatenate chunks
                header_content += f'    {{"{sql_file}", '
                for i, chunk in enumerate(chunks):
                    if i > 0:
                        header_content += '\n        '
                    header_content += f'R"SQLMACRO(\n{chunk}\n)SQLMACRO"'
                header_content += '},\n'

    header_content += """};

"""

    # Documentation table (#170). Keyed by macro name, consumed by
    # RegisterASTSQLMacros to decorate the catalog entries after creation.
    docs = extract_macro_docs(sql_dir, sql_files)
    header_content += """// Per-macro documentation, lifted from the `-- <name>: <summary>` banners and
// `-- Usage:` blocks in src/sql_macros/*.sql. Macros without a banner naming
// themselves are absent rather than guessed at -- see extract_macro_docs().
struct EmbeddedMacroDoc {
\tconst char *name;
\tconst char *description;
\tstd::vector<std::string> examples;
};

static const std::vector<EmbeddedMacroDoc> EMBEDDED_MACRO_DOCS = {
"""
    for name, description, examples in docs:
        ex = ', '.join(f'"{_cpp_escape(e)}"' for e in examples)
        header_content += f'    {{"{name}", "{_cpp_escape(description)}", {{{ex}}}}},\n'

    header_content += """};

} // namespace duckdb
"""
    print(f"  macro docs extracted: {len(docs)}")

    with open(output_file, 'w') as f:
        f.write(header_content)

    print(f"Generated {output_file}")

    # Print chunk info for debugging
    for sql_file in sql_files:
        filepath = os.path.join(sql_dir, sql_file)
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                content = f.read()
            chunks = split_content(content)
            if len(chunks) > 1:
                print(f"  {sql_file}: {len(content)} bytes -> {len(chunks)} chunks")

if __name__ == "__main__":
    # Get paths relative to script location
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    sql_dir = project_root / "src" / "sql_macros"
    output_file = project_root / "src" / "include" / "embedded_sql_macros.hpp"

    generate_header(sql_dir, output_file)
