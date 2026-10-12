#!/usr/bin/env python3
"""Find DEF_TYPE entries that declare a raw_type the grammar cannot emit.

Why these cannot just be deleted on sight (#232)
------------------------------------------------
An unreachable declaration is worse than dead code, because it reads as a
decision. #222 is the proof: ``typescript_types.def`` declared ``string`` as
TYPE_PRIMITIVE and had never run, and a dead ``variable_declarator NONE`` line
made the conformance kit report a native-payload "leak" (#221) by comparing the
engine against a declaration the compiler discards. Both cost real
investigation time before anyone suspected the declaration itself.

The static test is "is this raw_type in ``ts_symbol_names[]``?". That looked
like it had a known false-positive class -- #200's
anonymous-token-with-variable-text cases (ruby ``'``, lua ``--[[``) -- but #200
is about an anonymous token's *type* not equalling its *text*: ruby's symbol IS
``"`` and it also matches the text ``'``. The symbol is in the table; only the
text varies. That is orthogonal to a declaration naming a symbol that does not
exist, and the empirical pass below found zero false positives of any kind. So
absence from the symbol table means the grammar has no such symbol, full stop,
and the empirical pass exists to catch THIS script being wrong rather than to
license a deletion.

Two passes, and what each one proves
------------------------------------
**Static** (always): every ``raw_type`` the ``.def`` declares, against
``ts_symbol_names[]`` from the committed ``parser.c``, with tree-sitter's
builtins (``ERROR``, ``MISSING``, ``end``) excluded -- they are real nodes the
extension emits and the symbol table does not list under those names. Absence
here means the grammar *has no such symbol*, which is the strong standard.

**Empirical** (``--binary``): parses every file under ``test/data`` whose
extension maps to the language and collects ``DISTINCT type``. A candidate that
shows up is a FALSE POSITIVE and the static pass is wrong about it -- worth
recording, because each one is a mechanism (an ``alias_sym_*`` rename, an
external-scanner token, the #200 class). A candidate that never shows up is
*consistent with* being unreachable, which is weaker evidence than the symbol
table: a corpus can simply not exercise a construct.

So a deletion rests on the STATIC standard, and the empirical pass exists to
catch the static pass being wrong. Never the other way round.

Usage
-----
    scripts/audit_unreachable_declarations.py
    scripts/audit_unreachable_declarations.py --binary build/release/duckdb
    scripts/audit_unreachable_declarations.py --language swift --binary ...
"""

from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from audit_leaf_text_gaps import (  # noqa: E402
    PARSERS_DIR,
    collect_def_sources,
    raw_type_from_arg,
    iter_macro_arglists,
    parse_parser_c,
    preprocess_def,
    read_manifest,
    resolve_config_source,
    split_top_level_commas,
    unescape_c,
)

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
CORPUS_ROOT = REPO_ROOT / "test" / "data"

# Builtin symbols the extension really does emit and ts_symbol_names[] does not
# list under these names. Without this they appear as phantoms in every
# language at once, which is the tell that they are not phantoms.
BUILTIN_TYPES = {"ERROR", "MISSING", "end"}


def declared(path: pathlib.Path) -> dict[str, str]:
    """raw_type -> semantic_type argument, first-wins over the include chain."""
    out: dict[str, str] = {}
    for arglist in iter_macro_arglists(preprocess_def(path)):
        args = split_top_level_commas(arglist)
        if len(args) < 5:
            continue
        raw = raw_type_from_arg(args[0])
        if raw is None:
            continue
        if raw != "raw_type":
            out.setdefault(raw, args[1].strip())
    return out


def run_sql(binary: pathlib.Path, sql: str) -> list[str]:
    proc = subprocess.run(
        [str(binary), "-unsigned", "-csv", "-noheader", "-c", sql],
        capture_output=True, text=True, cwd=REPO_ROOT,
    )
    if proc.returncode != 0:
        return []
    return [line for line in proc.stdout.splitlines() if line]


def language_extensions(binary: pathlib.Path) -> dict[str, list[str]]:
    out: dict[str, list[str]] = {}
    for line in run_sql(binary, "SELECT language, list_aggregate(extensions, 'string_agg', ' ') "
                                "FROM ast_supported_languages() ORDER BY language;"):
        lang, _, exts = line.partition(",")
        if exts and exts != "NULL":
            out[lang] = exts.split()
    return out


def observed_types(binary: pathlib.Path, language: str, exts: list[str]) -> set[str]:
    """Every node type the corpus actually produces for this language."""
    files = sorted(
        str(p.relative_to(REPO_ROOT)).replace("'", "''")
        for e in exts for p in CORPUS_ROOT.rglob(f"*.{e}")
    )
    if not files:
        return set()
    seen: set[str] = set()
    # Batched rather than one glob, so one unparseable file cannot void the lot.
    for i in range(0, len(files), 40):
        batch = ", ".join(f"'{f}'" for f in files[i:i + 40])
        rows = run_sql(binary, f"SELECT DISTINCT type FROM read_ast([{batch}], "
                               f"'{language}', ignore_errors := true);")
        seen.update(r.strip('"') for r in rows)
    return seen


