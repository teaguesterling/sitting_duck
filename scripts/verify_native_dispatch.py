#!/usr/bin/env python3
"""Derivation gate for native extractors (#259).

#220 was three silent failures in three languages and NONE produced a compile
error, a link error or a warning: an extractor dispatched on node-type names the
grammar never emits, so every branch was dead and the declared strategy
delivered nothing. The conformance kit caught it; the compiler could not.

This asserts the thing a compiler cannot: every node-type string an extractor
dispatches on must exist in that grammar's symbol table.

The symbol table is the generated parser's own `ts_symbol_names[]`, which is
the authority on what a grammar can emit -- named rules and anonymous tokens
alike. A literal absent from it is a branch that can never be taken.

SCOPE, deliberately narrow. Only `strcmp(<var with "type" in its name>, "lit")`
is considered a type dispatch. Extractors also compare node TEXT ("async",
"public"), and those literals are not node types; including them would produce
false positives and the gate would be ignored, which is worse than not having it.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GRAMMAR_DIR_FOR = {"csharp": "c-sharp"}

# strcmp(<something containing "type">, "literal")
DISPATCH = re.compile(r'strcmp\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*,\s*"([^"]*)"\s*\)')
SYMBOL = re.compile(r'\]\s*=\s*"((?:[^"\\]|\\.)*)"\s*,')


def symbol_table(language: str):
    d = GRAMMAR_DIR_FOR.get(language, language)
    base = ROOT / "generated_parsers" / f"tree-sitter-{d}"
    # Some grammars nest the parser one level down (typescript/, php/), which is
    # why this searches rather than assuming src/parser.c. Skipping them silently
    # is exactly the partial coverage this gate exists to prevent elsewhere.
    parser = base / "src" / "parser.c"
    if not parser.exists():
        candidates = sorted(base.glob(f"{d}/src/parser.c")) or sorted(base.glob("*/src/parser.c"))
        parser = candidates[0] if candidates else None
    if parser is None or not parser.exists():
        return None
    text = parser.read_text(encoding="utf-8", errors="replace")
    start = text.find("ts_symbol_names[] = {")
    if start < 0:
        return None
    end = text.find("};", start)
    body = text[start:end]
    # Unescape only the two sequences a tree-sitter symbol name can contain.
    # `unicode_escape` is wrong here: parser.c carries literals like "\\?" and it
    # raises on them, which would make this gate fail for the wrong reason.
    def unescape(raw: str) -> str:
        return raw.replace('\\"', '"').replace("\\\\", "\\")

    return {unescape(m.group(1)) for m in SYMBOL.finditer(body)}


def self_test() -> int:
    """Planted-fault control: prove the detection can fail, not just pass.

    A gate that cannot be made to fail is indistinguishable from one that is
    not looking. This plants one dispatch the grammar does emit and one it does
    not, and requires exactly the second to be reported.
    """
    symbols = symbol_table("kotlin")
    if not symbols:
        print("::error::self-test could not load the kotlin symbol table")
        return 1

    good = "simple_identifier"          # really in tree-sitter-kotlin
    bad = "function_value_parameter"    # the singular form -- the #220 bug
    if good not in symbols:
        print(f"::error::self-test fixture is stale: {good!r} is no longer in the grammar")
        return 1
    if bad in symbols:
        print(f"::error::self-test fixture is stale: {bad!r} IS in the grammar now")
        return 1

    line = f'if (strcmp(child_type, "{good}") == 0 || strcmp(child_type, "{bad}") == 0) {{'
    hits = [m.group(2) for m in DISPATCH.finditer(line)
            if "type" in m.group(1).lower() and m.group(2) not in symbols]
    if hits != [bad]:
        print(f"::error::self-test FAILED: expected exactly ['{bad}'], got {hits}")
        return 1

    # And the text-comparison carve-out must hold, or the gate floods with noise.
    noise = 'if (strcmp(modifier_text, "async") == 0) {'
    if [m for m in DISPATCH.finditer(noise) if "type" in m.group(1).lower()]:
        print("::error::self-test FAILED: a text comparison was treated as a type dispatch")
        return 1

    print("self-test OK: plants a dead dispatch and detects exactly it; "
          "ignores text comparisons")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return self_test()

    headers = sorted((ROOT / "src" / "include").glob("*_native_extractors.hpp"))
    if not headers:
        print("::error::no *_native_extractors.hpp found; this check is looking in the wrong place")
        return 1

    total_lits = 0
    findings = []
    skipped = []

    for header in headers:
        language = header.name.replace("_native_extractors.hpp", "")
        symbols = symbol_table(language)
        if symbols is None:
            skipped.append(language)
            continue
        if not symbols:
            print(f"::error::{language}: parsed an EMPTY symbol table; the parse is wrong, not the grammar")
            return 1

        seen = {}
        for lineno, line in enumerate(header.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if line.lstrip().startswith("//"):
                continue
            for m in DISPATCH.finditer(line):
                var, lit = m.group(1), m.group(2)
                if "type" not in var.lower():
                    continue
                total_lits += 1
                if lit not in symbols:
                    seen.setdefault((language, lit), lineno)

        for (lang, lit), lineno in sorted(seen.items()):
            findings.append((lang, lit, lineno, header.name))

    print(f"checked {total_lits} type-dispatch literals across "
          f"{len(headers) - len(skipped)} extractors")
    if skipped:
        print(f"no grammar found, skipped: {', '.join(sorted(skipped))}")

    # NON-VACUITY: a regex that matched nothing would pass silently, which is
    # exactly the failure mode this gate exists to catch elsewhere.
    if total_lits == 0:
        print("::error::matched 0 dispatch literals; the pattern is wrong, not the code")
        return 1

    if findings:
        print()
        for lang, lit, lineno, fname in findings:
            print(f"::error file=src/include/{fname},line={lineno}::"
                  f"{lang} dispatches on \"{lit}\", which is not in the "
                  f"tree-sitter-{GRAMMAR_DIR_FOR.get(lang, lang)} symbol table — "
                  f"this branch can never be taken")
        print(f"\n{len(findings)} dead dispatch branch(es)")
        return 1

    print("OK: every type-dispatch literal exists in its grammar")
    return 0


if __name__ == "__main__":
    sys.exit(main())
