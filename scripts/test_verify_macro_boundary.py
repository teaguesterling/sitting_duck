#!/usr/bin/env python3
"""Test scripts/verify_macro_boundary.py against temporary fixture trees.

Run with `python3 -I scripts/test_verify_macro_boundary.py`. Exit 0 means every
case passed; any failure prints its name and exits 1. CI runs this as the
"prove the gate can fail" step: cases 2 and 3 plant a real boundary violation
and require the gate to go red.

Each case builds a tree with spec/layout/layout.yaml and src/sql_macros/*.sql
and runs the gate on it with --root. Exit 0 alone proves nothing, so every case
also checks the text the gate printed.
"""
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path

GATE = Path(__file__).resolve().parent / "verify_macro_boundary.py"


def run_gate(files, sides):
    """files: {name: sql}; sides: {name: 'core'|'addon'}. Returns (exit, stdout+stderr)."""
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "spec" / "layout").mkdir(parents=True)
        (root / "src" / "sql_macros").mkdir(parents=True)
        manifest = "".join(f"  - {{file: {n}, side: {sides[n]}}}\n" for n in files)
        (root / "spec" / "layout" / "layout.yaml").write_text("sql_macros:\n" + manifest)
        for name, sql in files.items():
            (root / "src" / "sql_macros" / name).write_text(sql)
        p = subprocess.run([sys.executable, "-I", str(GATE), "--root", str(root)],
                           capture_output=True, text=True)
        return p.returncode, p.stdout + p.stderr


def test_clean_tree_exits_0():
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS x;\n"
                  "CREATE OR REPLACE MACRO core_b(x) AS core_a(x) + 1;\n",
         "a.sql": "CREATE OR REPLACE MACRO addon_a(x) AS core_b(x);\n"},
        {"c.sql": "core", "a.sql": "addon"})   # addon -> core is allowed
    assert code == 0, f"clean tree exited {code}: {out}"
    assert "3 macros" in out and "0 violations" in out, out


def test_direct_core_to_addon_exits_1_naming_both():
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS addon_a(x);\n",
         "a.sql": "CREATE OR REPLACE MACRO addon_a(x) AS x;\n"},
        {"c.sql": "core", "a.sql": "addon"})
    assert code == 1, f"violation exited {code}: {out}"
    assert "core_a" in out and "addon_a" in out, out


def test_transitive_core_to_core_to_addon_exits_1():
    # core_a never names the add-on macro itself; it reaches it through core_b.
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS core_b(x);\n"
                  "CREATE OR REPLACE MACRO core_b(x) AS addon_a(x);\n",
         "a.sql": "CREATE OR REPLACE MACRO addon_a(x) AS x;\n"},
        {"c.sql": "core", "a.sql": "addon"})
    assert code == 1, f"transitive violation exited {code}: {out}"
    assert "core_a -> core_b -> addon_a" in out, out


def test_zero_macros_found_exits_1():
    code, out = run_gate({"c.sql": "-- no macro here\nSELECT 1;\n"}, {"c.sql": "core"})
    assert code == 1, f"empty parse exited {code}: {out}"
    assert "found no macro definitions; the pattern is wrong, not the code" in out, out


def test_comments_and_strings_are_not_references():
    # The measured false positive: a macro name inside an error-message literal
    # (with an escaped quote before it) and inside comments is prose, not a call.
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS core_b(\n"
                  "  error('it''s gone; use addon_a(x) instead')); -- addon_a(1)\n"
                  "/* addon_a(2) */\n"
                  "CREATE OR REPLACE MACRO core_b(x) AS x;\n",
         "a.sql": "CREATE OR REPLACE MACRO addon_a(x) AS x;\n"},
        {"c.sql": "core", "a.sql": "addon"})
    assert code == 0, f"prose was read as a call, exit {code}: {out}"


def main():
    cases = [test_clean_tree_exits_0, test_direct_core_to_addon_exits_1_naming_both,
             test_transitive_core_to_core_to_addon_exits_1, test_zero_macros_found_exits_1,
             test_comments_and_strings_are_not_references]
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
