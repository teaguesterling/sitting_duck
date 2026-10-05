# 047 — unparse level 4: leaf-text coverage + node templates

**Status:** Not started. On the v2.0.0 list; the work was described in 041 as future
steps but never tracked as a milestone (this is it).
**Related:** 041 (the PoC and the fidelity path), 045 / #174 (level 5 — `COPY ... TO
(FORMAT ast)`), shipped in v1.15.0 (default rules, 27 languages) and v1.15.1 (style
presets, custom rules tables, synthetic AST layout).

## Where the levels stand

041 laid out a three-step fidelity path. Step 2 shipped; steps 1 and 3 did not, and
together they are what "level 4" means:

| 041 step | what it is | status |
|---|---|---|
| 1. Leaf-text coverage | every named, non-self-describing leaf needs `NODE_TEXT` so its text is recoverable | **not done** |
| 2. Spacing / tight-token rules | per-language spacing, keyword-vs-identifier before `(` | **shipped** as the rules engine + presets (v1.15.0–v1.15.1) |
| 3. Node templates | per-node-type unparse templates — emit a *modified* tree with exact layout | **not done** |

Level 5 (`045` / #174) is already tracked and is "Planned (Phase 6)". It writes
unparsed output to disk; it does not improve fidelity, so it sits on top of this.

## 4a — leaf-text coverage (correctness, not polish)

041 records a gap that is **still open — verified 2026-10-04 against v1.15.4**:
`comment` has no name strategy, so unparse emits the literal string `comment` in
place of the comment's text.

```sql
SELECT source FROM ast_unparse_code('# hello there' || chr(10) || 'x = 1', 'python');
-- emits the word `comment` where `# hello there` should be
```

That is a correctness bug in round-tripping, not a formatting nicety: the output is
not the input, and it is not flagged as lossy.

Work:
- Audit every language's `.def` for named, text-bearing leaves lacking `NODE_TEXT`:
  comments, numeric and string literals, operators that appear as *named* nodes.
  Anonymous tokens need nothing (`type` == text).
- 27 languages × the audit. This is mechanical but wide, and it is the kind of list
  that must be derived from the grammars rather than written by hand — the same
  lesson as deriving the zero-arity pseudo-class list from the predicate dispatch
  rather than from the two reported cases (#184).
- Acceptance: a round-trip test per language asserting no output token equals its own
  node type (the `comment` → `"comment"` signature), plus byte-identity on a corpus
  under `source := 'full'` where columns are available.

## 4b — node templates (enables transformation)

Per-node-type unparse templates, so a *modified* tree can be emitted with controlled
layout rather than reconstructed from leaf concatenation. 041 calls this "heavier; not
needed for a round-trip PoC" — correct then, but it is the prerequisite for the
transformation story (`ast_patch` / `ast_replace`, shipped v1.11.0) producing
idiomatic output rather than normalized-whitespace output.

Open design question from 041, still open: **is byte-exact (`source := 'full'`,
reconstructing inter-token gaps from `start_column`/`end_column`) in scope, or is
normalized reconstruction the target?** 4a can proceed either way; 4b's design
depends on the answer, so decide before starting 4b.

## Why it gates v2.0.0

- Unparse is the output half of the transformation story the v2.0 architecture is
  organised around. `ast_patch`/`ast_replace` can modify a tree today, but what comes
  out is whitespace-normalized, so round-tripping a real file is lossy.
- 4a is a *correctness* gap with a known reproducer, not a feature. Shipping v2.0.0
  with `comment` unparsing to the literal text `comment` would be hard to defend.

## Sequencing note

4a is independent and can land now. 4b should wait on the byte-exact decision above,
and 045/#174 (level 5) should land after 4a — writing lossy output to disk is worse
than returning it in a result set, because it looks like a file you can keep.
