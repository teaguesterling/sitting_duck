# 047 — unparse level 4: leaf-text coverage + node templates

**Status:** 4a largely done (see "4a status" below); 4b not started. On the v2.0.0
list; the work was described in 041 as future steps but never tracked as a milestone
(this is it).
**Related:** 041 (the PoC and the fidelity path), 045 / #174 (level 5 — `COPY ... TO
(FORMAT ast)`), shipped in v1.15.0 (default rules, 27 languages) and v1.15.1 (style
presets, custom rules tables, synthetic AST layout).

## Where the levels stand

041 laid out a three-step fidelity path. Step 2 shipped; steps 1 and 3 did not, and
together they are what "level 4" means:

| 041 step | what it is | status |
|---|---|---|
| 1. Leaf-text coverage | every named, non-self-describing leaf needs `NODE_TEXT` so its text is recoverable | **done for 25 of 26 tree-sitter languages**; sql's `keyword_*` table outstanding |
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

## 4a status

Derived list, fixes and acceptance tests landed. Audit tooling lives in
`scripts/` and is re-runnable after a grammar bump:

- `scripts/audit_leaf_text_gaps.py` — reads each committed parser's symbol table
  (`enum ts_symbol_identifiers`, `ts_symbol_names[]`, `ts_symbol_metadata[]`,
  `TOKEN_COUNT`) and takes the visible+named symbols with `id < TOKEN_COUNT`.
- `scripts/apply_leaf_text_fixes.py` — applies the edits (idempotent; a clean
  tree produces no edits).
- `scripts/sweep_leaf_text_observed.py` — parses the corpus, confirms live gaps,
  and **validates the static derivation** against what is actually observed.
- `scripts/make_leaf_text_corpus.py` — writes `test/data/unparse_leaf_text/`.

Each takes `--help`. The gap list is recomputed in-process every run; there is
deliberately **no committed JSON snapshot**, because a tracked snapshot goes
stale silently the first time a grammar moves and a stale baseline is worse
than none. The outstanding work is recorded as prose below instead.

Result — 348 gaps found, 134 closed, 214 outstanding:

- 705 named leaves across 26 tree-sitter languages; **344** lacked a text
  strategy per the grammar derivation, plus **4** the derivation structurally
  cannot see (found by the corpus sweep) = **348**.
- Closed **134**: 125 static non-sql (121 file edits — TypeScript's 4 close via
  javascript_types.def, which it `#include`s), 4 empirical, 5 sql non-keyword.
- Outstanding **214**: sql `keyword_*` only.

Acceptance test: `test/sql/ast_unparse_leaf_text.test` (35 assertions, 1107
corpus leaves across 26 languages).

The reproducer now returns `# hello there\nx = 1`.

### Outstanding

- **sql: 214 `keyword_*` leaves.** Real — `ON DELETE NO ACTION` unparses as
  `keyword_no keyword_action`. Deferred because the existing SQL keyword entries
  carry meaningful semantic types and `IS_KEYWORD`, so 214 `PARSER_CONSTRUCT`
  rows beside them want their own change (and probably a per-keyword semantic
  classification pass).
- **`duckdb` (native parser) — filed as issue #197.** Its adapter reports
  `children_count = 0` for *every* node (while `descendant_count` is correct),
  so the whole tree reads as leaves and `ast_unparse_code(..., 'duckdb')` emits
  `program select_statement select_node ...`. A separate structural bug, not a
  leaf-text gap.

### Findings worth carrying forward

