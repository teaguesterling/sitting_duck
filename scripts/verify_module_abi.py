#!/usr/bin/env python3
"""Drift gate for the module ABI (044 M1 deliverable 1).

The ABI's weak half is not the base class. `LanguageAdapter`'s pure virtuals are
enforced by the compiler: miss one and the module does not build. Native context
extraction is declared through TEMPLATE SPECIALIZATION, and a missing
specialization is not an error -- it silently selects the empty default.

#220 was that defect in three languages: kotlin, lua and sql declared
function-shaped native strategies in their `.def` tables, had no
`NativeExtractionTraits` specialization, and returned an empty `NativeContext`.
No compile error, no link error, no warning.

Neither existing gate can see that shape:

  * scripts/verify_native_dispatch.py iterates `*_native_extractors.hpp`. A
    language with NO extractor header is simply absent from its input set, so it
    is never judged. (An anomaly-only probe goes silent when the input set is
    short.)
  * scripts/conformance_kit.sh judges only FUNCTION-SHAPED strategies, so a
    declared FUNCTION_CALL or CUSTOM with no extractor is outside its matrix.

This gate reads spec/abi/module-abi.yaml as the authority and reports the
sources that disagree with it. Five checks:

  1. the dispatcher's switch handles exactly the `dispatched` strategies
  2. `dispatched` + `reserved` equal the NativeExtractionStrategy enum, BOTH
     ways, so a stale spec entry cannot make its own bucket pass
  3. no declaration table names a `reserved` strategy (it would reach
     `default:` and return an empty context)
  4. every language declaring a non-NONE strategy has a traits mapping, or is
     named in `unmapped` with a matching count
  5. DENOMINATOR: every per-language adapter resolves to a declaration table,
     or is named in `no_declaration_source`. Checks 3 and 4 are only as good as
     the set of tables they see, and this gate's first draft scanned
     `src/language_configs/*_types.def` alone -- which silently omitted sql,
     whose 233 entries are inlined in `src/language_adapters/sql_adapter.cpp`,
     and sql is one of the three languages #220 broke.

plus: the hook specialization lists match the header.

SCOPE, stated because a gate's blind spots matter more than its checks:

  * Static. It reads headers and declaration tables, never a built binary, so it
    runs on a bare checkout. It says nothing about whether an extractor that
    EXISTS returns the right answer -- that is the conformance kit's job.
  * `DEF_TYPE` only. `DEF_TYPE_ANON` (#215) carries its own strategies and is
    outside this gate, as it is outside the kit's `.def` scan. Making both
    namedness-aware is one follow-up, not two.
  * Declaration tables are parsed with audit_leaf_text_gaps' helpers, which
    follow `.def` #includes and tolerate several calls per line and C-concatenated
    literals -- the two things clang-format does to sql_adapter.cpp.
"""
import pathlib
import re
import sys

import yaml

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from audit_leaf_text_gaps import (  # noqa: E402
    iter_macro_arglists,
    preprocess_def,
    raw_type_from_arg,
    resolve_config_source,
    split_top_level_commas,
)

ROOT = pathlib.Path(__file__).resolve().parent.parent
SPEC = ROOT / "spec" / "abi" / "module-abi.yaml"
HEADER = ROOT / "src" / "include" / "native_context_extraction.hpp"
NODE_CONFIG = ROOT / "src" / "include" / "node_config.hpp"
DEFS_DIR = ROOT / "src" / "language_configs"

# Where LanguageAdapter subclasses are declared. duckdb_adapter_backup.hpp also
# declares one and is deliberately excluded: the build never compiles it (#226).
ADAPTER_HEADERS = [
    ROOT / "src" / "include" / "language_adapter.hpp",
    ROOT / "src" / "include" / "duckdb_adapter.hpp",
]
# Implements the interface from DATA rather than being one language's module.
NOT_A_LANGUAGE = {"DynamicLanguageAdapter"}

