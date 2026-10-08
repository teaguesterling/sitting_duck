# 047 — unparse level 4: leaf-text coverage + node templates

**Status:** 4a largely done (see "4a status" below). 4b is **split in two, and the
first half has landed**: byte-exact reproduction of an *unmodified* tree is done and
measured (see "4b part 1" below); per-node templates for *synthesized / modified*
nodes are not started. On the v2.0.0 list; the work was described in 041 as future
steps but never tracked as a milestone (this is it).
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
| 3a. Byte-exact round trip | reproduce an *unmodified* tree byte for byte under `source := 'full'` | **done** — `ast_unparse_exact*`, 26/26 languages |
| 3b. Node templates | per-node-type unparse templates — emit a *modified* tree with exact layout | **not done** |

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
- **Byte-identity does not hold** for any of the 26 corpus languages **for the
  rules-based macros**; it fails on inter-token whitespace only. Python's output
  is semantically correct and differs just in spacing (`->` vs ` -> `). That is
  still true of `ast_unparse*` and is *conformant* below `source := 'full'`
  (#89) — but it is no longer the whole picture: `ast_unparse_exact*` now holds
  byte-identity for 26/26, by splicing instead of reconstructing. See "4b part
  1" below.
- **Some grammars hide delimiters entirely:** tree-sitter-kotlin's
  `string_literal` has no child node for its quote characters, so leaf
  concatenation structurally cannot recover them. **Resolved by 4b part 1, and
  the framing was wrong:** it is not a structural limit of *unparse*, only of
  *leaf concatenation*. A splice computes the gaps between leaves from BYTE
  OFFSETS rather than from grammar knowledge, so bytes inside a parent that no
  child covers — exactly kotlin's quotes, verified at bytes 75 and 78 of
  `test/data/unparse_leaf_text/sample.kt` — are emitted verbatim as gap bytes.
  No language needed special handling.

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

## 4b part 1 — byte-exact round trip (LANDED)

4b as written ("per-node unparse templates") is two separable pieces, and only the
second needs design decisions:

| | what | status |
|---|---|---|
| **part 1** | byte-exact reproduction of an **unmodified** tree | **landed** |
| **part 2** | templates for **synthesized / modified** nodes — emit a changed tree with controlled layout | not started |

Part 1 is precisely specified by the settled law and needs no template vocabulary
at all, because nothing is generated: the output is a **splice** of the original
bytes.

### The design

Four new macros in `src/sql_macros/ast_unparse.sql`, strictly additive (367
inserted lines, 0 deleted; the pre-existing 391 lines are byte-identical to
`6f82c98`):

| macro | input | bytes from |
|---|---|---|
| `ast_unparse_exact(path, language := NULL)` | a path | `read_blob(path)`, same statement |
| `ast_unparse_exact_from(ast_table, files, language := NULL, file_path := NULL)` | a node table | `read_blob(files)` |
| `ast_unparse_exact_code(source_code, language)` | an in-memory string | the string argument |
| `ast_unparse_exact_splice(ast_table, blob_table, …)` | the shared core | a `(fp, cblob)` relation |

The splice walks the **leaf frontier** — rows with `descendant_count = 0`, an
antichain, so their spans are disjoint — in `(start_byte, end_byte, node_id)`
order and emits, per leaf, the GAP `[previous end_byte, this start_byte)` then
the LEAF `[start_byte, end_byte)`, finishing with the TAIL
`[last end_byte, EOF)`.

**Why per-leaf-plus-gap rather than "slice the root's whole span".** The two
produce the same bytes for an unmodified tree, and that is the point: they differ
in what they *prove* and in what they generalise to.

- The splice is a **tiling proof**. Every byte is emitted exactly once,
  attributed either to a leaf or to a gap. A root-span slice is byte-exact while
  proving nothing whatever about the tree — and would not even be correct in
  general, because the root's span does not always start at byte 0 (leading
  whitespace sits outside it: `root.start_byte = 6` for a file beginning with
  three blank lines, measured).
- The splice is the shape **part 2** needs: substitute one leaf's slice and
  everything around it still comes from the original bytes. Asserted directly —
  `test/sql/ast_unparse_exact.test` §3d swaps one leaf for `'99'` and requires
  the exact edited text.