- **"Anonymous tokens need nothing because `type` == text" is false.**
  tree-sitter reuses one token symbol across grammar alternatives, so lua's
  `[[` also matches `--[[` and ruby's `"` symbol also matches `'` — the latter
  meant single-quoted Ruby strings round-tripped with a double quote. Any future
  audit of this kind must not assume anonymous ⇒ safe.
- **Three classes are invisible to a terminals-only derivation** and need the
  empirical sweep: named non-terminals whose children are all hidden (dart
  `comment`), anonymous tokens with variable text (above), and visible+named
  alias symbols (kotlin `interpolated_identifier`).
- **Pre-existing duplicate DEF_TYPE keys shadow live entries.** Under
  first-wins, `graphql_types.def` declares `type` and `directive` twice,
  `kotlin_types.def` declares `annotation` twice, and `ruby_types.def`
  declares `super`, `class` and `module` twice — in each case the later entry
  is dead code. Unrelated to this change (present before it), but a real bug:
  the keyword-section entries never take effect.
- **Semantic-type follow-up for the semantic-types owner:** 31 of the newly
  added entries have cross-language precedent for a richer type than
  `PARSER_CONSTRUCT` (e.g. `string_content` is `LITERAL_STRING` in 7 other
  languages, `escape_sequence` in 12, `comment` is `METADATA_COMMENT` in 25).
  Deliberately not applied here — 4a is a text fix, not a reclassification.
- **Byte-identity does not hold** for any of the 26 corpus languages; it fails
  on inter-token whitespace only. Python's output is semantically correct and
  differs just in spacing (`->` vs ` -> `). This is the open question below, and
  it is now measured rather than assumed.
- **Some grammars hide delimiters entirely:** tree-sitter-kotlin's
  `string_literal` has no child node for its quote characters, so leaf
  concatenation structurally cannot recover them. That is 4b work.

## 4b — node templates (enables transformation)

Per-node-type unparse templates, so a *modified* tree can be emitted with controlled
layout rather than reconstructed from leaf concatenation. 041 calls this "heavier; not
needed for a round-trip PoC" — correct then, but it is the prerequisite for the
transformation story (`ast_patch` / `ast_replace`, shipped v1.11.0) producing
idiomatic output rather than normalized-whitespace output.

**SETTLED 2026-10-07 (Teague) — 4b is unblocked.** The open question was *"is
byte-exact in scope, or is normalized reconstruction the target?"* Answer: **both,
at different retention levels**, which is what makes 4b designable.

| retention | law | strength |
|---|---|---|
| `source := 'full'` | `write_ast(read_ast(x, source := 'full')) = x` | **byte-exact** |
| anything less | `read_ast(write_ast(read_ast(x))) = read_ast(x)` | **pseudo-inverse only** — tree survives, text need not |

Explicitly **not** required below `'full'`: `write_ast(read_ast(x)) = x`. Normalising
whitespace there is conformant, not a bug, and must not be reported as one (#89).

Two further decisions from the same ruling:
- **`language :=` is an override, not a requirement.** `write_ast` infers the language
  from the `language` column that `read_ast`/`parse_ast` already emit. A table with
  more than one distinct `language` errors unless `language :=` resolves it; same for
  multi-`file_path` tables, which have no single textual answer.
- **COPY form is the same writer as a sink:**
  `COPY (FROM read_ast(x, source := 'full')) TO 'x2' (FORMAT ast, LANGUAGE 'c')`,
  with `LANGUAGE` optional when the column is unambiguous. This is 045/#174.

See `docs/planning/v2-architecture.md`, the `write_ast` laws, for the authoritative
statement.

## Why it gates v2.0.0

- Unparse is the output half of the transformation story the v2.0 architecture is
  organised around. `ast_patch`/`ast_replace` can modify a tree today, but what comes
  out is whitespace-normalized, so round-tripping a real file is lossy.
- 4a is a *correctness* gap with a known reproducer, not a feature. Shipping v2.0.0
  with `comment` unparsing to the literal text `comment` would be hard to defend.

## Sequencing note

4a has **landed** for 25 of the 26 tree-sitter languages (PR #199); sql's 214
`keyword_*` leaves remain, deferred for a per-keyword classification pass, and sql's
5 non-keyword gaps are closed. 4b no longer waits on a decision (settled 2026-10-07)
and its substrate now exists: `start_byte` / `end_byte` are exposed under
`source := 'full'` (PR #198), so byte-exact slicing is a `substring()` on raw bytes
rather than a reconstruction from line/column — which was fragile with multi-byte
characters and mixed line endings. The remaining order is:

1. ~~**4a** — leaf-text coverage.~~ **Landed**, PR #199. 134 of 348 grammar-derived
   gaps closed; 214 sql `keyword_*` outstanding. See also #200 (the
   anonymous-token-with-variable-text class is corpus-bounded, not enumerated) and
   #197 (the `duckdb` adapter's unparse is broken for an unrelated reason —
   `children_count` is 0 on every node).
2. ~~**byte offsets under `source := 'full'`**~~ — **Landed**, PR #198.
3. **4b** — node templates, designed against the byte-exact target. Now unblocked on
   both counts: the decision is made and the substrate is in.
4. **045/#174** — the COPY sink. Gated on 4a, which has landed, so this is now
   available to start. Writing lossy output to disk is worse than returning it in a
   result set, because it looks like a file you can keep.

Note the stated textual law takes a *path*, so the file-backed case needs only a
file re-read with honest staleness detection — not per-node text retention. Per-node
retention is required only for `parse_ast` over a string and for tables that outlive
their files, and can follow later.

Measured caveat for 4b: byte-identity under `source := 'full'` does **not** hold
today — it fails for all 26 corpus languages, on inter-token whitespace only. The
unparse macros take no `source` parameter and never read the position columns. That
is the gap 4b closes, and it is why 4b is required rather than optional.
