# 065 — the declaration-table resolver knows two conventions and there are three

*(Filename kept. The original title — "`tsx` is the only built-in language with
no generated reference page" — was measured false on 2026-11-17. See §5.)*

**Status:** Not fixed. Filed as **#347** on 2026-11-17, with a CORRECTED
headline. The claim this file was written around does not survive
re-measurement; §5 records what was wrong and what the real defect is.
**Found by:** feature 058 (the JSX node type declarations), 2026-11-16, while
regenerating `docs/reference/languages/` after adding seven entries.
**Related:** #329 added the `tsx` language. `tracker/bugs/063` is the other
loose end from that pull request.

## What is wrong

`docs/scripts/generate_language_reference.py` writes one page per
`languages/<lang>/*_types.def` table. `tsx` owns no table: `languages/tsx/`
holds only `module.cmake` and `tsx_adapter.cpp`, and `TSXAdapter` takes
TypeScript's node configs at run time.

So a reader looking up tsx in the reference finds nothing.

## The measurement

Measured at `4fb1e21`:

    $ ls docs/reference/languages/ | wc -l
    31                      # 30 language pages plus index.md
    $ ls docs/reference/languages/tsx.md
    ls: cannot access 'docs/reference/languages/tsx.md': No such file or directory
    $ grep -c jsx docs/reference/languages/typescript.md
    0

The seven new JSX entries appear on `javascript.md` alone, although all three
of javascript, typescript and tsx declare them at run time
(`ast_type_map()` reports 8 jsx rows for each).

`scripts/audit_unreachable_declarations.py` has the same blind spot, from the
same cause:

    tsx               0            0

Zero declarations, because the auditor also reads the `.def` files rather than
the runtime. That row is not a result about tsx.

## Who this hurts

Anyone reading the published reference for tsx. It also means a future JSX
change shows up in one generated page while three languages change.

## Candidate fixes

  1. Have the generator follow the `#include` chain and the adapter's config
     source, and emit a page per REGISTERED language rather than per table.
     This is the fix that also repairs the auditor's `tsx 0 0`.
  2. Emit a short `tsx.md` that states it inherits typescript's table and links
     to it. Cheap, and leaves the auditor wrong.
  3. Generate both pages from `ast_type_map()` at run time instead of from the
     source tables. Measures the artifact rather than its description, and
     costs a built binary at docs time.

---

## 5. Correction (added 2026-11-17, while filing #347)

**This file's headline was wrong, and the repository's own generated
documentation already said so three days before the file was written.**

### What was claimed

That `tsx` is the only built-in language with no generated reference page.

### What is measured

Three registered languages have no page: `duckdb`, `sql` and `tsx`. And the
error runs in both directions — four pages document languages that no build
registers:

| Set | Count | Members |
|---|---|---|
| `languages/` directories | 33 | |
| listed in `SITTING_DUCK_LANGUAGE_MODULES` | 29 | |
| directories with no module and no adapter | 4 | `fsharp`, `haskell`, `julia`, `scala` |
| present in `ast_type_map()` | 28 | the 29 modules less `duckdb`, which has no type table |
| `*_types.def` tables | 30 | |
| generated reference pages | 30 | one per table — including the 4 unbuildable ones |
| registered, but no page | 3 | `duckdb`, `sql`, `tsx` |

`docs/reference/languages/index.md` carries a generated warning naming all
seven, added 2026-11-13 in `03fa1f0`:

> **Fsharp, Haskell, Julia, Scala are not among them.** [...] no adapter is
> built, so they error with "Unsupported language". Those pages are design
> references.
>
> Conversely, 3 **supported** languages have no page here: `duckdb`, `sql`,
> `tsx` (no `_types.def` to generate one from).

So the documentation half of this bug is **documented and deliberate**, not a
defect. I wrote this file without reading the index page I had just regenerated.

### The real defect

One cause sits under both halves: a declaration table can live in three places,
and the resolver knows two.

| Convention | Languages | `resolve_config_source()` handles it |
|---|---|---|
| `languages/<lang>/<lang>_types.def` | 30 | yes |
| inline `DEF_TYPE` in `<lang>_adapter.cpp` | `sql` (472), `duckdb` | yes |
| inherited from another language at run time | `tsx` | **no** |

For `tsx` the resolver finds no table, falls through to
`languages/tsx/tsx_adapter.cpp`, and returns it. That file holds **zero**
`DEF_TYPE` entries, because `TSXAdapter` takes TypeScript's configs at run time.
So `scripts/audit_unreachable_declarations.py` prints

    tsx               0            0

where a `0` in column two means "clean". For tsx it means "not measured", and
tsx has 301 node types at run time — the same table typescript has. The script's
own `-- <lang>: no parser.c or no DEF_TYPE source` line exists to make exactly
this visible, and tsx never reaches it because the fallback path resolves.

Second consequence: tsx's grammar is never a reachability source, so a raw_type
emitted only by tsx's parser would be reported unreachable. **No such entry
exists today** (0 unreachable of 3926), so that is latent, not live.

### The ruling

The `tsx 0 0` row is the live defect and the one #347 is about. The missing
reference pages are a documented, deliberate state. The lesson for next time is
in the memory `search-tracker-before-measuring`: the generated artifact I was
about to file a bug against already contained the answer.
