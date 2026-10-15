# 049 — Language support parity: what "we support 28 languages" actually means

**Status:** Measured 2026-10-13, on `main` at the v1.16.0 release candidate
(`e8ce28c`). Written to answer a question asked before cutting: *"can we
confirm we are properly supporting all languages?"*

**Short answer: no, not uniformly — and the gaps are now located rather than
suspected.** 13 of 28 languages pass every check that exists. The rest range
from "passes everything that applies to it" down to one language where the
headline `.call` selector structurally cannot work.

Nothing here is a v1.16.0 regression. All of it predates the release, which is
why v1.16.0 shipped and this is scoped as the release after.

## The bar

"Properly supported" had no definition, which is why the question was hard to
answer. Proposed, in increasing strength:

1. **Registers and parses** — `ast_supported_languages()` lists it and fixtures
   produce nodes with no ERROR. *28 of 28.*
2. **Has a declaration table** — a `.def` the runtime can see. *27 of 28;
   `duckdb` wraps DuckDB's own parser and ships none.*
3. **Has a dedicated test** — at least one `*_language_support.test`,
   `*_refinements.test`, `<lang>.test` or `<lang>_*.test`. *28 of 28.*
4. **Passes every applicable conformance check** — `scripts/conformance_kit.sh`,
   8 checks. *See below; this is where it stops being uniform.*
5. **Round-trips byte-exactly** — `ast_unparse_exact`. *27 of 27 tree-sitter
   languages; `duckdb` is out of scope by construction (AST not CST).*
6. **Full leaf-text coverage** — every named leaf unparses to its text.
   *26 of 28 at zero gaps.*

## Axis 1 — the conformance kit: 8 checks x 28 languages = 224 cells

```
ok=189  n/a=19  void=7  UNDECL=5  FAIL=3  dirty=1      (after row 5 below)
ok=189  n/a=13  void=13 UNDECL=5  FAIL=3  dirty=1      (as first measured)
```

A seven-way partition of the 28, which sums — stated that way on purpose,
because the first draft of this section grouped 8 languages under a heading
that said 7 and the error was invisible until the totals were added up.

| group | n | languages |
|---|---|---|
| **All 8 checks `ok`** | **13** | c, cpp, csharp, go, java, javascript, php, python, r, ruby, rust, swift, typescript |
| All applicable `ok`, only `n/a` cells | 2 | hcl, zig (`NATIVE-DECL` n/a) |
| All applicable `ok`; `RAW-JOIN` void **inherently** | 4 | json, markdown, toml, yaml — a data format has no calls, so the check can never apply |
| `void` from **fixture thinness**, nothing behind it | 3 | bash, css, html (`NATIVE-DECL`; bash/html also `RAW-JOIN`) |
| **Real gap found by investigating `void`** | **2** | dart, graphql (#250) |
| **Real `FAIL`** | **3** | kotlin, lua, sql (#220) |
| Structurally exempt | 1 | duckdb |

13 + 2 + 4 + 3 + 2 + 3 + 1 = 28, and 189 + 13 + 13 + 5 + 3 + 1 = 224 = 8 x 28.
Both totals close, so no language and no cell is unaccounted for.

**The 3 `FAIL`s (#220)** are the sharpest result: kotlin, lua and sql each
declare a function-shaped native strategy and deliver **zero** parameters, so
the declaration is unbacked — the module claims a capability it does not have,
which is precisely what the kit exists to catch. kotlin additionally parses
`dirty` (1 ERROR/MISSING node of 799 in simple.kt).

**The 2 real gaps (#250)** had never been looked at. `void` is not a pass, and
reading those cells as questions rather than as background is the only reason
they were found.

**duckdb** carries 5 x `UNDECL`: no `.def` is reachable, so nothing about its
capabilities is declared anywhere the runtime can see. It still parses, and has
7 dedicated test files.

## Axis 2 — declaration table size

typescript 293, sql 263, cpp 232, c 218, dart 216, javascript 201, python 201,
kotlin 186, rust 181, ruby 178 … php 90, markdown 85, bash 83, csharp 57,
yaml 46, toml 33, html 27, json 20, duckdb 0. Total **3698**, equal to what the
extension loads.

Low counts are mostly honest — json has 20 node types because JSON has 20 node
types. **csharp at 57 is the outlier worth a look**: it is a large language with
fewer declarations than bash.

## Axis 3 — dedicated tests

All 28 have at least one. **13 also have a `_refinements.test`** (bash, c,
csharp, dart, go, java, lua, php, python, rust, swift, typescript, zig); the
other 15 have only the `_language_support` level. Refinement-level assertions
are the ones that catch a wrong *semantic type* rather than a failure to parse,
which is the class #231 and #222 lived in.

## Axis 4 — leaf-text coverage

212 gaps, and they are **not** spread out: **sql 210, yaml 2, everything else
0**. The sql gaps are the `keyword_*` family already recorded in `048` row 1.
So this axis is one language's problem, not a fleet-wide one.

## The work, in recommended order

| # | Item | Issue | Size | Why this order |
|---|---|---|---|---|
| 1 | graphql: decide `field` = call or access | #250 | Hours | A recorded decision either way. **Correction:** this row originally said that if access stands the kit cell becomes `n/a`. It does not — graphql genuinely declares call naming on `fragment_spread`, so its `void` is a corpus gap wanting a fixture (row 4), not a reclassification. The `n/a` lever was elsewhere: RAW-JOIN had no capability gate at all, which row 5 fixed |
| 2 | kotlin/lua/sql native strategies deliver no parameters | #220 | Days | The only cells that are outright **FAIL**. A module that declares a capability it lacks is the thing the kit exists to catch |
| 3 | dart: ordinary calls unreachable to `.call` | #250 | Days | Needs a previous-sibling name strategy or a CUSTOM handler — the grammar puts the callee in a *sibling* of the arguments node, so no existing strategy reaches it |
| 4 | Conformance fixtures for the remaining `void` cells | — | Hours | bash/css/html `NATIVE-DECL`, dart/graphql `RAW-JOIN`. A `void` cell is indistinguishable from an unexamined one, which is exactly how #250 survived |
| 5 | Mark inherently-void cells `n/a` | — | Hours | **DONE.** `check_raw_join` had no capability gate, so json/toml/yaml/markdown/html/bash reported CALL-NAMED `n/a` and RAW-JOIN `void` about the same fact. Gated on the declared call-naming capability: **6 cells moved `void` → `n/a`**, totals `void=13 → 7` and `n/a=13 → 19`, still summing to 224 |
| 6 | sql leaf-text `keyword_*` | `047`, `048` row 1 | Days | 210 of 212 gaps. Unparse works; the names do not |
| 7 | `_refinements.test` for the 15 languages without one | #203-adjacent | Days | The level that catches misclassification rather than non-parsing |
| 8 | csharp declaration coverage | — | Investigate | 57 declarations for a language of that size; may be fine, has not been checked |

## What this says about the kit

The kit was built to answer exactly this question and it did — but only after
someone read the `void` cells as *questions* rather than as background. 13 void
cells sat in a recorded baseline for days; 4 of them were a real gap in 2
languages. The lesson for `044`'s baseline discipline: **a baseline that
records `void` without a per-cell reason is a to-do list that looks like a
result.** Items 4 and 5 above exist to make every cell either a pass, a stated
failure, or an `n/a` with a reason.
