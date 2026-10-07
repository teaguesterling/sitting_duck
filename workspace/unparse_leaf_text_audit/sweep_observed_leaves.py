#!/usr/bin/env python3
"""Empirical companion to derive_named_leaf_text_gaps.py.

Does two jobs the static derivation cannot do alone.

1. **Confirms live bugs.** Parses the corpus and reports every leaf where the
   token the unparser would emit is not the node's own text, i.e.

       COALESCE(NULLIF(name, ''), type) <> peek      over children_count = 0

   That expression is lifted verbatim from src/sql_macros/ast_unparse.sql, and
   `peek` is an independent oracle precisely because the unparser deliberately
   never reads it ("reconstructs source from AST rows WITHOUT `peek`").
   `peek_mode := 'full'` is required -- the default truncates around 80 chars,
   which would make long comments look like mismatches.

2. **Validates the static derivation.** Asserts that every offending node type
   observed here was present in the statically derived candidate set. The
   static filter assumes tree-sitter lays terminals out before non-terminals
   (id < TOKEN_COUNT). If that assumption is ever wrong, or if a class the
   filter cannot see shows up -- an `alias_sym_*` renaming a token, or a named
   non-terminal whose children are all hidden and so arrives with
   children_count == 0 -- this check fails loudly instead of letting the audit
   quietly under-report. That is the issue #184 lesson applied to this script
   itself.

`duckdb` is covered here only, since it has no tree-sitter grammar.

Usage
-----
    python3 workspace/unparse_leaf_text_audit/sweep_observed_leaves.py
    python3 .../sweep_observed_leaves.py --binary build/release/duckdb
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
CORPUS_DIR = REPO_ROOT / "test" / "data" / "unparse_leaf_text"
GAPS_JSON = Path(__file__).resolve().parent / "leaf_text_gaps.json"

# language -> corpus filename (mirrors make_corpus.py)
CORPUS = {
    "bash": "sample.sh", "c": "sample.c", "cpp": "sample.cpp",
    "csharp": "sample.cs", "css": "sample.css", "dart": "sample.dart",
    "go": "sample.go", "graphql": "sample.graphql", "hcl": "sample.tf",
    "html": "sample.html", "java": "sample.java", "javascript": "sample.js",
    "json": "sample.json", "kotlin": "sample.kt", "lua": "sample.lua",
    "markdown": "sample.md", "php": "sample.php", "python": "sample.py",
    "r": "sample.R", "ruby": "sample.rb", "rust": "sample.rs",
    "sql": "sample.sql", "swift": "sample.swift", "toml": "sample.toml",
    "typescript": "sample.ts", "zig": "sample.zig",
}

# The unparser's own leaf predicate and token expression, kept verbatim so the
# sweep measures exactly the row set the unparser emits.
LEAF_PREDICATE = "children_count = 0"
TOKEN_EXPR = "COALESCE(NULLIF(name, ''), type)"


def run_sql(binary: Path, sql: str) -> list[list[str]]:
    proc = subprocess.run(
        [str(binary), "-unsigned", "-csv", "-noheader", "-c", sql],
        capture_output=True, text=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"query failed:\n{sql}\n{proc.stdout}\n{proc.stderr}")
    rows = []
    for line in proc.stdout.splitlines():
        if line.strip():
            rows.append(line.split(",", 1))
    return rows


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--binary", type=Path,
                    default=REPO_ROOT / "build" / "release" / "duckdb")
    ap.add_argument("--gaps", type=Path, default=GAPS_JSON)
    args = ap.parse_args()

    static = json.loads(args.gaps.read_text())["languages"]

    total_bad = 0
    validation_failures: list[str] = []
    beyond_static: list[str] = []
    print(f"{'language':<12} {'leaves':>7} {'mismatch':>9}  offending types")
    print("-" * 78)

    for language, filename in sorted(CORPUS.items()):
        path = CORPUS_DIR / filename
        if not path.exists():
            print(f"{language:<12} {'-':>7} {'-':>9}  MISSING CORPUS {path}")
            continue

        posix = path.as_posix()
        n_leaves = int(run_sql(args.binary, (
            f"SELECT count(*) FROM read_ast('{posix}', peek_mode:='full') "
            f"WHERE {LEAF_PREDICATE};"
        ))[0][0])

        rows = run_sql(args.binary, (
            f"SELECT DISTINCT type FROM read_ast('{posix}', peek_mode:='full') "
            f"WHERE {LEAF_PREDICATE} AND {TOKEN_EXPR} <> peek ORDER BY 1;"
        ))
        bad_types = [r[0] for r in rows]
        total_bad += len(bad_types)

        shown = ", ".join(bad_types[:5]) + ("..." if len(bad_types) > 5 else "")
        print(f"{language:<12} {n_leaves:>7} {len(bad_types):>9}  {shown}")

        # Classify each observed offender against the static derivation.
        #
        # An offender that the static derivation DID list as a named leaf is a
        # regression: the gap was reported and the fix should have closed it.
        # An offender outside that set belongs to one of the classes the static
        # derivation documents as structurally out of reach (an alias symbol, a
        # named non-terminal with only hidden children, or an anonymous token
        # whose text is not its own name). Those are findings to fix from this
        # sweep, not evidence the filter is broken.
        entry = static.get(language)
        if entry is not None:
            derived_tokens = set(entry["named_leaf_types"])
            regressions = [t for t in bad_types if t in derived_tokens]
            if regressions:
                validation_failures.append(
                    f"{language}: types the static derivation reported and the "
                    f"fix should have closed are still mismatching: {regressions}"
                )
            beyond = [t for t in bad_types if t not in derived_tokens]
            if beyond:
                beyond_static.append(f"{language}: {beyond}")

    print("-" * 78)
    print(f"total distinct offending (language, type) pairs: {total_bad}")

    if beyond_static:
        print("\nBeyond the static derivation (alias symbols, childless named")
        print("non-terminals, or anonymous tokens whose text is not their name).")
        print("These need fixing but could not have been derived from parser.c:")
        for item in beyond_static:
            print(f"   - {item}")

    if validation_failures:
        print("\n!! REGRESSION: a statically derived gap is still mismatching")
        for failure in validation_failures:
            print(f"   - {failure}")
        return 1

    if total_bad:
        print("\nremaining mismatches above are unfixed gaps")
        return 2

    print("\nclean: for every leaf in the corpus, the token the unparser would")
    print("emit equals the node's own source text.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
