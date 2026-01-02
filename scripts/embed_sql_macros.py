#!/usr/bin/env python3
"""
Generate a C++ header file with embedded SQL macros.
This script reads SQL files and creates a header with raw string literals.
Handles MSVC's 16KB string literal limit by splitting large files.
"""

import os
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

def generate_header(sql_dir, output_file):
    """Generate C++ header with embedded SQL macros."""

    sql_files = [
        'semantic_predicates.sql',
        'file_utilities.sql',
        'tree_navigation.sql',
        'pattern_matching.sql',
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

} // namespace duckdb
"""

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
