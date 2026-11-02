#!/usr/bin/env python3
r"""Assert two properties of every DEF_TYPE table: one entry per line, no
duplicate raw type.

WHY CHECK 1 EXISTS -- ONE ENTRY PER LINE

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

WHY CHECK 2 EXISTS -- NO DUPLICATE RAW TYPE

Every table feeds an `unordered_map<string, NodeConfig>` initialiser list, and
duplicate keys there are FIRST-WINS: the later entry is a silent no-op, exactly
as with insert(). So a second declaration of a raw type is dead code at best,
and at worst it is the entry somebody last edited while the first one kept
deciding the behaviour.

Both shapes were present when this check was written, and neither was known:

  * src/language_adapters/sql_adapter.cpp declared `op_unary_other` twice with
    CONFLICTING configs -- OPERATOR_ARITHMETIC/NODE_TEXT live, and
    PARSER_CONSTRUCT/NODE_TEXT dead. 264 entries for 263 distinct types.
  * src/language_configs/javascript_types.def declared the nullish-coalescing
    operator twice with IDENTICAL configs: `"?\?"` (escaped against the `??=`
    trigraph) and a plain `"??"` commented "alternate". Both are the raw type
    `??`, so the second never did anything.

An identical-config duplicate fails too. It is dead code, it is indistinguishable
from the conflicting kind until somebody edits one of the two, and a check that
only fires on the worse half of a defect class teaches people the other half is
fine.

WHY BOTH CHECKS SCAN OCCURRENCES, NOT LINE STARTS

An earlier revision of check 1 looked only at lines whose first token was
DEF_TYPE. That is why it could not be pointed at sql_adapter.cpp, whose inline
table had been through clang-format: 265 `DEF_TYPE(` occurrences, only 165 of
them starting a line. Worse, the duplicate above hid in precisely that
difference -- clang-format had split `"op_unary_" "other"` across adjacent
string literals, which C concatenates and a line-anchored grep does not.

So the scan here consumes every balanced `DEF_TYPE(` occurrence wherever it sits
on the line, using the parser in scripts/audit_leaf_text_gaps.py rather than a
second copy of it. sql's table is now fenced with `// clang-format off` and holds
one entry per line, which is what lets check 1 cover it at all.

SCOPE, deliberately narrow. Check 1 asserts ONE thing: the argument list holds
no newline. It does NOT require the whole generator regex to match, because
entries already fail that for legitimate reasons -- escaped-quote raw types such
as DEF_TYPE("\\"", ...) and the separate DEF_TYPE_ANON macro. Requiring the full
regex would fail on pre-existing, correct lines, and a check that cries wolf
gets switched off.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "scripts"))

from audit_leaf_text_gaps import (  # noqa: E402
    DEF_INCLUDE_RE,
    iter_macro_arglists,
    raw_type_from_arg,
    split_top_level_commas,
)

# (glob, minimum entries that glob must yield in total).
#
# The floors are denominators, not guesses. A glob that matched nothing, or a
# rename that emptied a directory, otherwise passes this check in silence --
# which is the failure mode it exists to prevent elsewhere. sql_adapter.cpp
# needs its OWN floor: the .def files alone clear 3000, so sql dropping out of
# the scan would change no number anybody reads.
SOURCES = (
    ("src/language_configs/*_types.def", 3000),
    ("src/language_adapters/sql_adapter.cpp", 200),
)


def line_of(text, offset):
    return text.count("\n", 0, offset) + 1


def scan_file(path, text=None):
    """Entries and rejected arg lists in one file.

    Returns (entries, rejected). An entry is
    (raw_type, lineno, args_without_raw, arglist). `rejected` holds every
    `DEF_TYPE(...)` whose first argument is not a string literal, with its line
    -- the macro's own `#define DEF_TYPE(raw_type, ...)` is the expected member.
    Rejected entries are REPORTED rather than dropped: a parse miss that silently
    discards real entries is how a gate loses its denominator.
    """
    if text is None:
        text = path.read_text(encoding="utf-8", errors="replace")
    entries, rejected = [], []
    for arglist, start in iter_macro_arglists(text, with_offsets=True):
        lineno = line_of(text, start)
        args = split_top_level_commas(arglist)
        raw = raw_type_from_arg(args[0]) if args else None
        if raw is None:
            rejected.append((lineno, " ".join(arglist.split())[:80]))
            continue
        entries.append((raw, lineno, tuple(" ".join(a.split()) for a in args[1:]), arglist))
    return entries, rejected


def iter_translation_unit(path, seen=None):
    """Yield (file, lineno, raw_type, args) in the order the compiler sees them.

    A .def file may pull in another language's table wholesale: typescript
    `#include`s javascript at line 60, near the TOP, so javascript's entries win
    every collision and a colliding typescript entry below is dead (#222). That
    is a per-translation-unit property, so duplicate detection has to follow the
    include rather than read each file alone.

    Line numbers stay anchored to the file that really holds the entry, which is
    why this walks the graph instead of scanning preprocess_def()'s output.
    """
    if seen is None:
        seen = set()
    resolved = path.resolve()
    if resolved in seen:
        return
    seen.add(resolved)

    text = path.read_text(encoding="utf-8", errors="replace")
    # Walk line by line so an #include is expanded at its own position.
    offset = 0
    for line in text.splitlines(keepends=True):
        include = DEF_INCLUDE_RE.match(line)
        if include:
            target = (path.parent / include.group(1)).resolve()
            if target.exists():
                yield from iter_translation_unit(target, seen)
                offset += len(line)
                continue
        chunk_entries, _ = scan_file(path, line)
        for raw, _, args, _ in chunk_entries:
            yield path, line_of(text, offset), raw, args
        offset += len(line)


def multiline_offenders(entries):
    """Entries whose argument list spans more than one line."""
    return [(raw, lineno, " ".join(arglist.split())[:90])
            for raw, lineno, _, arglist in entries if "\n" in arglist]


def duplicate_offenders(root):
    """Raw types declared more than once in one translation unit.

    Returns a list of (raw_type, [(file, lineno, args), ...]) with the LIVE
    declaration first, because first-wins is what decides which one runs.
    """
    occurrences = {}
    order = []
    for path, lineno, raw, args in iter_translation_unit(root):
        if raw not in occurrences:
            occurrences[raw] = []
            order.append(raw)
        occurrences[raw].append((path, lineno, args))
    return [(raw, occurrences[raw]) for raw in order if len(occurrences[raw]) > 1]


def self_test() -> int:
    """Planted-fault control: prove each detection can fail, not just pass.

    A gate whose faults cannot be planted proves nothing, and a planted fault
    that an unrelated code path already catches proves nothing either. Each
    plant below targets one check, and the clean control proves none of them
    fires on a correct table.
    """
    import tempfile

    clean = (
        'DEF_TYPE("a", A, NODE_TEXT, NONE, 0)\n'
        'DEF_TYPE("b", B, NONE, NONE, 0)\n'
        'DEF_TYPE("\\"", C, NODE_TEXT, NONE, 0)\n'   # escaped-quote raw type
        'DEF_TYPE("?\\?", D, NONE, NONE, 0)\n'       # escaped against the trigraph
    )
    wrapped = 'DEF_TYPE("y", A, B, C,\n         D)\n'
    conflicting = 'DEF_TYPE("dup", A, NODE_TEXT, NONE, 0)\nDEF_TYPE("dup", B, NONE, NONE, 0)\n'
    # The shape that actually hid in sql: one spelling split across adjacent
    # string literals, the other not. A per-literal comparison misses this.
    split_literal = 'DEF_TYPE("op_x", A, NODE_TEXT, NONE, 0)\nDEF_TYPE("op_" "x", B, NONE, NONE, 0)\n'
    # Duplicate across an #include boundary, in include order.
    included = 'DEF_TYPE("shared", A, NODE_TEXT, NONE, 0)\n'
    importer = '#include "inc_types.def"\nDEF_TYPE("shared", B, NONE, NONE, 0)\n'

    failures = []
    with tempfile.TemporaryDirectory() as tmp:
        tmpdir = Path(tmp)

        def write(name, body):
            p = tmpdir / name
            p.write_text(body, encoding="utf-8")
            return p

        # Control: the clean table trips neither check.
        p = write("clean_types.def", clean)
        entries, rejected = scan_file(p)
        if len(entries) != 4:
            failures.append(f"clean control: parsed {len(entries)} entries, expected 4")
        if rejected:
            failures.append(f"clean control: rejected {rejected}")
        if multiline_offenders(entries):
            failures.append("clean control: reported a multiline entry")
        if duplicate_offenders(p):
            failures.append(f"clean control: reported a duplicate: {duplicate_offenders(p)}")
        # The two escaped raw types must decode to distinct keys, or a decoder
        # regression would make this control pass by collapsing them.
        raws = sorted(raw for raw, _, _, _ in entries)
        # The escaped-quote entry decodes to the QUOTE character and the
        # trigraph-escaped one to `??`. Listing them explicitly is the point: if
        # a decoder regression collapsed either, this control would otherwise
        # still pass.
        if raws != ['"', "??", "a", "b"]:
            failures.append(
                f"""clean control: decoded raw types {raws}, expected ['"', '??', 'a', 'b']""")

        # Plant 1: a wrapped entry.
        p = write("wrapped_types.def", clean + wrapped)
        entries, _ = scan_file(p)
        bad = multiline_offenders(entries)
        if len(bad) != 1 or bad[0][0] != "y":
            failures.append(f"plant 1 (wrapped): expected exactly 'y', got {bad}")

        # Plant 2: a duplicate with conflicting configs.
        p = write("conflict_types.def", clean + conflicting)
        dups = duplicate_offenders(p)
        if len(dups) != 1 or dups[0][0] != "dup":
            failures.append(f"plant 2 (conflicting dup): expected 'dup', got {dups}")
        elif dups[0][1][0][2] == dups[0][1][1][2]:
            failures.append("plant 2: the two configs compared equal; args are not being read")

        # Plant 3: the same raw type spelled with and without a literal split.
        p = write("split_types.def", clean + split_literal)
        dups = duplicate_offenders(p)
        if len(dups) != 1 or dups[0][0] != "op_x":
            failures.append(f"plant 3 (split literal): expected 'op_x', got {dups}")

        # Plant 4: a duplicate across an #include, which per-file scanning misses.
        write("inc_types.def", included)
        p = write("importer_types.def", importer)
        dups = duplicate_offenders(p)
        if len(dups) != 1 or dups[0][0] != "shared":
            failures.append(f"plant 4 (cross-include dup): expected 'shared', got {dups}")
        else:
            live = dups[0][1][0]
            if live[0].name != "inc_types.def":
                failures.append(
                    f"plant 4: live declaration reported as {live[0].name}, but the "
                    "#include is above the local entry, so the included one wins")

    if failures:
        for line in failures:
            print(f"::error::self-test: {line}")
        return 1
    print("self-test OK: a clean table passes; a wrapped entry, a conflicting "
          "duplicate, a split-literal duplicate and a cross-include duplicate "
          "each fail")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return self_test()

    status = 0
    grand_total = 0

    for pattern, floor in SOURCES:
        paths = sorted(ROOT.glob(pattern))
        if not paths:
            print(f"::error::no files matched {pattern}; this check is looking in "
                  "the wrong place")
            status = 1
            continue

        total = 0
        wrapped = []
        rejected_all = []
        for path in paths:
            entries, rejected = scan_file(path)
            total += len(entries)
            rejected_all += [(path, lineno, text) for lineno, text in rejected]
            wrapped += [(path, lineno, raw, text)
                        for raw, lineno, text in multiline_offenders(entries)]

        grand_total += total
        print(f"checked {total} DEF_TYPE entries across {len(paths)} file(s) "
              f"matching {pattern}")

        # The denominator, stated out loud. Every arg list the parser declined is
        # printed, so a parse miss cannot be mistaken for a clean table.
        if rejected_all:
            print(f"  {len(rejected_all)} arg list(s) not read as entries "
                  "(the macro's own #define is the expected one):")
            for path, lineno, text in rejected_all:
                print(f"    {path.relative_to(ROOT)}:{lineno}  {text}")

        if total < floor:
            print(f"::error::only {total} DEF_TYPE entries matched {pattern}, "
                  f"expected at least {floor}; the scan is wrong, not the sources")
            status = 1

        for path, lineno, raw, text in wrapped:
            rel = path.relative_to(ROOT)
            print(f"::error file={rel},line={lineno}::DEF_TYPE is wrapped across "
                  "lines. docs/scripts/generate_language_reference.py matches a "
                  "single line, so this entry disappears from the generated "
                  f"node-type table: {text}")
        if wrapped:
            print(f"  {len(wrapped)} wrapped DEF_TYPE entr(ies); put each one on "
                  "a single line")
            status = 1

    # Duplicates are a per-translation-unit property, so the roots are scanned
    # whole, following every .def #include.
    print()
    roots = []
    for pattern, _ in SOURCES:
        roots += sorted(ROOT.glob(pattern))
    seen_findings = set()
    dup_count = 0
    for root in roots:
        for raw, occurrences in duplicate_offenders(root):
            key = (raw, tuple((str(p), n) for p, n, _ in occurrences))
            if key in seen_findings:
                continue  # javascript is a unit of its own AND part of typescript
            seen_findings.add(key)
            dup_count += 1
            live_path, live_line, live_args = occurrences[0]
            configs = {args for _, _, args in occurrences}
            kind = ("CONFLICTING configs" if len(configs) > 1
                    else "identical configs, so the later one is pure dead code")
            print(f"::error file={live_path.relative_to(ROOT)},line={live_line}::"
                  f"raw type {raw!r} is declared {len(occurrences)} times in this "
                  f"translation unit ({kind}). The DEF_TYPE table is an "
                  "unordered_map initialiser list, so the FIRST declaration wins "
                  "and every later one is a no-op. Delete the dead entries, or "
                  "merge them into the live one.")
            for i, (path, lineno, args) in enumerate(occurrences):
                tag = "LIVE" if i == 0 else "DEAD"
                print(f"    {tag}  {path.relative_to(ROOT)}:{lineno}  "
                      f"{raw!r} -> {', '.join(args)}")
            status = 1

    print(f"checked {grand_total} DEF_TYPE entries across "
          f"{len(roots)} translation unit root(s) for duplicate raw types")

    if status:
        print(f"\n{dup_count} duplicated raw type(s)" if dup_count else "")
        return status

    print("OK: every DEF_TYPE entry closes on its own line, and no raw type is "
          "declared twice in one translation unit")
    return 0


if __name__ == "__main__":
    sys.exit(main())
