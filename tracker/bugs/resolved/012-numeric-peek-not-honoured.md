# Bug #012: numeric `peek` accepted but not honoured

**Status:** Resolved — NOT A BUG (stale local install)
**Priority:** P2
**Found:** documented in PR #100's investigation, 2026-08-14
**Resolved:** 2026-08-31 — numeric `peek` works correctly on v1.10.3: `peek := 20`
caps previews at 20 chars, `peek := 60` at 60 (verified empirically, not a no-op).
The original report came from a **stale locally-installed build** — the same class
as #100/#101, whose peek/column doc claims were retracted in #101. The docs were
already corrected by #101; the behavior is now **pinned by
`test/sql/numeric_peek.test`** so this cannot be falsely re-reported behind
schema-shaped output. The "conformance kit" recommendation below still stands.

## Symptom (as originally reported — did NOT reproduce on v1.10.3)

`peek := <int>` binds successfully (parsed into `PeekLevel::CUSTOM` +
`peek_size`) but the runtime always emits the 80-char smart-mode output —
the numeric size is never applied.

```sql
SELECT peek FROM read_ast('file.py', peek := 50);
-- rows exceed 50 chars; identical output to peek := 'smart'
```

## Impact

The README and API reference document numeric peek as "fixed-size snippets:
at most N characters per node." The parameter silently does nothing —
the same value-shaped-nothing class as #100's zero columns and #011's
unclassified macros.

## Suspected shape

Same split-brain as #100: `ParseExtractionConfig`
(unified_ast_backend.cpp:~251-280) populates `config.peek = CUSTOM` and
`config.peek_size`, but the peek-emission path likely branches on a legacy
peek_mode/peek_size pair (see read_ast_streaming_state.hpp:93/118,
unified_ast_backend_impl.hpp:579) that CUSTOM never reaches. Unverified —
diagnose before fixing.

## Test to add (behavior, not interface)

```sql
query I
SELECT bool_and(length(peek) <= 50) FROM read_ast('f.py', peek := 50) WHERE peek IS NOT NULL;
-- expect true; today false
```

## Recommendation for v2 conformance kit

Generalize PR #100's `source_full_columns.test` approach into a kit rule:
**every advertised column/parameter must demonstrably take effect on a
canonical fixture** (columns vary, sizes bind, filters filter). Schema
presence and binder acceptance are not evidence of behavior — #100, #011,
and this bug are all the same failure class caught late.
