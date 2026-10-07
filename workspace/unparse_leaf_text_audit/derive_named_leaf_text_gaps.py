#!/usr/bin/env python3
"""Derive the set of named, text-bearing leaf node types per language and
cross-reference it against each language's ``*_types.def`` name strategy.

Why this is a script and not a hand-written list
------------------------------------------------
Tracker 047 ("4a — leaf-text coverage") explicitly requires this list to be
derived from the grammars rather than written by hand, citing issue #184: a
list enumerated from the two *reported* cases missed most of the real ones.
The same trap applies here. "Which node types are text-bearing leaves" is a
property of each tree-sitter grammar, it changes whenever a grammar submodule
moves, and eyeballing a 600-line ``.def`` file for "things that look like
literals" reproduces exactly the #184 failure mode.

The bug being audited
---------------------
``src/sql_macros/ast_unparse.sql`` emits one token per leaf node as::

    COALESCE(NULLIF(l.name, ''), l.type) AS tok   -- over WHERE children_count = 0

So for any leaf whose ``name`` is empty, the unparser emits the node's *type*
where its *text* belongs. For an anonymous token that is harmless, because
tree-sitter names anonymous tokens after their own text (type == text). For a
*named* token it is a round-trip correctness bug:

    comment  ->  the literal string "comment" instead of "# hello there"

``name`` is populated by the name-extraction strategy in the ``.def`` entry.
For a leaf, every ``FIND_*``/``FIRST_CHILD`` strategy necessarily returns ""
(there are no children to search), so the only strategies that can produce
text on a leaf are ``NODE_TEXT`` and ``CUSTOM``. A named leaf therefore needs
``NODE_TEXT`` (or a ``CUSTOM`` handler that is known to cover it).

Derivation (stage A, static, from the committed parsers)
-------------------------------------------------------
Every generated ``parser.c`` carries the full symbol table:

* ``enum ts_symbol_identifiers``  -- symbol spelling -> numeric id
* ``ts_symbol_names[]``           -- symbol spelling -> the node type string
                                    that sitting_duck surfaces as ``type``
* ``ts_symbol_metadata[]``        -- symbol spelling -> ``.visible`` / ``.named``
* ``#define TOKEN_COUNT n``       -- ids ``< n`` are terminals (tokens); tree-sitter
                                    lays terminals out first, non-terminals after

A **named leaf** is therefore a symbol with ``id < TOKEN_COUNT`` that is both
``visible`` and ``named``. ``visible`` excludes hidden/internal tokens (python's
``_newline``, ``_indent``, ``_dedent``, ...), which never surface as nodes;
``named`` excludes the anonymous tokens that need no text strategy.

Two residual classes the token filter cannot see, reported separately rather
than silently dropped:

* ``alias_sym_*`` symbols (visible+named aliases). Their ids sit past
  ``TOKEN_COUNT``, and an alias may rename either a token or a non-terminal;
  parser.c does not say which. Reported as ``alias_candidates``.
* Named *non-terminals* all of whose children are hidden, which still arrive
  with ``children_count == 0``. Not statically derivable here.

Both classes are resolved by the empirical companion script
``sweep_observed_leaves.py``, which also validates stage A by asserting that
every named leaf type actually observed in a parsed corpus is present in the
statically derived set. That cross-check is the #184 lesson applied to this
script itself: if the layout assumption above is ever wrong, the sweep fails
loudly instead of the audit quietly under-reporting.

Usage
-----
    python3 workspace/unparse_leaf_text_audit/derive_named_leaf_text_gaps.py
    python3 .../derive_named_leaf_text_gaps.py --json out.json --language python
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
PARSERS_DIR = REPO_ROOT / "generated_parsers"
MANIFEST = PARSERS_DIR / "MANIFEST"
DEFS_DIR = REPO_ROOT / "src" / "language_configs"
ADAPTERS_DIR = REPO_ROOT / "src" / "language_adapters"


def resolve_config_source(language: str) -> Path | None:
    """Locate the DEF_TYPE table for a language.

    Most languages keep it in ``src/language_configs/<lang>_types.def``, but
    ``sql`` and ``duckdb`` declare their tables inline in the adapter
    translation unit instead. Both spellings use the same DEF_TYPE macro, so
    the same parser reads either.
    """
    def_path = DEFS_DIR / f"{language}_types.def"
    if def_path.exists():
        return def_path
    adapter_path = ADAPTERS_DIR / f"{language}_adapter.cpp"
    if adapter_path.exists():
        return adapter_path
    return None

# Name-extraction strategies that can yield text for a node with no children.
# Every FIND_*/FIRST_CHILD strategy searches children, so on a leaf it returns
# "" and the unparser falls back to emitting the node type. See
# LanguageAdapter::ExtractByStrategy in src/language_adapter.cpp.
TEXT_BEARING_STRATEGIES = {"NODE_TEXT"}
# CUSTOM may or may not produce text for a given leaf; it is a per-language
# C++ handler. Reported as "review" rather than as a gap or as covered.
REVIEW_STRATEGIES = {"CUSTOM"}

# Node types the unparser must NOT be given a text strategy for, or which are
# not real source tokens. "end" is tree-sitter's builtin EOF symbol (it is
# named but never materialises as a node with text).
EXCLUDED_TYPES = {"end"}

ENUM_RE = re.compile(r"^\s*(\w+)\s*=\s*(\d+),\s*$")
TOKEN_COUNT_RE = re.compile(r"^#define TOKEN_COUNT (\d+)\s*$", re.M)
# e.g.   [sym_identifier] = "identifier",
SYMBOL_NAME_RE = re.compile(r'^\s*\[(\w+)\]\s*=\s*"((?:[^"\\]|\\.)*)",\s*$')
# e.g.   [sym_identifier] = {
METADATA_KEY_RE = re.compile(r"^\s*\[(\w+)\]\s*=\s*\{\s*$")
METADATA_FIELD_RE = re.compile(r"^\s*\.(\w+)\s*=\s*(true|false),\s*$")

# DEF_TYPE("raw_type", <semantic>, <name_strategy>, <native_strategy>, <flags>)
#
# Deliberately NOT a line-anchored regex. The DEF_TYPE tables that live inline
# in an adapter .cpp (sql, duckdb) have been through clang-format, which
# cascades the macro calls and leaves many of them mid-line:
#
#     DEF_TYPE("keyword_select", ...) DEF_TYPE("keyword_from", ...)
#
# A line-anchored match silently read only 181 of sql's 282 entries and
# reported the other 101 as "no DEF_TYPE entry" — false gaps for entries that
# already had NODE_TEXT. So the scan below finds every `DEF_TYPE(` occurrence
# and consumes a balanced argument list, wherever it sits on the line.
DEF_TYPE_TOKEN = "DEF_TYPE("


def unescape_c(s: str) -> str:
    """Decode the C string escapes tree-sitter emits in ts_symbol_names."""
    out: list[str] = []
    i = 0
    simple = {"n": "\n", "t": "\t", "r": "\r", "0": "\0", "\\": "\\", '"': '"', "'": "'"}
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            nxt = s[i + 1]
            if nxt in simple:
                out.append(simple[nxt])
                i += 2
                continue
        out.append(s[i])
        i += 1
    return "".join(out)


def split_top_level_commas(arg_text: str) -> list[str]:
    """Split a macro argument list on commas that are not nested or in a string.

    String awareness is load-bearing, not defensive: the raw node type is
    itself a C string literal and node types routinely *are* bracket
    characters -- DEF_TYPE("["), DEF_TYPE("("), DEF_TYPE("[[") and friends.
    Counting those as nesting left depth permanently unbalanced, so the
    remaining commas stopped looking top-level and the entry was silently
    skipped. That made lua's DEF_TYPE("[[") invisible and the fix
    non-idempotent.

    Angle brackets are deliberately NOT treated as nesting: DEF_TYPE arguments
    contain no templates, but they do contain operator node types like
    DEF_TYPE(">") and DEF_TYPE("<="), where a lone bracket would unbalance the
    depth in exactly the same way.
    """
    parts: list[str] = []
    depth = 0
    in_string = False
    current: list[str] = []
    i = 0
    while i < len(arg_text):
        ch = arg_text[i]
        if in_string:
            current.append(ch)
            if ch == "\\" and i + 1 < len(arg_text):
                current.append(arg_text[i + 1])
                i += 2
                continue
            if ch == '"':
                in_string = False
            i += 1
            continue

        if ch == '"':
            in_string = True
            current.append(ch)
            i += 1
            continue
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1

        if ch == "," and depth == 0:
            parts.append("".join(current).strip())
            current = []
        else:
            current.append(ch)
        i += 1
    parts.append("".join(current).strip())
    return parts


@dataclass
class SymbolTable:
    token_count: int
    ids: dict[str, int]
    names: dict[str, str]
    visible: dict[str, bool]
    named: dict[str, bool]


def parse_parser_c(path: Path) -> SymbolTable:
    text = path.read_text(encoding="utf-8", errors="replace")

    m = TOKEN_COUNT_RE.search(text)
    if not m:
        raise ValueError(f"{path}: no TOKEN_COUNT define found")
    token_count = int(m.group(1))

    lines = text.splitlines()

    ids: dict[str, int] = {}
    names: dict[str, str] = {}
    visible: dict[str, bool] = {}
    named: dict[str, bool] = {}

    section: str | None = None
    current_key: str | None = None

    for line in lines:
        if line.startswith("enum ts_symbol_identifiers {"):
            section = "enum"
            continue
        if line.startswith("static const char * const ts_symbol_names[] = {"):
            section = "names"
            continue
        if line.startswith("static const TSSymbolMetadata ts_symbol_metadata[] = {"):
            section = "metadata"
            current_key = None
            continue
        if section and line.startswith("};"):
            section = None
            current_key = None
            continue

        if section == "enum":
            em = ENUM_RE.match(line)
            if em:
                ids[em.group(1)] = int(em.group(2))
        elif section == "names":
            nm = SYMBOL_NAME_RE.match(line)
            if nm:
                names[nm.group(1)] = unescape_c(nm.group(2))
        elif section == "metadata":
            km = METADATA_KEY_RE.match(line)
            if km:
                current_key = km.group(1)
                continue
            fm = METADATA_FIELD_RE.match(line)
            if fm and current_key:
                value = fm.group(2) == "true"
                if fm.group(1) == "visible":
                    visible[current_key] = value
                elif fm.group(1) == "named":
                    named[current_key] = value

    if not ids or not names or not visible:
        raise ValueError(
            f"{path}: symbol table parse came up empty "
            f"(ids={len(ids)} names={len(names)} metadata={len(visible)})"
        )

    return SymbolTable(token_count, ids, names, visible, named)


@dataclass
class LanguageAudit:
    language: str
    parser_rel: str
    def_rel: str
    named_leaf_types: list[str] = field(default_factory=list)
    alias_candidates: list[str] = field(default_factory=list)
    covered: list[str] = field(default_factory=list)
    gaps_wrong_strategy: list[tuple[str, str]] = field(default_factory=list)
    gaps_missing_entry: list[str] = field(default_factory=list)
    review: list[tuple[str, str]] = field(default_factory=list)
    # Alias symbols that also lack a text strategy. Kept apart from the hard
    # gap list because parser.c does not record whether an alias renames a
    # token (a leaf, so affected) or a non-terminal (unaffected).
    alias_uncovered: list[str] = field(default_factory=list)
    parse_note: str | None = None

    @property
    def gap_types(self) -> list[str]:
        return sorted([t for t, _ in self.gaps_wrong_strategy] + self.gaps_missing_entry)


def iter_macro_arglists(text: str, token: str = DEF_TYPE_TOKEN):
    """Yield the raw argument text of every balanced `token(...)` call in text.

    Skips occurrences inside // line comments and /* block comments so that a
    commented-out DEF_TYPE is not read as a live entry.
    """
    i = 0
    n = len(text)
    while i < n:
        # Skip over comments so commented-out entries are not counted.
        if text.startswith("//", i):
            nl = text.find("\n", i)
            i = n if nl == -1 else nl + 1
            continue
        if text.startswith("/*", i):
            end = text.find("*/", i + 2)
            i = n if end == -1 else end + 2
            continue
        if not text.startswith(token, i):
            i += 1
            continue

        j = i + len(token)
        depth = 1
        in_string = False
        start = j
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
        if depth == 0:
            yield text[start:j]
            i = j + 1
        else:
            i = j


# Leading `"raw_type"` of a DEF_TYPE argument list.
RAW_TYPE_RE = re.compile(r'^\s*"((?:[^"\\]|\\.)*)"\s*$')


# A .def file may pull in another language's table wholesale:
# typescript_types.def does `#include "javascript_types.def"`, which is how
# TypeScript inherits every JavaScript node type. Not following that reported
# `identifier`, `true`, `false`, `null` and `comment` as missing from
# TypeScript when they are merely inherited.
DEF_INCLUDE_RE = re.compile(r'^\s*#include\s+"([^"]+\.def)"\s*$', re.M)


def collect_def_sources(path: Path, seen: set[Path] | None = None) -> list[Path]:
    """Return `path` plus every .def file it transitively #includes."""
    if seen is None:
        seen = set()
    resolved = path.resolve()
    if resolved in seen:
        return []
    seen.add(resolved)

    sources = [path]
    text = path.read_text(encoding="utf-8")
    for rel in DEF_INCLUDE_RE.findall(text):
        included = (path.parent / rel).resolve()
        if included.exists():
            sources.extend(collect_def_sources(included, seen))
    return sources


