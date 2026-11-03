#!/usr/bin/env python3
"""Apply the leaf-text fixes that scripts/audit_leaf_text_gaps.py reports.

This is the mechanical half of tracker 047 "4a — leaf-text coverage". The
audit script derives *which* named leaves lack a text-producing name
strategy; this script edits the ``.def`` tables to give them one, so the fix
is reproducible from the grammars rather than hand-applied 125 times.

Two kinds of edit, both deliberately minimal:

1. ``gaps_wrong_strategy`` -- the type already has a DEF_TYPE entry whose
   name-extraction column is NONE (or another strategy that cannot work on a
   childless node). Only that one column becomes NODE_TEXT. The semantic type
   and the flags are left exactly as they were.

2. ``gaps_missing_entry`` -- the grammar exposes the type but no DEF_TYPE
   entry configures it. A new entry is appended with semantic type
   ``PARSER_CONSTRUCT``.

Why PARSER_CONSTRUCT and not a "better" semantic type
-----------------------------------------------------
``PopulateSemanticFieldsTemplated`` in src/include/unified_ast_backend_impl.hpp
already falls back to ``PARSER_CONSTRUCT`` with flags 0 for any node type
with no config. Writing that same value explicitly means the new entry changes
*only* name extraction -- the node keeps the exact semantic type and flags it
resolves to today. 4a is a leaf-text correctness fix, not a semantic
reclassification: reclassifying these types would change selector results,
``ast_get_*`` behaviour and taxonomy audits under a commit that claims to fix
text round-tripping. Where cross-language precedent suggests a richer type
(e.g. ``string_content`` is LITERAL_STRING in 7 other languages) that is
reported as a follow-up for the semantic-types owner rather than applied here.

Excluded languages
------------------
* ``sql`` -- no longer excluded. Its remaining 209 gaps are all ``keyword_*``
  and are appended with ``IS_KEYWORD`` (see APPEND_FLAGS). The earlier revision
  of this docstring said 214 deferred keyword gaps out of 219; both counts were
  taken before ``op_unary_other``'s duplicate was resolved and before the
  non-keyword tail landed. Measured now: 350 named leaves, 141 covered,
  209 gaps, every one of them ``keyword_*``.
* ``typescript`` -- typescript_types.def ``#include``s javascript_types.def and
  all of its gaps are inherited ones, so fixing javascript fixes both. Adding
  them to typescript_types.def too would duplicate keys in the merged map.
* ``duckdb`` -- native parser, no tree-sitter grammar, outside the derivation.

When to re-run
--------------
After a grammar bump, once scripts/audit_leaf_text_gaps.py reports new gaps.
Idempotent: a clean tree produces no edits. Always --dry-run first, then
rebuild and run scripts/sweep_leaf_text_observed.py to confirm.

The gap list is recomputed in-process, not read from a committed snapshot.

Usage
-----
    python3 scripts/apply_leaf_text_fixes.py --dry-run
    python3 scripts/apply_leaf_text_fixes.py
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from audit_leaf_text_gaps import (  # noqa: E402
    DEF_TYPE_TOKEN,
    raw_type_from_arg,
    REPO_ROOT,
    audit_language,
    collect_def_sources,
    parse_def_strategies,
    read_manifest,
    resolve_config_source,
    split_top_level_commas,
    unescape_c,
)

# See module docstring for why each is excluded.
EXCLUDED_LANGUAGES = {"typescript"}

# sql's gap list is 219 entries, 214 of them `keyword_*`. The comment/literal/
# operator classes (the ones every other language got fixed for) are taken
# here; the keyword table is deferred to its own change because the existing
# SQL keyword entries carry meaningful semantic types and IS_KEYWORD, and
# adding 214 PARSER_CONSTRUCT rows beside them would make one table two-tier.
# The keyword gaps are real -- verified: `ON DELETE NO ACTION` unparses as
# "keyword_no keyword_action" -- and are reported as outstanding.
SKIP_TYPE_PREFIXES: dict[str, tuple[str, ...]] = {}

# Flags for an APPENDED entry, by language and raw-type prefix. First match
# wins; anything unmatched gets "0".
#
# Why sql's keyword_* entries get IS_KEYWORD and nothing else does
# ----------------------------------------------------------------
# An appended entry is meant to change name extraction ONLY, which is why the
# semantic type is PARSER_CONSTRUCT -- the value
# PopulateSemanticFieldsTemplated already assigns to an unconfigured type. Flags
# work the same way: the fallback assigns 0, so "0" is the no-change value and
# the default here.
#
# sql's keyword_* family is the one deliberate exception. These 209 types ARE
# keywords -- that is what the grammar calls them -- and the 141 sql types
# already configured include keyword entries that all carry IS_KEYWORD. Writing
# 209 siblings with flags 0 beside them would make one table two-tier, and the
# flag is what `WHERE flags & IS_KEYWORD` selects on.
#
# This IS a behaviour change, unlike the semantic type. The `flags` column moves
# from 0 to IS_KEYWORD for 209 sql node types, so the test suite has to be run
# and read, not assumed. The semantic type does not move: PARSER_CONSTRUCT is
# already what those nodes resolve to.
APPEND_FLAGS: dict[str, tuple[tuple[str, str], ...]] = {
    "sql": (("keyword_", "ASTNodeFlags::IS_KEYWORD"),),
}


def flags_for(language: str, raw_type: str) -> str:
    """The flags column for a newly appended entry. "0" unless a rule matches."""
    for prefix, flags in APPEND_FLAGS.get(language, ()):
        if raw_type.startswith(prefix):
            return flags
    return "0"

# Gaps that the static derivation structurally cannot see, found by
# sweep_observed_leaves.py against the corpus and each verified by hand. Three
# distinct classes, all real:
#
#   dart "comment"                 -- a *named non-terminal* whose children are
#                                     all hidden, so it arrives with
#                                     children_count == 0 and behaves as a leaf.
#                                     Its symbol id is >= TOKEN_COUNT, so the
#                                     "id < TOKEN_COUNT" token filter cannot see
#                                     it.
#   lua "[["   (text "--[[")       -- an *anonymous* token whose text is not its
#   ruby "\""  (text "'")             own name. tree-sitter reuses one token
#                                     symbol across grammar alternatives, so
#                                     `[[` also matches `--[[` and the `"`
#                                     symbol also matches `'`. This falsifies
#                                     the usual "anonymous tokens need nothing
#                                     because type == text" assumption: for
#                                     ruby it meant a single-quoted string
#                                     round-tripped with a double quote.
#   kotlin "interpolated_identifier" -- a visible+named alias symbol. Alias ids
#                                     sit past TOKEN_COUNT and parser.c does not
#                                     record whether an alias renames a token or
#                                     a non-terminal.
#
# language -> {raw_type: semantic_type_or_None}. None means "entry already
# exists, only retarget its strategy"; a string means "append a new entry with
# this semantic type".
EMPIRICAL_FIXES: dict[str, dict[str, str | None]] = {
    "dart": {"comment": None},
    "lua": {"[[": "PARSER_CONSTRUCT"},
    "ruby": {'"': None},
    "kotlin": {"interpolated_identifier": "PARSER_CONSTRUCT"},
}

APPEND_HEADER = """
// =============================================================================
// LEAF TEXT COVERAGE (tracker 047 "4a")
// =============================================================================
// Named, text-bearing leaf types that this grammar exposes but that no
// DEF_TYPE entry configured. The unparser emits one token per leaf as
// COALESCE(NULLIF(name, ''), type) (src/sql_macros/ast_unparse.sql), so a
// named leaf with no text-producing name strategy round-trips to its own type
// name instead of its text. On a leaf every FIND_*/FIRST_CHILD strategy
// returns "" -- there are no children to search -- so NODE_TEXT is the only
// strategy that can recover it.
//
// PARSER_CONSTRUCT is what PopulateSemanticFieldsTemplated already assigns to
// an unconfigured type, so the semantic type written here is the one these
// nodes already resolve to. Semantic classification does not move.
//
// The flags column is 0 for the same reason -- the fallback assigns 0 -- EXCEPT
// where the type family genuinely carries a universal flag. sql's keyword_*
// entries carry IS_KEYWORD, which the sql keyword types configured before them
// already carry; that one column does change what `flags` reports for those
// nodes. See APPEND_FLAGS in scripts/apply_leaf_text_fixes.py.
//
// Derived, not hand-written. Regenerate after a grammar bump with:
//   scripts/audit_leaf_text_gaps.py
"""


def find_def_type_spans(text: str):
    """Yield (raw_type, arglist_start, arglist_end) for each DEF_TYPE( call.

    Mirrors iter_macro_arglists but keeps offsets so an argument can be
    spliced in place. Comments are skipped so a commented-out entry is never
    rewritten.
    """
    i, n = 0, len(text)
    while i < n:
        if text.startswith("//", i):
            nl = text.find("\n", i)
            i = n if nl == -1 else nl + 1
            continue
        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            i = n if end == -1 else end + 2
            continue
        if not text.startswith(DEF_TYPE_TOKEN, i):
            i += 1
            continue

        j = start = i + len(DEF_TYPE_TOKEN)
        depth, in_string = 1, False
        while j < n and depth:
            ch = text[j]
            if in_string:
                if ch == "\\":
                    j += 2
                    continue
                if ch == '"':
                    in_string = False
            elif ch == '"':
                in_string = True
            elif ch == "(":
                depth += 1
            elif ch == ")":
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if depth != 0:
            i = j
            continue

        arglist = text[start:j]
        args = split_top_level_commas(arglist)
        # raw_type_from_arg() already unescapes and concatenates adjacent
        # literals, and returns a str or None -- not a regex match. An earlier
        # revision of this function was regex-based and its `yield` survived the
        # move to these helpers, referring to an `m` that no longer exists and
        # calling unescape_c() a second time. Every run died here on its first
        # retarget, which is why the leaf-text tail was never applied.
        raw = raw_type_from_arg(args[0]) if args else None
        if raw:
            yield raw, start, j
        i = j + 1


def top_level_comma_positions(arglist: str) -> list[int]:
    """Indices (within arglist) of commas that are not nested or in a string."""
    positions: list[int] = []
    depth, in_string = 0, False
    i = 0
    while i < len(arglist):
        ch = arglist[i]
        if in_string:
            if ch == "\\":
                i += 2
                continue
            if ch == '"':
                in_string = False
        elif ch == '"':
            in_string = True
        elif ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif ch == "," and depth == 0:
            positions.append(i)
        i += 1
    return positions


def rewrite_strategy(text: str, raw_type: str, new_strategy: str) -> tuple[str, bool]:
    """Set the name-extraction column of `raw_type`'s DEF_TYPE entry.

    Splices only the third argument, so the semantic type, native strategy
    and flags keep their exact original text.
    """
    for found_type, start, end in find_def_type_spans(text):
        if found_type != raw_type:
            continue
        arglist = text[start:end]
        commas = top_level_comma_positions(arglist)
        if len(commas) < 2:
            continue
        # Argument 2 (0-based) sits between the 2nd and 3rd top-level commas.
        seg_start = commas[1] + 1
        seg_end = commas[2] if len(commas) > 2 else len(arglist)
        segment = arglist[seg_start:seg_end]
        lead = len(segment) - len(segment.lstrip())
        abs_start = start + seg_start + lead
        abs_end = start + seg_start + len(segment.rstrip())
        return text[:abs_start] + new_strategy + text[abs_end:], True
    return text, False


def escape_c(raw_type: str) -> str:
    """Escape a node type for embedding in a C string literal.

    Node types really do include quote characters (ruby's `"` delimiter), so
    this is not theoretical.
    """
    return raw_type.replace("\\", "\\\\").replace('"', '\\"')


FENCE_ON = "// clang-format on"


def splice_into_initializer(text: str, block: str, path: Path) -> str:
    """Insert `block` just before the brace closing the node_configs list.

    If the table is fenced with `// clang-format off` / `// clang-format on`,
    insert INSIDE the fence. src/language_adapters/sql_adapter.cpp is fenced
    precisely so clang-format cannot cascade its entries back into a staircase;
    appending after `// clang-format on` would leave every new entry outside the
    protection and the next `make format` would shred them -- which is how seven
    of sql's raw types came to be split across adjacent string literals in the
    first place.
    """
    anchor = text.find("#undef DEF_TYPE")
    if anchor == -1:
        raise ValueError(f"{path}: no '#undef DEF_TYPE' to anchor the insert on")
    close = text.rfind("};", 0, anchor)
    if close == -1:
        raise ValueError(f"{path}: no closing brace for the initialiser before {anchor}")

    fence = text.rfind(FENCE_ON, 0, close)
    # Only treat it as this table's fence if nothing but whitespace separates
    # the two; a fence belonging to some earlier construct must not capture us.
    if fence != -1 and text[fence + len(FENCE_ON):close].strip() == "":
        close = fence

    return text[:close] + block + text[close:]


def main() -> int:
    ap = argparse.ArgumentParser(
        description='Apply the leaf-text fixes reported by scripts/audit_leaf_text_gaps.py: retarget a name-extraction column to NODE_TEXT, or add a missing DEF_TYPE entry as PARSER_CONSTRUCT.',
        epilog='Idempotent; a clean tree produces no edits. Always --dry-run first, then rebuild and run scripts/sweep_leaf_text_observed.py to confirm.',
    )
    ap.add_argument("--dry-run", action="store_true",
                    help="report the edits without writing them")
    args = ap.parse_args()

    # Recompute the gap list here rather than reading a snapshot, so the edits
    # always reflect the grammars currently in the tree.
    manifest = read_manifest()
    data: dict[str, dict] = {}
    for language in sorted(manifest):
        def_path = resolve_config_source(language)
        if def_path is None:
            continue
        audit = audit_language(language, manifest[language], def_path)
        data[language] = {
            "gaps_wrong_strategy": dict(audit.gaps_wrong_strategy),
            "gaps_missing_entry": list(audit.gaps_missing_entry),
        }

    pending: dict[Path, str] = {}
    appended: dict[str, list[str]] = {}
    changed_counts: dict[str, tuple[int, int]] = {}

    for language in sorted(set(data) | set(EMPIRICAL_FIXES)):
        if language in EXCLUDED_LANGUAGES:
            continue
        entry = data.get(language, {"gaps_wrong_strategy": {},
                                    "gaps_missing_entry": []})
        wrong = dict(entry["gaps_wrong_strategy"])
        missing = list(entry["gaps_missing_entry"])

        # Fold in the empirically derived gaps (see EMPIRICAL_FIXES). These are
        # invisible to the static audit, so they would be re-applied on every
        # run; check the current strategy to keep the script idempotent.
        empirical = EMPIRICAL_FIXES.get(language, {})
        if empirical:
            primary_now = resolve_config_source(language)
            assert primary_now is not None, language
            current, _ = parse_def_strategies(primary_now)
            for raw_type, semantic in empirical.items():
                if current.get(raw_type) == "NODE_TEXT":
                    continue  # already fixed
                if semantic is None:
                    wrong.setdefault(raw_type, current.get(raw_type, "NONE"))
                elif raw_type not in missing:
                    missing.append(raw_type)

        # Drop deferred type families (see SKIP_TYPE_PREFIXES).
        skip = SKIP_TYPE_PREFIXES.get(language, ())
        if skip:
            wrong = {t: s for t, s in wrong.items() if not t.startswith(skip)}
            missing = [t for t in missing if not t.startswith(skip)]

        if not wrong and not missing:
            continue

        primary = resolve_config_source(language)
        assert primary is not None, language
        sources = collect_def_sources(primary)

        # 1. Retarget existing entries to NODE_TEXT, in whichever file holds them.
        n_wrong = 0
        for raw_type in sorted(wrong):
            for source in sources:
                text = pending.get(source, source.read_text(encoding="utf-8"))
                new_text, ok = rewrite_strategy(text, raw_type, "NODE_TEXT")
                if ok:
                    pending[source] = new_text
                    n_wrong += 1
                    break
            else:
                print(f"!! {language}: could not locate DEF_TYPE for {raw_type}",
                      file=sys.stderr)
                return 1

        # 2. Append entries for types the grammar exposes but nothing configures.
        if missing:
            lines = [
                f'DEF_TYPE("{escape_c(t)}", PARSER_CONSTRUCT, NODE_TEXT, NONE, '
                f'{flags_for(language, t)})'
                for t in sorted(missing)
            ]
            text = pending.get(primary, primary.read_text(encoding="utf-8"))
            block = APPEND_HEADER + "\n".join(lines) + "\n"
            if primary.suffix == ".def":
                # A .def file is #included *inside* the initialiser list, so
                # end-of-file is inside the braces and appending is safe.
                if not text.endswith("\n"):
                    text += "\n"
                pending[primary] = text + block
            else:
                # An adapter .cpp holds the list inline, so end-of-file is way
                # outside the braces. Splice in just before the `};` that
                # closes it -- i.e. the last `};` before `#undef DEF_TYPE`.
                pending[primary] = splice_into_initializer(text, block, primary)
            appended[language] = lines

        changed_counts[language] = (n_wrong, len(missing))

    for language, (n_wrong, n_missing) in sorted(changed_counts.items()):
        print(f"{language:<12} strategy->NODE_TEXT: {n_wrong:<3} new entries: {n_missing}")
    total_w = sum(w for w, _ in changed_counts.values())
    total_m = sum(m for _, m in changed_counts.values())
    print(f"\n{len(changed_counts)} languages, "
          f"{total_w} strategy changes, {total_m} new entries, "
          f"{total_w + total_m} gaps closed")
    print(f"files touched: {len(pending)}")

    if args.dry_run:
        print("\n(dry run, nothing written)")
        return 0

    for path, text in pending.items():
        path.write_text(text, encoding="utf-8")
    print("\nwritten")
    return 0


if __name__ == "__main__":
    sys.exit(main())
