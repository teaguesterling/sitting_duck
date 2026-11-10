#!/usr/bin/env python3
"""Static gate for the core/add-on macro boundary (M2 stage 3).

Rule: no macro defined in a file tagged `core` may reference a macro owned by a
file tagged `addon`, directly or through any chain of other macros.

WHY THIS IS STATIC. CREATE MACRO binds eagerly and rejects an unknown function
at LOAD, so a bad reference usually cannot ship. MEASURED exception: when the
body's FROM is `query_table(<parameter>)`, DuckDB defers the whole body to call
time. Every `_from` macro does that, so a core `_from` macro that reaches an
add-on macro loads fine and fails inside a user's query. A test that only loads
the extension cannot see it.

The side tags come from spec/layout/layout.yaml (`sql_macros`), read through
embed_sql_macros.load_manifest so a bad tag is rejected there, not skipped here.

PARSING. Comments AND single-quoted string literals are blanked before any
matching. A comment-only stripper reported css_selectors.sql:1950 calling
ast_dead_code; that is prose inside an error message. `''` is the escape for an
embedded quote. This is a lexer, not a SQL grammar: a macro "references"
another when its name is followed by `(` in the body, which also catches
`x.name(...)` method-call syntax.

Run: python3 -I scripts/verify_macro_boundary.py [--root DIR]
Exit 0 clean, 1 on a violation or on input too thin to judge.
"""
import argparse
import re
import sys
from collections import deque
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import embed_sql_macros  # noqa: E402  (also supplies PyYAML or exits with a hint)

import yaml  # noqa: E402

DEFAULT_ROOT = Path(__file__).resolve().parent.parent
DEF_RE = re.compile(
    r'\bCREATE\s+(?:OR\s+REPLACE\s+)?(?:TEMP(?:ORARY)?\s+)?MACRO\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(',
    re.I)
CALL_RE = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]*)\s*\(')


def strip_comments_and_strings(sql):
    """Blank `--` and `/* */` comments and '...' literals, keeping length and newlines.

    One left-to-right scan, because the three contexts nest: `--` inside a string
    is not a comment and `'` inside a comment does not open a string.
    "..." identifiers pass through untouched but are tracked so an apostrophe in
    one does not open a string.
    """
    out = []
    i, n = 0, len(sql)
    blank = lambda ch: ch if ch == '\n' else ' '
    while i < n:
        c = sql[i]
        if sql.startswith('--', i):
            while i < n and sql[i] != '\n':
                out.append(' ')
                i += 1
        elif sql.startswith('/*', i):
            end = sql.find('*/', i + 2)
            end = n if end < 0 else end + 2
            out.extend(blank(ch) for ch in sql[i:end])
            i = end
        elif c == "'":
            out.append("'")
            i += 1
            while i < n:
                if sql[i] == "'":
                    if i + 1 < n and sql[i + 1] == "'":   # '' escape
                        out.append('  ')
                        i += 2
                        continue
                    break
                out.append(blank(sql[i]))
                i += 1
            if i < n:
                out.append("'")
                i += 1
        elif c == '"':
            j = sql.find('"', i + 1)
            j = n - 1 if j < 0 else j
            out.append(sql[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def parse_macros(sql_dir, manifest):
    """Return ({macro: owning file}, {macro: body text}, [files with no definitions]).

    Raises on a name defined twice.
    """
    owner, body, empty = {}, {}, []
    for entry in manifest:
        fname = entry['file']
        path = Path(sql_dir) / fname
        if not path.is_file():
            raise FileNotFoundError(f"manifest entry {fname} has no file at {path}")
        text = strip_comments_and_strings(path.read_text(encoding='utf-8'))
        defs = list(DEF_RE.finditer(text))
        if not defs:
            empty.append(fname)
        for k, m in enumerate(defs):
            name = m.group(1)
            if name in owner:
                raise ValueError(f"macro {name} is defined in both {owner[name]} and {fname}; "
                                 "the owner is ambiguous, so the boundary cannot be judged")
            end = defs[k + 1].start() if k + 1 < len(defs) else len(text)
            owner[name], body[name] = fname, text[m.end():end]
    return owner, body, empty


def build_edges(owner, body):
    """{caller: {callee}} over defined macros, self-calls excluded."""
    edges = {name: set() for name in owner}
    for name, text in body.items():
        for m in CALL_RE.finditer(text):
            callee = m.group(1)
            if callee in owner and callee != name:
                edges[name].add(callee)
    return edges


def find_violations(edges, owner, side):
    """[(core macro, addon macro, path)] following edges transitively."""
    found = []
    for start in sorted(edges):
        if side[owner[start]] != 'core':
            continue
        parent, queue = {start: None}, deque([start])
        while queue:
            cur = queue.popleft()
            for nxt in sorted(edges[cur]):
                if nxt in parent:
                    continue
                parent[nxt] = cur
                queue.append(nxt)
                if side[owner[nxt]] == 'addon':
                    path, p = [], nxt
                    while p is not None:
                        path.append(p)
                        p = parent[p]
                    found.append((start, nxt, path[::-1]))
    return found


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--root', type=Path, default=DEFAULT_ROOT,
                    help='tree holding spec/layout/layout.yaml and src/sql_macros (default: this repo)')
    root = ap.parse_args(argv).root
    layout_path, sql_dir = root / 'spec' / 'layout' / 'layout.yaml', root / 'src' / 'sql_macros'

    try:
        embed_sql_macros.load_manifest(layout_path)           # validates every entry
        manifest = yaml.safe_load(layout_path.read_text(encoding='utf-8'))['sql_macros']
        on_disk = {p.name for p in sql_dir.glob('*.sql')}
        unlisted = on_disk - {e['file'] for e in manifest}
        if unlisted:
            raise ValueError(f"{sorted(unlisted)} are in {sql_dir} but not in the manifest, "
                             "so the gate would never check them")
        owner, body, empty = parse_macros(sql_dir, manifest)
    except (OSError, ValueError) as e:
        print(f"verify_macro_boundary: FAIL: {e}", file=sys.stderr)
        return 1

    # Non-vacuity: a pattern that matches nothing makes "no violations" meaningless.
    if not owner:
        print("verify_macro_boundary: FAIL: found no macro definitions; "
              "the pattern is wrong, not the code", file=sys.stderr)
        return 1
    # Per file, not only in total: an unbalanced quote swallows the rest of one
    # file and its macros vanish while the other files keep the total above
    # zero. Every manifest file defines at least one macro today.
    if empty:
        print(f"verify_macro_boundary: FAIL: found no macro definitions in {empty}; the "
              "pattern (or an unbalanced quote or comment) is wrong, not the code",
              file=sys.stderr)
        return 1
    edges = build_edges(owner, body)
    n_edges = sum(len(v) for v in edges.values())
    if n_edges == 0:
        print(f"verify_macro_boundary: FAIL: found {len(owner)} macros but no macro-to-macro "
              "edges; the call pattern is wrong, not the code", file=sys.stderr)
        return 1

    side = {e['file']: e['side'] for e in manifest}
    violations = find_violations(edges, owner, side)
    for src, dst, path in violations:
        print(f"VIOLATION: core macro {src} ({owner[src]}) reaches addon macro {dst} "
              f"({owner[dst]}): {' -> '.join(path)}", file=sys.stderr)
    callers = sum(1 for v in edges.values() if v)
    print(f"macro boundary: {len(owner)} macros, {n_edges} macro-to-macro edges "
          f"({callers} macros call another macro), {len(violations)} violations")
    return 1 if violations else 0


if __name__ == '__main__':
    sys.exit(main())
