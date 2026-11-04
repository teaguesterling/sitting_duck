#!/usr/bin/env python3
"""Layout and registration-order gate for the v2 M2 move (tracker/features/054).

WHY THIS EXISTS
---------------
tracker/features/044-v2-m1-contract.md:22 names a safety net for M2:

    "Safety net for later (M2): the #92 invariance test -- default build
     byte-equivalent, canonical registration order -- must stay green
     throughout."

Measured 2026-11-03: it did not exist. PR #92 is compile-time language
selection, the feature, and its 8 files contain no test. A content grep for
`registration order`, `byte.?equivalen` and `canonical order` across test/,
.github/ and scripts/ returned nothing; a filename grep for `invarian`,
`byte_equiv` and `registration_order` over all tracked files returned nothing.
A gate that nothing asserts cannot "stay green" -- it was never red. This is
that gate.

M2 stage 1 moves every per-language source into `languages/<lang>/`. Three
things must survive that move, and each has a way of breaking silently:

  1. REGISTRATION ORDER. cmake/BuiltinLanguages.cmake says declaration order is
     registration order, and re-sorts a user-supplied subset into it. Reordering
     it changes nothing observable in any existing test.

     `ast_supported_languages()` does NOT return this order -- measured, the
     same 28 languages come back in what looks like hash-map order. So reading
     order from SQL pins a hash order and passes whatever the move did. The
     authority is spec/layout/layout.yaml, compared against the cmake table and
     against the generated X-macro table when a build is present.

  2. THE CONFORMANCE KIT'S DENOMINATOR. conformance_kit.sh scans
     src/language_configs/*_types.def and src/language_adapters/*_adapter.cpp.
     After the move those globs match zero files, and a language whose
     declarations the kit cannot read is reported UNDECL -- not FAIL. The kit
     would go blind and STAY GREEN. This gate asserts floors on the file count,
     the DEF_TYPE entry count and the number of readable tables.

  3. FILE RESOLUTION. Every registered language must resolve to the sources the
     spec says it has, at the paths the spec gives. Stage 1 edits those four
     path templates and nothing else in the spec, so this check is what proves
     the new paths actually resolve.

SCOPE, stated because a gate's blind spots matter more than its checks:

  * Static by default. It reads the cmake table, the spec and the declaration
    tables, so it runs on a bare checkout. The generated-table check is skipped
    when no build is present, and `--require-build` makes that skip an error.
  * It does NOT check the compiled extension. `.duckdb_extension` cannot stay
    byte-identical across a `git mv`, because __FILE__ strings and debug-info
    paths carry the source path, so "byte-equivalent binary" is the wrong
    reading of 044:22 and is not attempted here.
  * It does NOT check runtime behaviour. Whether a registered adapter returns
    the right answer is the conformance kit's job. This gate only guarantees the
    kit can still see its own input.
  * DEF_TYPE only. DEF_TYPE_ANON (#215) carries its own strategies and is
    outside this gate, as it is outside the kit's .def scan.
"""
import argparse
import pathlib
import re
import sys

import yaml

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from audit_leaf_text_gaps import (  # noqa: E402
    iter_macro_arglists,
    preprocess_def,
)

ROOT = pathlib.Path(__file__).resolve().parent.parent
SPEC = ROOT / "spec" / "layout" / "layout.yaml"
BUILD_DIR = ROOT / "build"

# sitting_duck_language(<name> <AdapterClass> <TS|NATIVE> ADAPTER ... PARSERS ...)
DECLARATION = re.compile(r"^\s*sitting_duck_language\(\s*([a-z0-9_]+)\s", re.M)
# SD_BUILTIN_LANGUAGE_TS(python, PythonAdapter)
GENERATED = re.compile(r"^SD_BUILTIN_LANGUAGE_(?:TS|NATIVE)\(\s*([a-z0-9_]+)\s*,", re.M)


def load_spec(path=SPEC):
    return yaml.safe_load(path.read_text(encoding="utf-8"))


def declared_order(spec, root=ROOT):
    """Language names in cmake declaration order, which IS registration order."""
    table = root / spec["paths"]["declaration_table"]
    if not table.exists():
        return None, f"{spec['paths']['declaration_table']} does not exist"
    return DECLARATION.findall(table.read_text(encoding="utf-8", errors="replace")), None


def generated_tables(spec, root=ROOT):
    """Every generated X-macro table present in the build tree, newest first.

    There are several build directories (release, release-cyano-184, ...). A
    stale one carries a stale order, so every one that exists is checked: a
    gate that reads only the newest would pass while a configured build still
    holds the wrong table.
    """
    rel = spec["paths"]["generated_table"]
    found = sorted((root / "build").glob(f"*/{rel}"))
    return found


