#!/usr/bin/env python3
"""Test scripts/verify_macro_boundary.py against temporary fixture trees.

Run with `python3 -I scripts/test_verify_macro_boundary.py`. Exit 0 means every
case passed; any failure prints its name and exits 1. CI runs this as the
"prove the gate can fail" step: cases 2 and 3 plant a real boundary violation
and require the gate to go red. The last four are the I3 hardening cases: a
case-variant reference, an add-on declared `CREATE FUNCTION` or `main.<name>`,
a dollar-quoted string, and the docstring's record of what is still not seen.

Each case builds a tree with spec/layout/layout.yaml and src/sql_macros/*.sql
and runs the gate on it with --root. Exit 0 alone proves nothing, so every case
also checks the text the gate printed.
"""
import ast as pyast
import subprocess
import sys
import tempfile
import traceback
from pathlib import Path

GATE = Path(__file__).resolve().parent / "verify_macro_boundary.py"


def run_gate(files, sides, unlisted=None):
    """files: {name: sql}; sides: {name: 'core'|'addon'};
    unlisted: {name: sql} written to disk but left out of the manifest. Returns (exit, stdout+stderr)."""
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        (root / "spec" / "layout").mkdir(parents=True)
        (root / "src" / "sql_macros").mkdir(parents=True)
        manifest = "".join(f"  - {{file: {n}, side: {sides[n]}}}\n" for n in files)
        (root / "spec" / "layout" / "layout.yaml").write_text("sql_macros:\n" + manifest)
        for name, sql in files.items():
            (root / "src" / "sql_macros" / name).write_text(sql)
        for name, sql in (unlisted or {}).items():
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


def test_zero_edges_exits_1():
    # Macros parse, but no call is ever found: the call pattern is suspect.
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS x;\n"
                  "CREATE OR REPLACE MACRO core_b(x) AS x + 1;\n"},
        {"c.sql": "core"})
    assert code == 1, f"zero-edge tree exited {code}: {out}"
    assert "no macro-to-macro edges" in out, out


def test_unlisted_sql_file_exits_1_naming_it():
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS x;\n"
                  "CREATE OR REPLACE MACRO core_b(x) AS core_a(x);\n"},
        {"c.sql": "core"},
        unlisted={"stray.sql": "CREATE OR REPLACE MACRO addon_a(x) AS x;\n"})
    assert code == 1, f"unlisted file exited {code}: {out}"
    assert "stray.sql" in out and "not in the manifest" in out, out


def test_file_yielding_zero_macros_exits_1_naming_it():
    # An unterminated quote swallows the rest of broken.sql, so its macro is
    # never seen. The tree as a whole still has macros and edges, so only a
    # per-file check can catch this.
    code, out = run_gate(
        {"ok.sql": "CREATE OR REPLACE MACRO core_a(x) AS x;\n"
                   "CREATE OR REPLACE MACRO core_b(x) AS core_a(x);\n",
         "broken.sql": "-- it's fine here\nSELECT 'oops;\n"
                       "CREATE OR REPLACE MACRO core_c(x) AS core_a(x);\n"},
        {"ok.sql": "core", "broken.sql": "core"})
    assert code == 1, f"per-file vacuity exited {code}: {out}"
    assert "broken.sql" in out and "no macro definitions" in out, out

def test_case_variant_names_are_the_same_macro():
    # SQL folds an unquoted identifier to lower case, so ADDON_A(x) is a live
    # reference to addon_a, and `Addon_B` defines the same macro a core file
    # calls as addon_b. Both directions matter: the CALL may be shouted, or the
    # DEFINITION may be.
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS ADDON_A(x) + addon_b(x);\n",
         "a.sql": "CREATE OR REPLACE MACRO addon_a(x) AS x;\n"
                  "CREATE OR REPLACE MACRO Addon_B(x) AS x;\n"},
        {"c.sql": "core", "a.sql": "addon"})
    assert code == 1, f"case-variant reference exited {code}: {out}"
    assert "VIOLATION" in out, out
    assert "addon_a" in out and "addon_b" in out, out


def test_addon_defined_as_function_or_qualified_is_owned():
    # An add-on declared `CREATE FUNCTION` or `CREATE MACRO main.<name>` must
    # enter the owner map. The gate only checks calls to names it owns, so an
    # add-on it never owned makes EVERY core call to it invisible. a.sql keeps
    # one plain MACRO so the per-file "no definitions" guard is not what makes
    # this case red: the discriminating assertion is the two names.
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS "
                  "addon_fn(x) + addon_q(x) + addon_plain(x);\n",
         "a.sql": "CREATE OR REPLACE FUNCTION addon_fn(x) AS x;\n"
                  "CREATE OR REPLACE MACRO main.addon_q(x) AS x;\n"
                  "CREATE OR REPLACE MACRO addon_plain(x) AS x;\n"},
        {"c.sql": "core", "a.sql": "addon"})
    assert code == 1, f"alternate definition forms exited {code}: {out}"
    assert "addon_fn" in out and "addon_q" in out, out


def test_dollar_quoted_string_fails_loudly_naming_the_file():
    # The worst hole, because it defeats the gate's own non-vacuity guards. The
    # apostrophe inside a VALID tagged dollar-quoted string flips quote parity
    # for the rest of the file, so core_c vanishes from the owner map and its
    # add-on call with it -- while c.sql still has definitions and an edge, so
    # both vacuity guards stay quiet. The gate must refuse to judge the file
    # rather than analyse a truncated body.
    code, out = run_gate(
        {"c.sql": "CREATE OR REPLACE MACRO core_a(x) AS core_b(x);\n"
                  "CREATE OR REPLACE MACRO core_b(x) AS (SELECT $tag$it's here$tag$);\n"
                  "CREATE OR REPLACE MACRO core_c(x) AS addon_a(x);\n",
         "a.sql": "CREATE OR REPLACE MACRO addon_a(x) AS x;\n"},
        {"c.sql": "core", "a.sql": "addon"})
    assert code != 0, f"dollar-quoted string exited {code}: {out}"
    assert "c.sql" in out and "dollar-quoted" in out, out


def test_docstring_records_the_remaining_holes():
    # Task 3 deferred two known holes with "the docstring could say so", and it
    # did not say so. They stay open by design, so the gate must state that it
    # cannot see them. Read from the DOCSTRING, not from the file text, so a
    # mention in a comment somewhere else cannot satisfy this.
    doc = pyast.get_docstring(pyast.parse(GATE.read_text(encoding="utf-8"))) or ""
    assert '"name"(' in doc, f"quoted-identifier limitation not in the docstring:\n{doc}"
    assert "concatenation" in doc, f"concatenation limitation not in the docstring:\n{doc}"


def main():
    cases = [test_clean_tree_exits_0, test_direct_core_to_addon_exits_1_naming_both,
             test_transitive_core_to_core_to_addon_exits_1, test_zero_macros_found_exits_1,
             test_comments_and_strings_are_not_references,
             test_zero_edges_exits_1, test_unlisted_sql_file_exits_1_naming_it,
             test_file_yielding_zero_macros_exits_1_naming_it,
             test_case_variant_names_are_the_same_macro,
             test_addon_defined_as_function_or_qualified_is_owned,
             test_dollar_quoted_string_fails_loudly_naming_the_file,
             test_docstring_records_the_remaining_holes]
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