TRAITS = re.compile(r"struct\s+NativeExtractionTraits<\s*([A-Za-z_][A-Za-z0-9_]*)\s*>")
HOOK = re.compile(r"\b(IsBareNameDefinition[A-Za-z]*)<\s*([A-Za-z_][A-Za-z0-9_]*)\s*>")
CASE = re.compile(r"case\s+NativeExtractionStrategy::([A-Z_]+)\s*:")
ADAPTER_CLASS = re.compile(r"class\s+([A-Za-z0-9_]+Adapter)\s*:\s*public\s+LanguageAdapter")


def strategy_enum():
    """The NativeExtractionStrategy members, in declaration order."""
    text = NODE_CONFIG.read_text(encoding="utf-8", errors="replace")
    start = text.find("enum class NativeExtractionStrategy")
    if start < 0:
        return []
    body = text[text.find("{", start) + 1 : text.find("}", start)]
    members = []
    for line in body.splitlines():
        line = line.split("//", 1)[0].strip().rstrip(",")
        # Members may carry an initializer (NONE = 0, CUSTOM = 255). Dropping
        # those would let the spec classify a member the enum does not have, and
        # check 2 would pass without looking at it.
        line = line.split("=", 1)[0].strip()
        if re.fullmatch(r"[A-Z][A-Z0-9_]*", line):
            members.append(line)
    return members


def adapter_classes():
    """Per-language LanguageAdapter subclasses, by header."""
    found = set()
    for header in ADAPTER_HEADERS:
        if not header.exists():
            continue
        found |= set(ADAPTER_CLASS.findall(header.read_text(encoding="utf-8", errors="replace")))
    return found - NOT_A_LANGUAGE


def candidate_languages(adapters):
    """Every language that could declare a table: one per adapter, plus any
    .def with no adapter at all (fsharp, haskell, julia, scala today -- the
    condition yaml was in before #227 wired it up)."""
    languages = {a[: -len("Adapter")].lower() for a in adapters}
    languages |= {p.name[: -len("_types.def")] for p in DEFS_DIR.glob("*_types.def")}
    return languages


def locate(path, node_type):
    """Best-effort file:line for a node type, for the error message only.

    Returns the file name alone when the literal is split across adjacent string
    literals, which is one of the shapes clang-format produces. Guessing a line
    number would be worse than omitting it.
    """
    for lineno, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if "DEF_TYPE" in line and f'"{node_type}"' in line:
            return f"{path.name}:{lineno}"
    return path.name


def entry_count(language):
    """How many DEF_TYPE entries this gate can actually read for a language.

    Zero means invisible, whether the table is missing or merely empty.
    """
    source = resolve_config_source(language)
    if source is None:
        return 0
    return sum(1 for _ in iter_macro_arglists(preprocess_def(source)))


def declarations(languages):
    """{language: {strategy: [(node_type, site)]}} for non-NONE strategies."""
    out = {}
    for language in sorted(languages):
        source = resolve_config_source(language)
        if source is None:
            continue
        for arglist in iter_macro_arglists(preprocess_def(source)):
            args = split_top_level_commas(arglist)
            if len(args) < 5:
                continue
            node_type = raw_type_from_arg(args[0])
            if node_type is None or node_type == "raw_type":
                continue
            native = args[-2].strip()
            if native in ("NONE", "native_strat", "native_extraction"):
                continue
            out.setdefault(language, {}).setdefault(native, []).append((node_type, locate(source, node_type)))
    return out


def adapter_for_language(language, adapters):
    """Map a language name onto its adapter class, by lowercased stem."""
    for adapter in adapters:
        if adapter[: -len("Adapter")].lower() == language:
            return adapter
    return None