def preprocess_def(path: Path, seen: set[Path] | None = None) -> str:
    """Inline .def #includes to reproduce the text the compiler actually sees.

    Order matters: the DEF_TYPE table is an unordered_map initialiser list, and
    duplicate keys there are first-wins (later entries are no-ops, as with
    insert()). So the strategy in force for a type is the one in its *first*
    occurrence in this preprocessed text, not the last.
    """
    if seen is None:
        seen = set()
    resolved = path.resolve()
    if resolved in seen:
        return ""
    seen.add(resolved)

    out: list[str] = []
    for line in path.read_text(encoding="utf-8").splitlines(keepends=True):
        m = DEF_INCLUDE_RE.match(line)
        if m:
            included = (path.parent / m.group(1)).resolve()
            if included.exists():
                out.append(preprocess_def(included, seen))
                continue
        out.append(line)
    return "".join(out)


def parse_def_strategies(path: Path) -> tuple[dict[str, str], list[Path]]:
    """Map raw node type -> name-extraction strategy from a DEF_TYPE table.

    Works for ``src/language_configs/*_types.def`` (following .def #includes)
    and the inline tables in ``src/language_adapters/{sql,duckdb}_adapter.cpp``.

    Duplicate raw types resolve first-wins, matching the unordered_map
    initialiser list the entries feed. ruby_types.def really does declare
    "super" twice with different strategies, so this is not hypothetical.
    """
    strategies: dict[str, str] = {}
    sources = collect_def_sources(path)
    text = preprocess_def(path)
    for arglist in iter_macro_arglists(text):
        args = split_top_level_commas(arglist)
        if len(args) < 3:
            continue
        m = RAW_TYPE_RE.match(args[0])
        if not m:
            continue
        raw_type = unescape_c(m.group(1))
        # args = [raw_type, semantic_type, name_strategy, native_strategy, flags]
        strategies.setdefault(raw_type, args[2].strip())
    return strategies, sources


