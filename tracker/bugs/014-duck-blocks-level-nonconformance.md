# 014 — ast_to_blocks emits `level` non-conformant with current duck_blocks spec

**Status:** open — assessment only, no code change yet (kibitzer floor blocks src/test edits; needs Teague to widen `[floor] allow` or work in a permitted mode)
**Found:** 2026-09-01, reviewing duck_block_utils `origin/main` @ SPEC_VERSION **6.2**
**Our reader cites:** duck_blocks spec **v0.4.0** (`src/sql_macros/duck_blocks.sql:7`)

## Summary

`ast_to_blocks_from` / `ast_to_blocks` / `ast_to_blocks_list` emit a duck_block STRUCT
that is conformant in **every field except `level`**. We set `level` per the old
"NULL-at-top-level" convention, which the spec explicitly **reversed and calls "never
approved"**.

- metadata block → `level = 0`
- heading / code / paragraph → `level = NULL`
  (`src/sql_macros/duck_blocks.sql`, CTE projections feeding the block STRUCT at `:286-294`)

Current spec (`duck_block_vocabulary.hpp` version history, 2.0→3.0):
> *every element carries an EXPLICIT level; no NULLs, one scale for blocks and inlines,
> and `level` is never semantic. The NULL-at-top-level convention 1.x and 2.0 documented
> was never approved.*

The conformance validator `duck_block_is_valid` asserts `elem.level >= 1`. Under it,
**every block we emit fails** — `NULL >= 1` is NULL, `0 >= 1` is false. This is the same
year-long, four-extension drift the 3.0+ spec exists to end; the v1-alignment review
(2026-08-31) reaffirms `level ≥ 1` structural-depth as the *v1-faithful* reading, not a
2.0 invention.

## The fix (small, but it changes emitted output + tests)

We emit a **flat** outline — `kind` is always `'block'`, nothing nests (agent-verified:
no container children anywhere in the blocks path). So the correct structural depth for
**every** block we emit is `level = 1`.

- metadata: `0` → `1`
- heading / code / paragraph: `NULL` → `1`
- heading *rank* stays where it already correctly is: `attributes['heading_level']`
  (string `'1'..'6'`) — do **not** move it into `level`.

Then update `test/sql/duck_blocks.test`, which currently *enshrines the old rule* — it
asserts `block.level IS NULL` for headings (`:37-45`). That assertion must flip to
`level = 1`. (The test was written against the same v0.4.0 the producer claims, so it
cannot independently catch this drift — see "Vendoring" below.)

### Implementation traps
- `src/include/embedded_sql_macros.hpp` is **build-generated** and carries the same
  macros (~line 4664). The CI job "Embedded SQL macros header in sync" fails if you edit
  `duck_blocks.sql` without regenerating it.
- **kibitzer floor**: `src/sql_macros/duck_blocks.sql` and `test/sql/duck_blocks.test` are
  outside the `tracker/**` allow-list. The code fix needs `[floor] allow` widened first.

## Everything else already conforms (verified against 6.2)

| field | we emit | 6.2 | verdict |
|---|---|---|---|
| STRUCT shape | `kind, element_type, content, level, encoding, attributes, element_order` | same 7 fields | ✅ |
| `kind` | `'block'` | block/inline/value | ✅ |
| `element_type` | metadata, heading, code, paragraph | all recognized (`TYPE_METADATA`, `TYPE_HEADING`, `TYPE_PARAGRAPH`, `TYPE_CODE`) | ✅ |
| `encoding` | `yaml`, `text` | both legal (`ENCODING_YAML`, `ENCODING_TEXT`) | ✅ |
| `heading_level` | string `'1'..'6'` in attributes | matches | ✅ |
| `element_order` | 0-based, per-file order | matches | ✅ |
| content rule | leaf blocks carry content, no container children | universal rule (content XOR children) trivially satisfied | ✅ |
| **`level`** | **NULL / 0** | **must be ≥ 1** | ❌ **this bug** |

The pandoc-only additions (figure, caption, deflist, lineblock, underline, plain) are
**not our concern** — we read source code, not documents.

## Why this matters concretely (not a hypothetical consumer)

The duck_block_utils reader-dispatch design (`docs/superpowers/specs/2026-08-31-pandoc-gaps-and-reader-dispatch-design.md`)
names us a first-class reader:
- sitting_duck is the **fallback reader** for extensions no document/data reader claims
  (`.py .rs .go .css` …); it is **never** chosen for `.md/.html/.json/.toml` (native
  readers win), reachable for those only via `format := 'code'`. Dispatch is *derived*
  from `ast_supported_languages()` — which we already expose
  (`src/ast_supported_languages_function.cpp:82`) — so **no dispatch change is needed on
  our side**; that ownership moves to panduck.
- The design's `doc_select` exception states **"`ast_to_blocks_from` remains the only path
  producing blocks from a selection."** i.e. the library asserts a hard dependency on our
  reader's output shape *by name*. Our `level` non-conformance is therefore a real
  cross-extension defect, not a latent one.

## Recommendation — split settled from churning

1. **Fix `level` now.** Stable since spec 3.0, reaffirmed v1-faithful, won't churn back.
   TDD: flip the `duck_blocks.test` level assertions first, then the projections, then
   regenerate `embedded_sql_macros.hpp`, then full suite.
2. **Vendoring — Teague's call, defer.** The right long-term move is to vendor
   `duck_block_vocabulary.hpp` + `vendor/duck_block_conformance.sql` and add a test that
   runs `duck_blocks_validate()` over our output, so producer and tests stop citing the
   same private version and drift gets caught. But SPEC_VERSION went 3.0→4.0→6.0→6.2 in
   ~24h and 6.2 is a day old on an unreleased branch — pinning to a fast-moving version is
   a deliberate decision, not a default. (This is tracker item #22.)
3. Bump the `v0.4.0` citation comment to whatever version we vendor/target.