def self_test() -> int:
    """Planted-fault control: prove the detection can fail, not just pass.

    A gate that cannot be made to fail is indistinguishable from one that is not
    looking. This plants the two shapes that matter -- a language declaring a
    strategy with no traits mapping, and a per-language adapter whose
    declaration table this gate cannot see -- and requires exactly them.
    """
    adapters = adapter_classes()
    for required in ("PythonAdapter", "SQLAdapter"):
        if required not in adapters:
            print(f"::error::self-test fixture is stale: {required} is no longer declared")
            return 1
    if "DynamicLanguageAdapter" in adapters:
        print("::error::self-test FAILED: DynamicLanguageAdapter is not a per-language module")
        return 1

    # 1. the #220 shape
    mapped = {"PythonAdapter"}
    planted = adapters | {"ZyzzyvaAdapter"}
    declared = {"python": {"FUNCTION_CALL": [("call", "python_types.def:1")]},
                "zyzzyva": {"FUNCTION_CALL": [("call", "zyzzyva_types.def:1")]}}
    missing = sorted(lang for lang in declared
                     if adapter_for_language(lang, planted) not in mapped and lang not in set())
    if missing != ["zyzzyva"]:
        print(f"::error::self-test FAILED: expected exactly ['zyzzyva'], got {missing}")
        return 1
    if [lang for lang in declared
            if adapter_for_language(lang, planted) not in mapped and lang not in {"zyzzyva"}]:
        print("::error::self-test FAILED: an `unmapped` entry did not silence its language")
        return 1

    # 2. the denominator: sql's table is NOT in src/language_configs, and the
    #    resolver must still find it. This is the hole the first draft had.
    sql_source = resolve_config_source("sql")
    if sql_source is None or sql_source.parent.name != "language_adapters":
        print(f"::error::self-test FAILED: sql's declaration table resolved to {sql_source}, "
              f"not an inline adapter table")
        return 1
    sql_declared = declarations({"sql"}).get("sql")
    if not sql_declared:
        print("::error::self-test FAILED: parsed 0 non-NONE declarations for sql; "
              "the inline adapter table is not being read")
        return 1
    if resolve_config_source("zyzzyva") is not None:
        print("::error::self-test FAILED: a language with no table resolved to one")
        return 1

    print(f"self-test OK: plants an unmapped declaring language and detects exactly it; "
          f"an `unmapped` entry silences one language and no more; "
          f"sql's inline table resolves and yields "
          f"{sum(len(v) for v in sql_declared.values())} non-NONE declarations")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return self_test()

    spec = yaml.safe_load(SPEC.read_text(encoding="utf-8"))
    header = HEADER.read_text(encoding="utf-8", errors="replace")

    native = spec["native_extraction"]
    spec_dispatched = set(native["dispatched"])
    spec_reserved = set(native["reserved"])
    spec_modules = set(native["modules"])
    spec_unmapped = {entry["language"]: entry for entry in native.get("unmapped") or []}
    spec_no_table = set(native.get("no_declaration_source") or [])

    findings = []

    enum = strategy_enum()
    traits = set(TRAITS.findall(header))
    adapters = adapter_classes()
    languages = candidate_languages(adapters)
    declared = declarations(languages)

    # --- non-vacuity: every scan must find something, or the pattern is wrong ---
    for what, n in (("NativeExtractionStrategy members", len(enum)),
                    ("NativeExtractionTraits specializations", len(traits)),
                    ("per-language adapter classes", len(adapters)),
                    ("languages declaring a non-NONE strategy", len(declared))):
        if n == 0:
            print(f"::error::matched 0 {what}; the pattern is wrong, not the code")
            return 1

    # --- 1. the dispatcher handles exactly the `dispatched` set ---
    switch = set(CASE.findall(header))
    for extra in sorted(switch - spec_dispatched):
        findings.append(f"{HEADER.name} dispatches {extra}, which the spec does not list under `dispatched`")
    for missing in sorted(spec_dispatched - switch):
        findings.append(f"spec lists {missing} under `dispatched`, but the dispatcher has no case for it")

    # --- 2. the enum and the spec's two buckets are the same set, both ways ---
    for member in enum:
        if member not in spec_dispatched and member not in spec_reserved:
            findings.append(
                f"NativeExtractionStrategy::{member} is in the enum but neither `dispatched` nor "
                f"`reserved`; classify it before a declaration can name it")
    for named in sorted(spec_dispatched | spec_reserved):
        if named not in enum:
            findings.append(
                f"spec names {named}, which is not a NativeExtractionStrategy member -- a stale "
                f"spec entry makes its bucket's check pass without looking at anything")
    for both in sorted(spec_dispatched & spec_reserved):
        findings.append(f"spec lists {both} as BOTH dispatched and reserved")

    # --- 3. no declaration table names a reserved strategy ---
    for language, by_strategy in sorted(declared.items()):
        for strategy, sites in sorted(by_strategy.items()):
            if strategy in spec_reserved:
                node_type, site = sites[0]
                findings.append(
                    f"{site} declares {strategy} for {node_type}, which the dispatcher does not "
                    f"handle -- it reaches `default:` and returns an empty NativeContext")

    # --- 4. the traits mapping matches, and every declaring language is covered ---
    for extra in sorted(traits - spec_modules):
        findings.append(f"{HEADER.name} specializes NativeExtractionTraits<{extra}>, which the spec does not list")
    for missing in sorted(spec_modules - traits):
        findings.append(f"spec lists {missing} under `modules`, but the header has no specialization for it")

    for language, by_strategy in sorted(declared.items()):
        if adapter_for_language(language, adapters) in traits:
            continue
        count = sum(len(sites) for sites in by_strategy.values())
        entry = spec_unmapped.get(language)
        if entry is None:
            site = sorted(by_strategy.items())[0][1][0][1]
            findings.append(
                f"{site} -- {language} declares {count} non-NONE native strategy/strategies and "
                f"has no NativeExtractionTraits mapping, so the extractor is the empty default "
                f"(#220's shape). Give it a mapping, or declare it under "
                f"`native_extraction.unmapped`")
        elif entry.get("declares") != count:
            findings.append(
                f"{language} is listed under `unmapped` with declares: {entry.get('declares')}, "
                f"but {count} non-NONE declarations are present")

    # --- 5. denominator: no per-language adapter may be invisible to checks 3-4 ---
    # A resolved FILE is not a table. duckdb_adapter.cpp exists and holds zero
    # DEF_TYPE entries, so an existence test passed vacuously here -- which is
    # the failure mode this check exists to prevent. Count the entries instead.
    for adapter in sorted(adapters):
        language = adapter[: -len("Adapter")].lower()
        if entry_count(language) > 0 or language in spec_no_table:
            continue
        findings.append(
            f"{adapter} has no declaration table this gate can read, and is not listed under "
            f"`native_extraction.no_declaration_source` -- checks 3 and 4 are silent for it")
    for language in sorted(spec_no_table):
        if entry_count(language) > 0:
            findings.append(
                f"{language} is listed under `no_declaration_source`, but {entry_count(language)} "
                f"DEF_TYPE entries are now readable for it")

    # --- hooks ---
    in_header = {}
    for hook, adapter in HOOK.findall(header):
        in_header.setdefault(hook, set()).add(adapter)
    for hook, body in sorted((spec.get("hooks") or {}).items()):
        listed = set(body.get("specialized_by") or [])
        found = in_header.get(hook, set())
        if not found:
            print(f"::error::no {hook} specializations found in {HEADER.name}; the pattern is wrong")
            return 1
        for extra in sorted(found - listed):
            findings.append(f"{HEADER.name} specializes {hook}<{extra}>, which the spec does not list")
        for missing in sorted(listed - found):
            findings.append(f"spec lists {hook}<{missing}>, but the header has no specialization for it")

    total_declarations = sum(len(sites) for by in declared.values() for sites in by.values())
    print(f"module ABI {spec['version']}: {len(enum)} strategies "
          f"({len(spec_dispatched)} dispatched, {len(spec_reserved)} reserved), "
          f"{len(traits)} traits mappings over {len(adapters)} per-language adapters, "
          f"{total_declarations} non-NONE declarations in {len(declared)} languages, "
          f"{sum(len(v) for v in in_header.values())} hook specializations")
    if spec_unmapped:
        print(f"declared unmapped: {len(spec_unmapped)} languages "
              f"({sum(1 for e in spec_unmapped.values() if e.get('builtin'))} compiled in) "
              f"-- see native_extraction.unmapped")

    if findings:
        print()
        for finding in findings:
            print(f"::error::{finding}")
        print(f"\n{len(findings)} module-ABI drift(s)")
        return 1

    print("OK: the sources agree with spec/abi/module-abi.yaml")
    return 0


if __name__ == "__main__":
    sys.exit(main())