`descendant_count = 0` is used rather than `children_count = 0` (which the
rules-based macros use). The two select identical rows on all 26 languages
(measured), but the former is definitionally "no descendant rows exist", which
is the antichain property the splice needs, and it is immune to an adapter that
miscounts children (#197).

`language :=` is an override, not a requirement: it is inferred from the
`language` column, and a table carrying more than one language — or more than one
`file_path` — errors unless `language :=` / `file_path :=` resolves it. Note the
deliberate asymmetry with `ast_unparse(glob)`, which returns one row per file:
the textual law is stated over a single `x`, and the same writer has to serve the
COPY sink (045/#174) where one destination means one textual answer.

### Measured result

| | before (`6f82c98`) | after |
|---|---|---|
| byte-identity, 26-language corpus | **0 / 26** | **26 / 26** |
| byte-identity, CRLF + multi-byte fixtures | 0 / 2 | 2 / 2 |
| byte-identity, 176 real source files (incl. a 352 KB and a 207 KB file) | — | **176 / 176**, 421 267 gap bytes spliced |
| rules-based `ast_unparse` on the same corpus | 0 / 28 | 0 / 28 (**unchanged — conformant, #89**) |

Languages served: **all 26 tree-sitter languages**, with no per-language code.
Not served: `duckdb` (#197) — no byte positions, and `depth = 0` on several
nodes; it **errors** rather than emitting anything.

The kotlin case 4a recorded as a structural limit is served too: `"hi"` comes
back with its quotes, while the rules-based unparser emits `val s = hi`. Both
lines are asserted side by side in the test.

### Honest failure (#89)

Nine conditions error rather than falling back to normalised output — NULL byte
columns, multi-language, multi-file, missing/duplicate root, **incomplete tree**,
unreadable file, `<inline>` parse, length-mismatch staleness, overlapping leaves.
Each has a `statement error` assertion. The incomplete-tree check (row count =
`root.descendant_count + 1`) is the subtle one: a filtered subset would be
absorbed into the gaps and come out byte-exact anyway, looking right while
proving nothing.

Staleness guard: `root.end_byte = octet_length(file)`. tree-sitter's root ends at
EOF for all 26 languages and for empty, whitespace-only and no-trailing-newline
files (measured), so any change to the file's **length** is caught. A same-length
edit is invisible — the same limitation `ast_patch` documents. Parse and unparse
in one motion; `ast_unparse_exact(path)` does.

**That guard is MEASURED, not proven**, and it is the one empirical
generalization the whole thing rests on. If a future grammar's root node stops
short of EOF, `ast_unparse_exact` will error on *every* file of that language
rather than produce a wrong answer — a loud, self-diagnosing failure (the
message names both numbers), and the one
`scripts/verify_unparse_byte_exact.sh` would catch on a grammar bump. The
alternative, dropping to `max(end_byte) <= octet_length(file)`, detects
truncation but not an append, which is a silent wrong answer; the stricter guard
is the right trade.

Two further notes for whoever touches this next. §3a of the test asserts literal
fixture-dependent numbers (`55 172 50` — leaves, leaf bytes, gap bytes for
`sample.py`); editing that fixture breaks it loudly and obviously.
`ast_unparse_exact_splice`'s `blob_table` argument is **trusted**: the length
guard is all that stands between a caller and a same-length blob from the wrong
file, which is exactly what §5g exploits to make the guard fire.

### Verification

- `test/sql/ast_unparse_exact.test` — 61 assertions (77 as the runner counts
  them). Includes a negative control: a one-byte boundary shift and a replaced
  leaf must both be detected, and the control's own unshifted arithmetic must
  reproduce the file first, so a detected difference is attributable to the plant.
- `scripts/verify_unparse_byte_exact.sh` — the corpus sweep with per-language
  numbers, harness guards (empty glob, wrong row count), vacuity guards (empty
  frontier, zero-width leaves, zero corpus-wide gap bytes) and the same negative
  control. Re-run after a grammar bump.
- Additivity: `ast_unparse.test` (46), `ast_unparse_presets.test` (23),
  `ast_unparse_leaf_text.test` (35) and `ast_patch.test` (64) all pass unchanged,
  and 345 captured outputs of the four pre-existing macros over the corpus and
  all nine presets are identical between the embedded (re-chunked) definitions
  and the pristine `6f82c98` SQL text.

### Incidental findings

- **`blob[i:j]` is a better byte-slicing primitive than
  `from_hex(substring(to_hex(blob), …))`.** It is 1-based inclusive and
  byte-indexed, needs no hex round trip, and is O(slice) rather than O(file) per
  slice. `ast_patch.sql` already relied on it; API_REFERENCE documented only the
  hex form. Both were verified to agree byte for byte on the CRLF + multi-byte
  fixtures, and API_REFERENCE now records both — the hex form stays useful for a
  negative control precisely because it has no UTF-8 validity constraint (a
  boundary planted mid-sequence yields wrong hex rather than a `decode()` error).
- **A scalar subquery inside a `CASE` is materialized whether or not its branch
  is taken.** `CASE` short-circuits its result *expressions*, but the planner
  evaluates the subqueries regardless — so a bare
  `(SELECT nbytes FROM one_row_per_file)` on a two-file table raised DuckDB's own
  "more than one row returned by a subquery" *before* the multi-file branch could
  produce the message that explains the problem. Every validation probe is wrapped
  in an aggregate to keep it single-row. Any future SQL-macro validation chain
  wants this.
- **A macro parameter named after a column it must compare against is a silent
  no-op risk.** `WHERE t.language = language` in a scope that also has a
  `language` column binds the bare name to the column, making the filter always
  true. The columns are renamed in a CTE that references nothing unqualified
  before any filtering, so the parameters are the only possible meaning.
- **`scripts/embed_sql_macros.py` splits mid-statement, and that is fine** — the
  chunks are emitted as adjacent C++ string literals and concatenate at compile
  time. Worth knowing before anyone "fixes" the boundaries: `ast_unparse.sql` went
  from 2 chunks to 3 and the new boundaries land inside statements.

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
3. ~~**4b part 1** — byte-exact round trip for an unmodified tree.~~ **Landed**
   (`ast_unparse_exact*`); 26/26 languages, `duckdb` excluded (#197). See
   "4b part 1" above.
4. **4b part 2** — node templates for synthesized / modified nodes, designed
   against the splice that part 1 established. Still open.
5. **045/#174** — the COPY sink. Gated on 4a, which has landed, so this is now
   available to start. Writing lossy output to disk is worse than returning it in a
   result set, because it looks like a file you can keep. 4b part 1 gives it a
   byte-exact writer for the `source := 'full'` case, and the single-file /
   single-language error semantics it needs are already enforced there.

Note the stated textual law takes a *path*, so the file-backed case needs only a
file re-read with honest staleness detection — not per-node text retention. Per-node
retention is required only for `parse_ast` over a string and for tables that outlive
their files, and can follow later.

Measured caveat for 4b, as recorded before part 1 landed: byte-identity under
`source := 'full'` did **not** hold — it failed for all 26 corpus languages, on
inter-token whitespace only, because the unparse macros take no `source` parameter
and never read the position columns. **That gap is now closed for an unmodified
tree** (part 1 below); it remains open for a modified one (part 2).