def language_sources(spec, language):
    """{kind: relative path or None} for one language, honouring `exceptions`."""
    exc = (spec.get("exceptions") or {}).get(language) or {}
    paths = spec["paths"]
    out = {}

    out["adapter"] = paths["adapter"].format(language=language)

    if exc.get("definitions") == "none":
        out["definitions"] = None
    else:
        out["definitions"] = paths["definitions"].format(language=language)

    if exc.get("grammar") == "none":
        out["grammar"] = None
    else:
        grammar = exc.get("grammar") or language
        out["grammar"] = paths["grammar"].format(grammar=grammar)
    return out


def def_glob(template):
    """Turn a path template into the glob the conformance kit uses."""
    return template.format(language="*")


def entry_count(path):
    """DEF_TYPE entries readable in one declaration table. 0 means invisible."""
    try:
        return sum(1 for _ in iter_macro_arglists(preprocess_def(path)))
    except (OSError, UnicodeDecodeError):
        return 0


def measure_denominator(spec, root=ROOT):
    """What the conformance kit's own globs can see, from the spec's paths.

    DEDUPED BY LANGUAGE, because every `<lang>_adapter.cpp` does
    `#include "../language_configs/<lang>_types.def"` and preprocess_def follows
    includes. Summing per FILE over both globs therefore counts each table
    twice: the first draft of this gate reported 7510 entries and 57 tables for
    a tree with 4039 and 31. The kit dedupes the same way (`def_seen`), which is
    why its header says 31 languages and not 57.

    The entry count here is the RAW DEF_TYPE macro-invocation count. The kit's
    own header figure is lower -- 3837 against this tree's 4039 -- because the
    kit counts rows it parsed into four fields and drops the `raw_type`
    placeholder and any arglist shorter than five arguments. Two different
    measurements, both legitimate; the floors in spec/layout/layout.yaml are set
    from THIS one, and must not be copied from the kit's header.
    """
    paths = spec["paths"]
    globs = [def_glob(paths["definitions"]), def_glob(paths["adapter"])]
    files = []
    for pattern in globs:
        files.extend(sorted(root.glob(pattern)))

    languages = list(spec["registration_order"]) + list(spec.get("unregistered_definitions") or [])
    entries = 0
    with_tables = 0
    for language in languages:
        source = language_sources(spec, language)
        # A language declared `definitions: none` keeps its table inline in the
        # adapter translation unit (sql). duckdb has neither, and resolves to an
        # adapter holding zero DEF_TYPE entries -- which is why this counts
        # entries rather than testing for the file's existence.
        rel = source["definitions"] or source["adapter"]
        n = entry_count(root / rel) if (root / rel).exists() else 0
        entries += n
        if n > 0:
            with_tables += 1
    return {"globs": globs, "files": len(files), "entries": entries,
            "languages_with_tables": with_tables}


