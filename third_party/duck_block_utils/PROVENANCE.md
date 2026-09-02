# Vendored from duckdb_duck_block_utils

These files are a **vendored copy** — do not edit here; re-sync from upstream.

- **Source repo:** teaguesterling/duckdb_duck_block_utils
- **Commit:** `3bd7ff8a2bbe` (origin/main, 2026-09-01)
- **SPEC_VERSION:** 6.2
- **Files:** `duck_block_vocabulary.hpp` (type/encoding vocabulary + SPEC_VERSION),
  `duck_block_conformance.sql` (`duck_blocks_validate()` and friends — pure SQL,
  requires no extension loaded).

sitting_duck's `ast_to_blocks` emits duck_blocks conforming to this version; the
conformance test in `test/sql/duck_blocks_conformance.test` runs the validator here
over real `ast_to_blocks` output so producer/spec drift is caught (see tracker/bugs/014).
