#!/usr/bin/env python3
"""Report DEF_TYPE entries that a `.def` file declares but that are DEAD because
an #included file already declared the same raw_type.

The bug being audited (#222)
----------------------------
The DEF_TYPE entries feed an ``unordered_map`` **initialiser list**, where
duplicate keys are first-wins -- later entries are no-ops, exactly as with
``insert()``. ``typescript_types.def`` begins by ``#include``-ing
``javascript_types.def``, so for every raw_type javascript also declares,
**javascript's entry is the one in force** and typescript's is dead.

That was invisible for as long as it existed, because the conformance kit's
``.def`` scan globs ``*_types.def`` and derives the language from each
*filename* without following ``#include``s: it compared the engine against a
declaration the compiler discards. `ast_type_map('typescript')` reported
``LITERAL_STRING`` for ``string`` while the file said ``TYPE_PRIMITIVE`` three
lines of its own, and conformance ``NATIVE-ABST`` reported a payload "leak" on
``variable_declarator`` that was really javascript's ``VARIABLE_WITH_TYPE``
doing its job (see the correction on #221).

What this script does
---------------------
A shadowed entry that is byte-identical to the one shadowing it is harmless
duplication. A shadowed entry that DIFFERS is dead code that also documents
something untrue.

All 79 are now resolved, so ``KNOWN_DIFFERING`` is empty and ANY shadowed and
differing declaration fails. The 56 identical ones were deleted first (#228);
the 23 differing ones went four ways (#222 part 2), and the split is worth
keeping in mind before adding to that baseline again:

* 6 were JavaScript **omissions**, not TypeScript refinements -- a JS ``switch``
  is multiway and a JS ``break`` is a BREAK jump. Promoted into
  ``javascript_types.def``, which fixes both languages. Refinements live in the
  low 2 bits and ``base_type`` masks them, so no selector changed.
* 6 were node types the JavaScript grammar cannot emit at all
  (``method_signature``, ``property_signature``, ``required_parameter``,
  ``predefined_type``, ``type_annotation``, ``type_identifier``). JavaScript's
  entry was unreachable, so deleting it lets TypeScript's richer one -- five
  native strategies TypeScript had been silently not using -- take effect.
* 8 resolved as "JavaScript was already right", including ``variable_declarator``,
  whose TypeScript entry added a MUTABLE refinement that is wrong for ``const``
  while dropping the VARIABLE_WITH_TYPE native strategy that works.
* 3 (``case_clause``, ``default_clause``, ``property_assignment``) were declared
  in BOTH files and exist in NEITHER grammar's symbol table. Deleted from both.

If a new entry has to be allowed temporarily, add it to ``KNOWN_DIFFERING`` with
a reason -- but prefer resolving it, because every one of the 79 turned out to
have a right answer.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from audit_leaf_text_gaps import (  # noqa: E402
    RAW_TYPE_RE,
    iter_macro_arglists,
    split_top_level_commas,
    unescape_c,
)

DEFS_DIR = pathlib.Path(__file__).resolve().parent.parent / "src" / "language_configs"
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+\.def)"', re.M)
FIELDS = ("semantic_type", "name_strategy", "native_strategy", "flags")

# The 23 entries typescript_types.def declares that javascript_types.def
# already declared DIFFERENTLY, as of 2026-10-09. Each is dead. Tracked on #222;
# roughly five of them (variable_declarator, import_specifier, class_body,
# property_assignment, optional_chain) are cases where JAVASCRIPT'S entry is the
# richer one, so a blanket "let the deriving file win" would be a regression.
KNOWN_DIFFERING: set[tuple[str, str]] = set()


def first_decls(path: pathlib.Path, token: str = "DEF_TYPE(") -> dict[str, tuple[str, ...]]:
    """raw_type -> its first declaration in this file alone, first-wins."""
    out: dict[str, tuple[str, ...]] = {}
    for arglist in iter_macro_arglists(path.read_text(errors="replace"), token):
        args = split_top_level_commas(arglist)
        if len(args) < 5:
            continue
        m = RAW_TYPE_RE.match(args[0])
        if not m:
            continue
        raw = unescape_c(m.group(1))
        if raw == "raw_type":
            continue
        out.setdefault(raw, tuple(a.strip() for a in args[1:5]))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Audit shadowed DEF_TYPE declarations (#222).")
    ap.add_argument("--all", action="store_true", help="also list the identical (harmless) duplicates")
    args = ap.parse_args()

    new_differing: list[tuple[str, str, tuple, tuple]] = []
    resolved: list[tuple[str, str]] = []
    total_identical = total_differing = 0
    edges = 0

    for defp in sorted(DEFS_DIR.glob("*_types.def")):
        lang = defp.name[: -len("_types.def")]
        own = first_decls(defp)
        for inc_name in INCLUDE_RE.findall(defp.read_text(errors="replace")):
            inc = (defp.parent / inc_name).resolve()
            if not inc.is_file():
                print(f"!! {defp.name} includes {inc_name}, which does not exist", file=sys.stderr)
                continue
            edges += 1
            base = first_decls(inc)
            # An included file is textually FIRST, so its declarations win.
            shadowed = [r for r in own if r in base]
            ident = [r for r in shadowed if own[r] == base[r]]
            diff = [r for r in shadowed if own[r] != base[r]]
            total_identical += len(ident)
            total_differing += len(diff)
            print(
                f"{lang} includes {inc_name}: {len(shadowed)} of {len(own)} own entries shadowed "
                f"({len(ident)} identical, {len(diff)} differing)"
            )
            for r in sorted(diff):
                key = (lang, r)
                mark = "known" if key in KNOWN_DIFFERING else "NEW"
                print(f"    {mark:5} {r}")
                for f, b, o in zip(FIELDS, base[r], own[r]):
                    if b != o:
                        print(f"            {f:16} in force={b[:46]:46} dead here={o[:46]}")
                if key not in KNOWN_DIFFERING:
                    new_differing.append((lang, r, base[r], own[r]))
            if args.all:
                for r in sorted(ident):
                    print(f"    ident {r}")
            for key in sorted(KNOWN_DIFFERING):
                if key[0] == lang and key[1] not in diff:
                    resolved.append(key)

    print(f"\nTOTAL across {edges} include edge(s): {total_identical} identical, {total_differing} differing")

    rc = 0
    if new_differing:
        print(
            f"\n{len(new_differing)} NEW shadowed-and-differing declaration(s). Each is dead code that\n"
            "also documents something untrue: the included file's entry is what runs.\n"
            "Either delete the entry, or change the one in force -- an entry here cannot\n"
            "override it. See #222.",
            file=sys.stderr,
        )
        rc = 1
    if resolved:
        print(
            f"\n{len(resolved)} entr(ies) in KNOWN_DIFFERING no longer shadowed-and-differing: "
            + ", ".join(f"{a}:{b}" for a, b in resolved)
            + "\nRemove them from KNOWN_DIFFERING in this script so the baseline stays exact.",
            file=sys.stderr,
        )
        rc = rc or 1
    return rc


if __name__ == "__main__":
    sys.exit(main())
