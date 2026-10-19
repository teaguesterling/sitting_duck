#!/usr/bin/env python3
"""Assert every DEF_TYPE entry closes on its own line.

WHY THIS EXISTS

docs/reference/languages/*.md is generated from src/language_configs/*_types.def
by docs/scripts/generate_language_reference.py. That generator reads one line at
a time and matches DEF_TYPE with a single-line regular expression
(generate_language_reference.py:149).

So a DEF_TYPE wrapped across two lines to stay inside a column limit does not
match, and the entry silently disappears from the generated node-type table. No
error, no warning: the published page simply loses rows.

That happened. PR #280 wrapped the four python comprehension entries and
deleted the whole comprehension table from the reference. A sync check alone
would not have caught it, because regenerating and committing the table-less
page makes the diff clean again. The input has to be checked, not only the
output.

SCOPE, deliberately narrow. This asserts ONE thing: the parenthesis closes on
the same line. It does NOT require the whole generator regex to match, because
47 entries already fail that for legitimate reasons -- escaped-quote raw types
such as DEF_TYPE("\\"", ...) and the separate DEF_TYPE_ANON macro. Requiring
the full regex would fail on pre-existing, correct lines, and a check that
cries wolf gets switched off.
"""
import glob
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PATTERN = "src/language_configs/*_types.def"

# A floor, not a guess: there are 3394 entries today. A glob that matched
# nothing, or a rename that emptied the directory, would otherwise pass this
# check silently -- which is the failure mode it exists to prevent elsewhere.
MIN_ENTRIES = 3000


def offenders(paths):
    total = 0
    bad = []
    for path in paths:
        with open(path, encoding="utf-8", errors="replace") as handle:
            for lineno, line in enumerate(handle, 1):
                if not line.lstrip().startswith("DEF_TYPE"):
                    continue
                total += 1
                if ")" not in line:
                    bad.append((path, lineno, line.strip()[:90]))
    return total, bad


def self_test() -> int:
    """Planted-fault control: prove the detection can fail, not just pass."""
    import tempfile

    good = 'DEF_TYPE("x", A, B, C, D)\n'
    wrapped = 'DEF_TYPE("y", A, B, C,\n         D)\n'
    with tempfile.TemporaryDirectory() as tmp:
        f = Path(tmp) / "planted_types.def"
        f.write_text(good + wrapped, encoding="utf-8")
        total, bad = offenders([f])
    if total != 2:
        print(f"::error::self-test counted {total} DEF_TYPE entries, expected 2")
        return 1
    if len(bad) != 1 or not bad[0][2].startswith('DEF_TYPE("y"'):
        print(f"::error::self-test expected exactly the wrapped entry, got {bad}")
        return 1
    print("self-test OK: plants a wrapped DEF_TYPE and detects exactly it")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return self_test()

    paths = sorted(ROOT.glob(PATTERN))
    if not paths:
        print(f"::error::no files matched {PATTERN}; this check is looking in the wrong place")
        return 1

    total, bad = offenders(paths)

    print(f"checked {total} DEF_TYPE entries across {len(paths)} .def files")

    if total < MIN_ENTRIES:
        print(f"::error::only {total} DEF_TYPE entries found, expected at least {MIN_ENTRIES}; "
              "the scan is wrong, not the sources")
        return 1

    if bad:
        for path, lineno, text in bad:
            rel = Path(path).relative_to(ROOT) if Path(path).is_absolute() else path
            print(f"::error file={rel},line={lineno}::DEF_TYPE is wrapped across lines. "
                  "docs/scripts/generate_language_reference.py matches a single line, so this "
                  f"entry disappears from the generated node-type table: {text}")
        print(f"\n{len(bad)} wrapped DEF_TYPE entr(ies); put each one on a single line")
        return 1

    print("OK: every DEF_TYPE entry closes on its own line")
    return 0


if __name__ == "__main__":
    sys.exit(main())