def check(spec, root=ROOT, require_build=False):
    """Returns (findings, summary_lines). An empty findings list is a pass."""
    findings = []
    summary = []
    expected = list(spec["registration_order"])

    # --- non-vacuity: the spec itself must say something ---
    if not expected:
        return ["spec/layout/layout.yaml lists no registration_order"], []

    # --- 1. the cmake table is the spec's order, exactly and in order ---
    found, err = declared_order(spec, root)
    if err:
        findings.append(err)
    elif not found:
        findings.append(
            f"matched 0 sitting_duck_language() declarations in "
            f"{spec['paths']['declaration_table']}; the pattern is wrong, not the code")
    else:
        if set(found) != set(expected):
            for extra in sorted(set(found) - set(expected)):
                findings.append(
                    f"{spec['paths']['declaration_table']} declares `{extra}`, which "
                    f"spec/layout/layout.yaml does not list under registration_order")
            for missing in sorted(set(expected) - set(found)):
                findings.append(
                    f"spec lists `{missing}` under registration_order, but "
                    f"{spec['paths']['declaration_table']} does not declare it")
        elif found != expected:
            first = next(i for i, (a, b) in enumerate(zip(found, expected)) if a != b)
            findings.append(
                f"REGISTRATION ORDER CHANGED: position {first} is `{found[first]}` in "
                f"{spec['paths']['declaration_table']} and `{expected[first]}` in the spec. "
                f"Declaration order is registration order, and ast_supported_languages() "
                f"does not preserve it, so no other test can see this")
        else:
            summary.append(f"registration order: {len(found)} languages, cmake == spec")

    # --- 2. every generated table in the build tree carries the same order ---
    tables = generated_tables(spec, root)
    if not tables:
        message = (f"no generated table found at build/*/{spec['paths']['generated_table']}; "
                   f"the order check ran against the cmake table only")
        if require_build:
            findings.append(message + " -- --require-build was given")
        else:
            summary.append("generated table: none present, skipped (static run)")
    else:
        for table in tables:
            order = GENERATED.findall(table.read_text(encoding="utf-8", errors="replace"))
            label = table.relative_to(root)
            if not order:
                findings.append(
                    f"matched 0 SD_BUILTIN_LANGUAGE_* entries in {label}; "
                    f"the pattern is wrong, not the code")
                continue
            # A subset build legitimately holds fewer languages. Its RELATIVE
            # order must still follow the spec -- that is the property
            # BuiltinLanguages.cmake claims when it re-sorts a user's list.
            rank = {name: i for i, name in enumerate(expected)}
            unknown = [n for n in order if n not in rank]
            if unknown:
                findings.append(
                    f"{label} registers {unknown[0]}, which the spec does not list")
                continue
            ranks = [rank[n] for n in order]
            if ranks != sorted(ranks):
                bad = next(i for i in range(1, len(ranks)) if ranks[i] < ranks[i - 1])
                findings.append(
                    f"{label} registers `{order[bad]}` after `{order[bad - 1]}`, which "
                    f"reverses the spec's canonical order")
            else:
                kind = "full" if len(order) == len(expected) else f"subset of {len(order)}"
                summary.append(f"generated table {label}: {kind}, order follows spec")

    # --- 3. every language resolves to the sources the spec says it has ---
    missing_files = 0
    for language in expected:
        for kind, rel in language_sources(spec, language).items():
            if rel is None:
                continue
            if not (root / rel).exists():
                missing_files += 1
                findings.append(
                    f"{language}: no {kind} at `{rel}`. Either the file moved and "
                    f"spec/layout/layout.yaml's `paths.{kind}` is stale, or the language "
                    f"lost a source it is declared to have")
    if missing_files == 0:
        summary.append(f"file resolution: all {len(expected)} languages resolve at the spec's paths")

    # --- 3b. the two exact sets, so the denominator below stays honest ---
    paths = spec["paths"]
    on_disk_defs = {p.name[: -len("_types.def")]
                    for p in root.glob(def_glob(paths["definitions"]))}
    declared_defs = {lang for lang in expected
                     if language_sources(spec, lang)["definitions"] is not None}
    listed_extra = set(spec.get("unregistered_definitions") or [])
    for extra in sorted(on_disk_defs - declared_defs - listed_extra):
        findings.append(
            f"`{extra}` has a declaration table but is neither a registered language nor "
            f"listed under `unregistered_definitions` -- the kit scans it and this gate "
            f"cannot say whether that is intended")
    for stale in sorted(listed_extra - on_disk_defs):
        findings.append(
            f"`{stale}` is listed under `unregistered_definitions`, but no declaration "
            f"table for it exists any more -- remove the entry")

    on_disk_adapters = {p.name[: -len("_adapter.cpp")]
                        for p in root.glob(def_glob(paths["adapter"]))}
    listed_not_lang = set(spec.get("not_a_language") or [])
    for extra in sorted(on_disk_adapters - set(expected) - listed_not_lang):
        findings.append(
            f"`{extra}_adapter.cpp` exists and is neither a registered language nor listed "
            f"under `not_a_language`")
    for stale in sorted(listed_not_lang - on_disk_adapters):
        findings.append(
            f"`{stale}` is listed under `not_a_language`, but no adapter for it exists "
            f"any more -- remove the entry")

    # --- 4. the conformance kit can still see its own input ---
    d = measure_denominator(spec, root)
    floors = spec["kit_denominator"]
    for key, label, floor_key in (
            ("files", "declaration-table files", "files_min"),
            ("entries", "DEF_TYPE entries", "def_type_entries_min"),
            ("languages_with_tables", "languages with a readable table", "languages_with_tables_min")):
        floor = floors[floor_key]
        if d[key] < floor:
            findings.append(
                f"THE CONFORMANCE KIT HAS GONE BLIND: {d[key]} {label} via "
                f"{' and '.join(d['globs'])}, below the floor of {floor}. A language whose "
                f"declarations the kit cannot read is reported UNDECL, not FAIL, so the kit "
                f"stays green while measuring nothing")
    if all(d[k] >= floors[f] for k, f in (("files", "files_min"),
                                          ("entries", "def_type_entries_min"),
                                          ("languages_with_tables", "languages_with_tables_min"))):
        summary.append(f"kit denominator: {d['files']} files, {d['entries']} DEF_TYPE entries, "
                       f"{d['languages_with_tables']} readable tables "
                       f"(floors {floors['files_min']}/{floors['def_type_entries_min']}/"
                       f"{floors['languages_with_tables_min']})")

    return findings, summary