def count_def_type_occurrences(sources: list[Path]) -> int:
    """Raw count of `DEF_TYPE(` across all sources, for the parse self-check."""
    return sum(p.read_text(encoding="utf-8").count(DEF_TYPE_TOKEN) for p in sources)


def read_manifest() -> dict[str, str]:
    mapping: dict[str, str] = {}
    for line in MANIFEST.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        lang, _, rel = line.partition(":")
        if rel:
            mapping[lang.strip()] = rel.strip()
    return mapping


def audit_language(language: str, parser_rel: str, def_path: Path) -> LanguageAudit:
    parser_c = PARSERS_DIR / parser_rel / "src" / "parser.c"
    if not parser_c.exists():
        parser_c = PARSERS_DIR / parser_rel / "parser.c"

    audit = LanguageAudit(
        language=language,
        parser_rel=str(parser_c.relative_to(REPO_ROOT)),
        def_rel=str(def_path.relative_to(REPO_ROOT)),
    )

    table = parse_parser_c(parser_c)
    strategies, def_sources = parse_def_strategies(def_path)
    audit.def_rel = ", ".join(str(p.relative_to(REPO_ROOT)) for p in def_sources)

    # Self-check: every DEF_TYPE( in the file must have been read. A silent
    # shortfall here manufactures "no DEF_TYPE entry" gaps for entries that
    # actually exist (this fired for real on sql_adapter.cpp: 181/282).
    # Duplicate raw types legitimately collapse in the dict, so only a
    # shortfall below the distinct-type count is an error.
    raw_occurrences = count_def_type_occurrences(def_sources)
    parsed = len(strategies)
    if parsed < raw_occurrences:
        audit.parse_note = (
            f"read {parsed} distinct DEF_TYPE entries from {raw_occurrences} "
            f"occurrences (duplicates collapse; large gaps mean a parse miss)"
        )

    leaf_types: set[str] = set()
    alias_types: set[str] = set()
    for symbol, sym_id in table.ids.items():
        if not table.visible.get(symbol, False):
            continue
        if not table.named.get(symbol, False):
            continue
        type_name = table.names.get(symbol)
        if type_name is None or type_name in EXCLUDED_TYPES:
            continue
        if sym_id < table.token_count:
            leaf_types.add(type_name)
        elif symbol.startswith("alias_sym_"):
            alias_types.add(type_name)

    audit.named_leaf_types = sorted(leaf_types)
    audit.alias_candidates = sorted(alias_types)

    for type_name in audit.named_leaf_types:
        if type_name not in strategies:
            audit.gaps_missing_entry.append(type_name)
            continue
        strategy = strategies[type_name]
        if strategy in TEXT_BEARING_STRATEGIES:
            audit.covered.append(type_name)
        elif strategy in REVIEW_STRATEGIES:
            audit.review.append((type_name, strategy))
        else:
            audit.gaps_wrong_strategy.append((type_name, strategy))

    for type_name in audit.alias_candidates:
        if strategies.get(type_name) not in TEXT_BEARING_STRATEGIES | REVIEW_STRATEGIES:
            audit.alias_uncovered.append(type_name)

    return audit


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--json", type=Path, help="write machine-readable results here")
    ap.add_argument("--language", action="append", help="limit to these languages")
    ap.add_argument("--quiet", action="store_true", help="summary table only")
    args = ap.parse_args()

    manifest = read_manifest()
    languages = args.language or sorted(manifest)

    audits: list[LanguageAudit] = []
    for language in languages:
        if language not in manifest:
            print(f"!! {language}: not in {MANIFEST.name}", file=sys.stderr)
            return 2
        def_path = resolve_config_source(language)
        if def_path is None:
            print(f"!! {language}: no DEF_TYPE table found in "
                  f"{DEFS_DIR} or {ADAPTERS_DIR}", file=sys.stderr)
            return 2
        audits.append(audit_language(language, manifest[language], def_path))

    if not args.quiet:
        for audit in audits:
            gaps = audit.gap_types
            print(f"\n=== {audit.language} "
                  f"({len(audit.named_leaf_types)} named leaves, {len(gaps)} gaps) ===")
            if audit.parse_note:
                print(f"  note: {audit.parse_note}")
            for type_name, strategy in sorted(audit.gaps_wrong_strategy):
                print(f"  GAP     {type_name:<34} strategy={strategy}")
            for type_name in sorted(audit.gaps_missing_entry):
                print(f"  GAP     {type_name:<34} (no DEF_TYPE entry)")
            for type_name, strategy in sorted(audit.review):
                print(f"  REVIEW  {type_name:<34} strategy={strategy}")
            for type_name in sorted(audit.alias_uncovered):
                print(f"  ALIAS   {type_name:<34} (alias symbol, no text strategy;"
                      f" verify empirically)")

    print("\n" + "=" * 72)
    print(f"{'language':<12} {'leaves':>7} {'covered':>8} {'gaps':>6} "
          f"{'review':>7} {'alias':>6}")
    print("-" * 72)
    total_leaves = total_gaps = total_covered = 0
    for audit in audits:
        gaps = audit.gap_types
        total_leaves += len(audit.named_leaf_types)
        total_gaps += len(gaps)
        total_covered += len(audit.covered)
        print(f"{audit.language:<12} {len(audit.named_leaf_types):>7} "
              f"{len(audit.covered):>8} {len(gaps):>6} "
              f"{len(audit.review):>7} {len(audit.alias_uncovered):>6}")
    print("-" * 72)
    print(f"{'TOTAL':<12} {total_leaves:>7} {total_covered:>8} {total_gaps:>6}")
    print("\nNote: `duckdb` is a native (non-tree-sitter) language and has no")
    print("generated parser, so it is outside this static derivation. Use")
    print("sweep_observed_leaves.py to audit it empirically.")

    if args.json:
        payload = {
            "languages": {
                a.language: {
                    "parser": a.parser_rel,
                    "def": a.def_rel,
                    "named_leaf_types": a.named_leaf_types,
                    "alias_candidates": a.alias_candidates,
                    "alias_uncovered": sorted(a.alias_uncovered),
                    "covered": sorted(a.covered),
                    "gaps_wrong_strategy": dict(sorted(a.gaps_wrong_strategy)),
                    "gaps_missing_entry": sorted(a.gaps_missing_entry),
                    "review": dict(sorted(a.review)),
                }
                for a in audits
            }
        }
        args.json.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n")
        print(f"\nwrote {args.json}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
