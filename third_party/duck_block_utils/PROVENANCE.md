# Vendored from duckdb_duck_block_utils

These files are a **vendored copy** — do not edit here; re-sync from upstream.

- **Source repo:** teaguesterling/duckdb_duck_block_utils
- **Commit:** `5017b86` (fix/duckdb-v2-compat, PR #26, 2026-09-05)
- **SPEC_VERSION:** 6.5
- **Files:** `duck_block_vocabulary.hpp` (type/encoding vocabulary + SPEC_VERSION),
  `duck_block_conformance.sql` (`duck_blocks_validate()` and friends — pure SQL,
  requires no extension loaded).

sitting_duck's `ast_to_blocks` emits duck_blocks conforming to this version; the
conformance test in `test/sql/duck_blocks_conformance.test` runs the validator here
over real `ast_to_blocks` output so producer/spec drift is caught (see tracker/bugs/014).

**6.5 note (PR #26):** the 6.4→6.5 change is BREAKING on the FUNCTION SURFACE only —
`duck_blocks_toc`/`_headings`/`_code_blocks`/`_links`/`get_section`/`get_pages`/
`sections_like` now return `LIST(duck_block)`; the old projections live on as `_structs`
siblings and the old strings as `_text` siblings; named params removed. **The struct
shape is unchanged.** sitting_duck is a *producer* (`ast_to_blocks`) and calls none of
those functions, so it is unaffected — this sync is a vocabulary/conformance refresh, not
a migration. Any future consumer of those functions must move to the `_structs`/`_text`
siblings (a binder error, never a wrong answer, if missed).