def self_test() -> int:
    """Planted-fault control: prove each check can fail, not just pass.

    A gate that cannot be made to fail is indistinguishable from one that is not
    looking. This plants the three shapes that matter -- a reordered
    registration list, a path template that resolves to nothing, and a
    denominator collapsed to zero -- and requires that each is detected.
    """
    spec = load_spec()

    if len(spec["registration_order"]) < 2:
        print("::error::self-test fixture is stale: registration_order has fewer than 2 entries")
        return 1

    # Control 0: the real tree must PASS, or a planted fault proves nothing.
    findings, _ = check(spec)
    if findings:
        print("::error::self-test FAILED: the unmodified tree already has findings, so a "
              "planted fault cannot be attributed. First finding:")
        print(f"::error::  {findings[0]}")
        return 1

    # 1. swap two adjacent languages: order must be reported, membership must not.
    reordered = dict(spec)
    order = list(spec["registration_order"])
    order[0], order[1] = order[1], order[0]
    reordered["registration_order"] = order
    findings, _ = check(reordered)
    order_findings = [f for f in findings if "REGISTRATION ORDER CHANGED" in f]
    if len(order_findings) != 1:
        print(f"::error::self-test FAILED: swapping two languages produced "
              f"{len(order_findings)} order finding(s), expected exactly 1")
        return 1
    if any("does not list under registration_order" in f for f in findings):
        print("::error::self-test FAILED: a reorder was reported as a membership change; "
              "the set check and the order check are not independent")
        return 1

    # 2. drop a language from the spec: membership must be reported, not order.
    dropped = dict(spec)
    dropped["registration_order"] = list(spec["registration_order"])[:-1]
    findings, _ = check(dropped)
    if not any("does not list under registration_order" in f for f in findings):
        print("::error::self-test FAILED: removing a language from the spec was not detected")
        return 1

    # 3. point the paths at nothing: resolution AND the denominator must fire.
    moved = dict(spec)
    moved["paths"] = dict(spec["paths"])
    moved["paths"]["definitions"] = "zyzzyva/{language}_types.def"
    moved["paths"]["adapter"] = "zyzzyva/{language}_adapter.cpp"
    findings, _ = check(moved)
    if not any("GONE BLIND" in f for f in findings):
        print("::error::self-test FAILED: globs matching zero files did not trip the "
              "kit-denominator floor -- this is the exact shape M2 would hit")
        return 1
    if not any("no adapter at" in f for f in findings):
        print("::error::self-test FAILED: a path template resolving to nothing was not "
              "reported per language")
        return 1

    # 4. the floors must be reachable: a floor nothing can satisfy is not a floor.
    impossible = dict(spec)
    impossible["kit_denominator"] = dict(spec["kit_denominator"])
    impossible["kit_denominator"]["def_type_entries_min"] = 10 ** 9
    findings, _ = check(impossible)
    if not any("GONE BLIND" in f for f in findings):
        print("::error::self-test FAILED: an unreachable floor was not reported")
        return 1

    d = measure_denominator(spec)
    print(f"self-test OK: detects a two-language reorder as an ORDER change and not a "
          f"membership change; detects a dropped language as a membership change; detects "
          f"path templates that resolve to nothing, both per language and at the kit "
          f"denominator; and the real tree passes with "
          f"{d['files']} files / {d['entries']} DEF_TYPE entries / "
          f"{d['languages_with_tables']} readable tables")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--self-test", action="store_true",
                        help="run the planted-fault control and exit")
    parser.add_argument("--require-build", action="store_true",
                        help="fail when no generated table is present, instead of skipping "
                             "that check")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    spec = load_spec()
    findings, summary = check(spec, require_build=args.require_build)

    print(f"layout spec {spec['version']}:")
    for line in summary:
        print(f"  {line}")

    if findings:
        print()
        for finding in findings:
            print(f"::error::{finding}")
        print(f"\n{len(findings)} layout invariant violation(s)")
        return 1

    print("OK: the tree agrees with spec/layout/layout.yaml")
    return 0


if __name__ == "__main__":
    sys.exit(main())
