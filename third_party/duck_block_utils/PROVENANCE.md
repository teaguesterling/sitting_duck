# Vendored from duckdb_duck_block_utils

These files are a **vendored copy** — do not edit here; re-sync from upstream.

- **Source repo:** teaguesterling/duckdb_duck_block_utils
- **Commit:** `079123d` (main, PR #32, 2026-09-11)
- **SPEC_VERSION:** `1.3` (public; supersedes the retired internal `6.6` line — see the
  "TWO NUMBER LINES" note in `duck_block_vocabulary.hpp`. sitting_duck previously
  vendored `1.2` = commit `1c5f2a6`, and before that `6.5` = public `1.1`.)
- **Files:** `duck_block_vocabulary.hpp` (type/encoding vocabulary + SPEC_VERSION +
  `ImplicitParentOf`/`RequiresAncestor`; from upstream `src/include/`),
  `duck_block_conformance.sql` (`duck_blocks_validate()` + list-level rules; from
  upstream `vendor/`).

sitting_duck's `ast_to_blocks` emits duck_blocks conforming to this version; the
conformance test in `test/sql/duck_blocks_conformance.test` runs the validator here
over real `ast_to_blocks` output so producer/spec drift is caught (see tracker/bugs/014).

**1.3 note (PR #32):** additive over 1.2 — one new predicate, `struct shape and
validator unchanged`. `IsBody(kind, element_type)` names the document BODY as
`kind IN (block, inline) AND element_type <> 'metadata'`. The point is that a `kind`
filter alone does NOT give you the body: the verbatim `metadata` blob is `kind='block'`
(it has a level + source position) yet is not prose, so a renderer/indexer/embedder that
filters on kind leaks frontmatter into the body. It was found when duck_block_utils'
own `duck_blocks_to_text` and duckeye's renderer both printed a markdown file's
frontmatter above its first heading (2026-09-11). Upstream now ships the predicate at
three layers — C++ `BlockTypes::IsBody`, the SQL scalar `duck_block_is_body(kind,
element_type)`, and the reference renderer routing through it. **sitting_duck is a
producer, not a consumer**, so this bump is mechanical for us: it changes no
`ast_to_blocks` output and adds no new conformance rule (the validator is byte-identical
to 1.2). Any sitting_duck-side body-aware *reader* should consume the canonical
`duck_block_is_body` rather than mint a fourth copy of the predicate.

**1.2 note (PR #30):** version guidance changed to `major == 1 AND minor >= N` (not
whole-string equality). The spec adds, over 6.5:
- **Fragments are legal input** — a consumer must handle/wrap a fragment, never drop it.
- **Implicit-parent table** — `ImplicitParentOf(type, kind)` / `RequiresAncestor(...)`
  (constexpr, C++11-safe); exposed upstream as `duck_block_implicit_parent`.
- **Validation over the LIST** (`field = 'list'`): L1 dense `element_order` from 0 in
  list position, L2 shallowest level 1, L3 no level jump > 1, L4 required ancestor,
  L5 inline under a block/value.
- **One shape per element_type binds EVERY producer** (tight list item: content = Plain,
  paragraph child = Para).

The **struct shape is unchanged** (same 7-field layout / index constants). This sync
therefore updates the vocabulary + validator; whether `ast_to_blocks` output already
satisfies the new list-level rules is checked by the conformance test. The optional
`vendor/duck_block_normalize.hpp` transform (collapse a lone `plain` into its container)
is NOT vendored here — add it only if the producer needs to normalize its output.
