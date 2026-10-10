#!/usr/bin/env python3
"""Derive the set of node type strings a grammar lists BOTH as a named rule and
as an anonymous token, and check that each one whose declaration would mislead
the anonymous half has a ``DEF_TYPE_ANON`` entry.

Why this is a script and not a hand-written list
------------------------------------------------
Issue #215 listed 21 instances; the real number of *declared* collisions is 59,
and the subset that is actually harmful is 27. The difference is not sloppiness
in the issue -- it is that "which type strings collide" is a property of each
tree-sitter grammar, it moves whenever a grammar submodule moves, and it cannot
be eyeballed. Same lesson as #184's zero-arity list and 047's leaf-text sweep:
derive it.

The bug being audited
---------------------
``node_configs`` is an ``unordered_map<string, NodeConfig>`` keyed on the raw
type string. A tree-sitter grammar can use one string for two different
symbols -- ruby's ``if`` is both the if-statement rule (``named: true``) and
the bare ``if`` keyword that introduces it (``named: false``). Only one
declaration survives the map, so the keyword leaf is classified as a second
if-statement:

    SELECT count(*) FROM read_ast('x.rb')
    WHERE semantic_type_to_string(semantic_type) LIKE 'FLOW_%'
      AND NOT is_syntax_only(flags);
    -- 34 before #215 (17 constructs + 17 keyword tokens), 17 after

Three things already cover part of the set, and this script must not report
them as gaps:

* the named declaration carries bit 0 (``IS_SYNTAX_ONLY``, for which
  ``IS_KEYWORD`` is an alias), so the token inherits a syntax classification
  and nothing is double counted;
* the named declaration carries a NAME_ROLE or ``IS_SCOPE`` flag, which the
  #208 engine rule in ``PopulateSemanticFieldsTemplated`` strips from an
  unnamed node -- an unnamed node has no children and no fields, so it can
  neither bind a name nor open a scope;
* a ``DEF_TYPE_ANON`` entry already declares the anonymous half (#215).

Anything else is a GAP: the anonymous token inherits a construct's semantic
type with nothing to contradict it.

Namedness comes from ``parser.c``, not from ``node-types.json``
---------------------------------------------------------------
``ts_symbol_metadata[].named`` is the same bit tree-sitter uses at run time,
and every language in ``generated_parsers/MANIFEST`` has a committed
``parser.c``. Four grammars (fsharp, haskell, julia, scala) have no
``node-types.json`` in this tree at all, so reading the JSON would silently
report them as having no collisions. Reusing ``audit_leaf_text_gaps.py``'s
symbol-table parser keeps one implementation of that parse.

Usage
-----
    scripts/audit_anon_token_collisions.py              # report, exit 1 on a gap
    scripts/audit_anon_token_collisions.py --emit       # print the DEF_TYPE_ANON lines
    scripts/audit_anon_token_collisions.py --language ruby
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from audit_leaf_text_gaps import (  # noqa: E402
    PARSERS_DIR,
    RAW_TYPE_RE,
    iter_macro_arglists,
    parse_parser_c,
    preprocess_def,
    read_manifest,
    resolve_config_source,
    split_top_level_commas,
    unescape_c,
)

ANON_TOKEN = "DEF_TYPE_ANON("

# Bit 0 of the flags byte. IS_KEYWORD and IS_KEYWORD_IF_LEAF are aliases for
# IS_SYNTAX_ONLY (src/include/node_config.hpp), so all three spellings mean the
# anonymous token is already classified as syntax.
SYNTAX_FLAGS = ("IS_SYNTAX_ONLY", "IS_KEYWORD")
# Flags the #208 engine rule strips from an unnamed node, and in doing so sets
# IS_SYNTAX_ONLY. A declaration carrying any of these is already handled.
ENGINE_RULE_FLAGS = ("NAME_REFERENCE", "NAME_DECLARATION", "NAME_DEFINITION", "IS_SCOPE")


@dataclass
class Decl:
    semantic: str
    name_strategy: str
    native_strategy: str
    flags: str


@dataclass
class LanguageResult:
    language: str
    collisions: int = 0
    gaps: list[tuple[str, Decl]] = field(default_factory=list)
    declared_anon: list[str] = field(default_factory=list)
    declared_syntax: list[str] = field(default_factory=list)
    engine_rule: list[str] = field(default_factory=list)
    note: str | None = None


def parse_decls(path: Path, token: str) -> dict[str, Decl]:
    """raw_type -> its first declaration, first-wins like the map initialiser."""
    out: dict[str, Decl] = {}
    text = preprocess_def(path)
    for arglist in iter_macro_arglists(text, token):
        args = split_top_level_commas(arglist)
        if len(args) < 5:
            continue
        m = RAW_TYPE_RE.match(args[0])
        if not m:
            continue
        raw = unescape_c(m.group(1))
        out.setdefault(
            raw,
            Decl(args[1].strip(), args[2].strip(), args[3].strip(), args[4].strip()),
        )
    return out


def audit(language: str, parser_rel: str) -> LanguageResult:
    res = LanguageResult(language)
    def_path = resolve_config_source(language)
    if def_path is None:
        res.note = "no DEF_TYPE source found"
        return res
    parser_c = PARSERS_DIR / parser_rel / "src" / "parser.c"
    if not parser_c.is_file():
        res.note = f"no parser.c at {parser_c.relative_to(PARSERS_DIR.parent)}"
        return res

    sym = parse_parser_c(parser_c)
    named_types: set[str] = set()
    anon_types: set[str] = set()
    for key, type_string in sym.names.items():
        if not sym.visible.get(key, False):
            continue
        (named_types if sym.named.get(key, False) else anon_types).add(type_string)
    both = named_types & anon_types

    declared = parse_decls(def_path, "DEF_TYPE(")
    declared_anon = parse_decls(def_path, ANON_TOKEN)

    for raw in sorted(both):
        decl = declared.get(raw)
        if decl is None:
            continue  # not declared at all: inherits PARSER_CONSTRUCT, no claim to contradict
        res.collisions += 1
        if raw in declared_anon:
            res.declared_anon.append(raw)
        elif any(f in decl.flags for f in SYNTAX_FLAGS):
            res.declared_syntax.append(raw)
        elif any(f in decl.flags for f in ENGINE_RULE_FLAGS):
            res.engine_rule.append(raw)
        else:
            res.gaps.append((raw, decl))
    return res


def emit_line(raw: str, decl: Decl) -> str:
    """The DEF_TYPE_ANON entry a gap needs.

    Same semantic type as the named twin -- a bare `if` is still conditional
    flow and `.flow` should still reach it -- plus IS_SYNTAX_ONLY, which is
    what distinguishes the token from the construct for prune('syntax') and the
    class selectors. Name and native strategies are NONE: an unnamed node has
    no children, so every FIND_*/FIRST_CHILD strategy would return "" anyway.
    """
    escaped = raw.replace("\\", "\\\\").replace('"', '\\"')
    return f'DEF_TYPE_ANON("{escaped}", {decl.semantic}, NONE, NONE, ASTNodeFlags::IS_SYNTAX_ONLY)'


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Audit named/anonymous node type collisions (#215).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--language", action="append", help="limit to this language (repeatable)")
    ap.add_argument("--emit", action="store_true", help="print the DEF_TYPE_ANON line each gap needs")
    ap.add_argument("-q", "--quiet", action="store_true", help="only print gaps and the totals")
    args = ap.parse_args()

    manifest = read_manifest()
    wanted = set(args.language) if args.language else None
    results = [audit(lang, rel) for lang, rel in sorted(manifest.items()) if wanted is None or lang in wanted]

    if wanted:
        missing = wanted - set(manifest)
        for lang in sorted(missing):
            print(f"!! {lang}: not in {PARSERS_DIR.name}/MANIFEST", file=sys.stderr)

    total_gaps = total_coll = 0
    for r in results:
        total_coll += r.collisions
        total_gaps += len(r.gaps)
        if r.note:
            if not args.quiet:
                print(f"-- {r.language}: {r.note}")
            continue
        if not r.collisions:
            if not args.quiet:
                print(f"ok {r.language}: no declared collisions")
            continue
        covered = len(r.declared_anon) + len(r.declared_syntax) + len(r.engine_rule)
        status = "GAP" if r.gaps else "ok "
        if r.gaps or not args.quiet:
            print(
                f"{status} {r.language}: {r.collisions} declared collisions, "
                f"{covered} covered ({len(r.declared_anon)} DEF_TYPE_ANON, "
                f"{len(r.declared_syntax)} declared syntax-only, "
                f"{len(r.engine_rule)} by the #208 engine rule), {len(r.gaps)} gaps"
            )
        for raw, decl in r.gaps:
            print(f"      GAP {raw}: inherits {decl.semantic} with flags={decl.flags}")
            if args.emit:
                print(f"          {emit_line(raw, decl)}")

    print(f"\nTOTAL: {total_coll} declared collisions, {total_gaps} gaps across {len(results)} languages")
    if total_gaps:
        print(
            "\nEach gap means an anonymous grammar token is emitted as a construct of\n"
            "its named twin's kind. Add the DEF_TYPE_ANON line (--emit prints it) to\n"
            "the language's .def file, and give the adapter the second include pass\n"
            "(see src/language_adapters/ruby_adapter.cpp).",
            file=sys.stderr,
        )
    return 1 if total_gaps else 0


if __name__ == "__main__":
    sys.exit(main())
