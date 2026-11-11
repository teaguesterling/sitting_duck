#!/usr/bin/env python3
"""Test that the macro manifest in spec/layout/layout.yaml drives the generator.

Run with `python3 -I scripts/test_embed_manifest.py`. Exit 0 means every case
passed; any failure prints its name and exits 1.

WHY THIS EXISTS
---------------
M2 stage 3 moved the macro file list out of scripts/embed_sql_macros.py and
into layout.yaml's `sql_macros` key. Three things can go wrong silently, and
each case below pins one:

  1. The manifest and the tree disagree. A file on disk the manifest does not
     name is never registered; a name with no file is never registered either.
  2. The generator skips a manifest entry whose file is missing. It used to
     (`if os.path.exists(...)`), so the extension lost those macros without a
     word. It must raise, naming the file.
  3. The header's file order differs from the manifest's. Registration order
     is load-bearing: semantic_predicates.sql binds eagerly for five others.

The bad manifest in case 2 is passed as data and written to a temporary
directory. The real layout.yaml is never modified.
"""
import contextlib
import io
import re
import sys
import tempfile
import traceback
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
LAYOUT = ROOT / "spec" / "layout" / "layout.yaml"
SQL_DIR = ROOT / "src" / "sql_macros"

# `python3 -I` leaves the script's own directory off sys.path.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import embed_sql_macros  # noqa: E402

# One file entry in the generated header: `    {"name.sql", R"SQLMACRO(`.
HEADER_ENTRY = re.compile(r'^    \{"([A-Za-z0-9_]+\.sql)", R"SQLMACRO\(', re.M)


def manifest_files():
    with open(LAYOUT, encoding="utf-8") as f:
        layout = yaml.safe_load(f)
    if "sql_macros" not in layout:
        raise AssertionError(f"{LAYOUT} has no `sql_macros` key")
    return [entry["file"] for entry in layout["sql_macros"]]


def test_manifest_matches_tree():
    named = manifest_files()
    on_disk = {p.name for p in SQL_DIR.glob("*.sql")}
    assert len(named) == len(set(named)), "manifest lists a file twice"
    assert set(named) == on_disk, (
        f"only in manifest: {sorted(set(named) - on_disk)}; "
        f"only on disk: {sorted(on_disk - set(named))}")
    assert len(named) == 14, f"expected 14 manifest entries, got {len(named)}"


def test_missing_file_raises():
    bad = manifest_files() + ["no_such_macros.sql"]
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "out.hpp"
        try:
            embed_sql_macros.generate_header(SQL_DIR, out, bad)
        except FileNotFoundError as e:
            assert "no_such_macros.sql" in str(e), f"error does not name the file: {e}"
        else:
            raise AssertionError("a manifest entry with no file was skipped silently")


def test_header_order_equals_manifest_order():
    named = manifest_files()
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / "out.hpp"
        with contextlib.redirect_stdout(io.StringIO()):
            embed_sql_macros.generate_header(SQL_DIR, out, named)
        got = HEADER_ENTRY.findall(out.read_text(encoding="utf-8"))
    assert got == named, f"header order {got} != manifest order {named}"


def _load_manifest_from(text):
    """Run load_manifest against a layout.yaml written from `text`."""
    with tempfile.TemporaryDirectory() as tmp:
        p = Path(tmp) / "layout.yaml"
        p.write_text(text, encoding="utf-8")
        return embed_sql_macros.load_manifest(p)


def _assert_rejected(text, *needles):
    try:
        _load_manifest_from(text)
    except ValueError as e:
        for n in needles:
            assert n in str(e), f"error {e!r} does not name {n!r}"
    else:
        raise AssertionError(f"load_manifest accepted: {text!r}")


def test_rejects_empty_manifest():
    # An empty list used to yield a valid header with zero macros.
    _assert_rejected("sql_macros: []\n", "empty")


def test_rejects_duplicate_file():
    _assert_rejected(
        "sql_macros:\n  - {file: a.sql, side: core}\n  - {file: a.sql, side: addon}\n",
        "a.sql", "sql_macros[1]")


def test_rejects_entry_without_file():
    _assert_rejected("sql_macros:\n  - {file: a.sql, side: core}\n  - {side: core}\n",
                     "file", "sql_macros[1]")


def test_rejects_bad_side():
    # A typo here would make the boundary gate silently skip the file.
    _assert_rejected("sql_macros:\n  - {file: a.sql, side: adon}\n", "adon", "sql_macros[0]")


def test_rejects_missing_side():
    _assert_rejected("sql_macros:\n  - {file: a.sql}\n", "side", "sql_macros[0]")


def test_accepts_valid_manifest():
    got = _load_manifest_from(
        "sql_macros:\n  - {file: a.sql, side: core}\n  - {file: b.sql, side: addon}\n")
    assert got == ["a.sql", "b.sql"], got


def main():
    cases = [test_manifest_matches_tree, test_missing_file_raises,
             test_header_order_equals_manifest_order,
             test_rejects_empty_manifest, test_rejects_duplicate_file,
             test_rejects_entry_without_file, test_rejects_bad_side,
             test_rejects_missing_side, test_accepts_valid_manifest]
    failed = 0
    for case in cases:
        try:
            case()
            print(f"PASS {case.__name__}")
        except Exception:
            failed += 1
            print(f"FAIL {case.__name__}")
            traceback.print_exc()
    print(f"{len(cases) - failed}/{len(cases)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
