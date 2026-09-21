# 040 — Body-aware duck_blocks reader (DEFERRED to v2.0-era)

**Status:** deferred — blocked on a runtime dependency that does not exist yet.
**Origin:** duck_blocks spec 1.3 (`IsBody` predicate), 2026-09-13 session.

## What this would be

A sitting_duck reader helper that selects the *body* of a document-shaped block
list — i.e. excludes the verbatim `metadata` blob — so callers get "what a text
renderer / indexer / embedder should see" without reimplementing the rule.

The 1.3 rule (canonical, from duck_block_utils):

```
IsBody(kind, element_type) := kind IN ('block','inline') AND element_type <> 'metadata'
```

A `kind` filter alone does NOT give you the body: the `metadata` blob is
`kind='block'` (it carries a level + source position) yet is not prose, so
filtering on kind leaks frontmatter into the body. sitting_duck emits such a
metadata block from `ast_to_blocks` (the `include_bodies := false` path, and the
module-metadata block generally), so a body-aware reader is a legitimate consumer
helper over `ast_to_blocks` output.

## Chosen design (when unblocked): lazy-consume the canonical scalar

Route through duck_block_utils' canonical `duck_block_is_body(kind, element_type)`
scalar rather than minting a fourth copy of the predicate — the whole point of the
1.3 bump was to kill the "every consumer wrote its own" drift. Auto-load
duck_block_utils on use, mirroring the existing `func_apply` pattern
(`sitting_duck_enable_dynamic_predicates` / `TryAutoLoadExtension`), degrading with
a clear "install it" message when absent.

## Why it's blocked (the hard part)

sitting_duck runs on **DuckDB v2.0-cyanoptera** (submodule pin `e3946f2327`,
v2.0.0-dev83854). duck_block_utils is on the **v1.5.x** DuckDB line (submodule
`b155d6f63c`), and the community registry only builds it against released DuckDB.
Extensions are ABI-locked to a DuckDB build, so sitting_duck's runtime **cannot
load any existing duck_block_utils** — verified: `LOAD duck_block_utils` in
sitting_duck's own `duckdb` reports no candidate for platform `e3946f2327`.

Therefore the canonical `duck_block_is_body` scalar is **unreachable** from
sitting_duck's runtime today. A stub-macro test would only prove the filter's
plumbing, not the predicate — worthless. Mirroring the predicate locally was
rejected (structural drift). So this waits.

## Unblock condition

duck_block_utils ported to / built against DuckDB v2.0 (whatever line sitting_duck
is on at that point). Most naturally handled during the **v2.0 refactor** (see
#87 sitting_duckling / #71 SQL-macro extraction to Fledgling), when the whole
fleet's DuckDB target is being reconciled. At that point: build the lazy-consume
reader + a real integration test against a matched duck_block_utils build.

## Not blocked, shipped separately

- **Producer sync to 1.3** — done (PR #126): vendored header + PROVENANCE + a
  conformance-test note. sitting_duck is a producer; the bump is mechanical, no
  runtime dep. `ast_to_blocks` output already conformant (metadata block emitted
  with `element_type='metadata'`, so a 1.3-aware consumer excludes it correctly).
- **duckeye reader fix (③)** — duckeye shells out to a *stock* v1.5.x `duckdb` and
  loads community duck_block_utils, so it is NOT blocked by the v2.0 gap; it gets a
  version-floor (warn at init/update) + an xfail-until-1.3 regression test.