def main() -> int:
    ap = argparse.ArgumentParser(
        description="Audit DEF_TYPE entries for raw_types the grammar cannot emit (#232).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--language", action="append", help="limit to this language (repeatable)")
    ap.add_argument("--binary", type=pathlib.Path,
                    help="duckdb binary; enables the empirical pass that catches false positives")
    ap.add_argument("-q", "--quiet", action="store_true", help="only languages with candidates")
    args = ap.parse_args()

    manifest = read_manifest()
    wanted = set(args.language) if args.language else None
    exts = language_extensions(args.binary) if args.binary else {}

    # A .def can be SHARED: typescript_types.def #includes javascript_types.def,
    # so one entry is seen by two languages with two different grammars. A
    # raw_type absent from typescript's symbol table may be perfectly reachable
    # in javascript's -- `field_definition` and `html_character_reference` are
    # exactly that -- and deleting it from the shared file to satisfy
    # typescript would BREAK javascript. (I did that once; javascript lost two
    # live declarations and the row count gave it away.)
    #
    # So reachability is decided over every language that SEES the declaration,
    # not per language. An entry is unreachable only if no language sharing it
    # can emit it. The rest are reported separately: nothing is wrong with them,
    # they are just declarations a sibling language needs.
    owners: dict[pathlib.Path, list[str]] = {}
    for lang, rel in sorted(manifest.items()):
        def_path = resolve_config_source(lang)
        parser_c = PARSERS_DIR / rel / "src" / "parser.c"
        if def_path is None or not parser_c.is_file():
            continue
        for src in collect_def_sources(def_path):
            owners.setdefault(src, []).append(lang)
    known_by_lang: dict[str, set[str]] = {}
    for lang, rel in sorted(manifest.items()):
        parser_c = PARSERS_DIR / rel / "src" / "parser.c"
        if parser_c.is_file():
            known_by_lang[lang] = set(parse_parser_c(parser_c).names.values()) | BUILTIN_TYPES

    def reachable_for_a_sibling(lang: str, raw: str) -> str | None:
        """The sibling language that CAN emit `raw`, if any."""
        def_path = resolve_config_source(lang)
        if def_path is None:
            return None
        for src in collect_def_sources(def_path):
            for other in owners.get(src, []):
                if other != lang and raw in known_by_lang.get(other, set()):
                    return other
        return None

    total_decl = total_cand = total_obs = total_shared = 0
    rows: list[tuple[str, int, list[str], list[str]]] = []

    for lang, rel in sorted(manifest.items()):
        if wanted and lang not in wanted:
            continue
        parser_c = PARSERS_DIR / rel / "src" / "parser.c"
        def_path = resolve_config_source(lang)
        if not parser_c.is_file() or def_path is None:
            if not args.quiet:
                print(f"-- {lang}: no parser.c or no DEF_TYPE source")
            continue
        sym = parse_parser_c(parser_c)
        known = set(sym.names.values()) | BUILTIN_TYPES
        d = declared(def_path)
        all_cand = sorted(r for r in d if r not in known)
        shared: list[tuple[str, str]] = []
        cand: list[str] = []
        for r in all_cand:
            sib = reachable_for_a_sibling(lang, r)
            (shared.append((r, sib)) if sib else cand.append(r))
        total_decl += len(d)
        total_cand += len(cand)
        total_shared += len(shared)

        obs: list[str] = []
        if args.binary and cand and lang in exts:
            seen = observed_types(args.binary, lang, exts[lang])
            obs = [c for c in cand if c in seen]
            total_obs += len(obs)
        never = [c for c in cand if c not in obs]
        rows.append((lang, len(d), never, obs, shared))

    hdr = f"{'language':12} {'decls':>6} {'unreachable':>12}"
    if args.binary:
        hdr += f" {'false pos':>10}"
    print(hdr)
    for lang, n, never, obs, shared in sorted(rows, key=lambda r: -len(r[2])):
        if args.quiet and not never and not obs:
            continue
        line = f"{lang:12} {n:>6} {len(never):>12}"
        if args.binary:
            line += f" {len(obs):>10}"
        print(line)
        if never:
            print(f"               cannot be emitted: {', '.join(never)}")
        if obs:
            print(f"               OBSERVED despite absence from the symbol table: "
                  f"{', '.join(obs)}")
        if shared and not args.quiet:
            print("               needed by a sibling that shares the file, so NOT deletable: "
                  + ", ".join(f"{r} (reachable in {sib})" for r, sib in shared))

    print(f"\nTOTAL: {total_cand} unreachable of {total_decl} declarations"
          f"; {total_shared} more are unreachable for one language but needed by a sibling"
          " that shares the .def")
    if args.binary:
        print(f"       {total_obs} observed in test/data (false positives), "
              f"{total_cand - total_obs} never observed")
    else:
        print("       run with --binary to separate false positives from the rest")
    return 1 if (total_cand - total_obs) else 0


if __name__ == "__main__":
    sys.exit(main())
