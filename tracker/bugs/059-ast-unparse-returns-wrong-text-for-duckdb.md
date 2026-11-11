# 059 — the rules-based unparse returns plausible-but-wrong text for a `duckdb` AST instead of raising

**Status:** Not fixed. Measured, pinned as a KNOWN DEFECT, and deliberately not
fixed inside M2 stage 3 task 7 — see "Why it is not fixed here". Ships to the
public `duckdb/community-extensions` registry with the stage.
**Found by:** M2 stage 3 task 7, which the plan sent looking for exactly this.
The stage's Review Focus item 5 asked whether the rules-based unparse raises or
returns text over a language with no unparse rules, and said not to assume
either answer. It returns text. Every figure below was measured against
`build/release/duckdb` at `ca0b7fd`.
**Related:** `tracker/bugs/043` (duckdb nodes report no source location) is the
same adapter and a different symptom; it is cited here only as the neighbouring
duckdb-adapter defect, never as this one's home. `#89` (silent-empty) is the
PRECEDENT for the shape of the fix — a loud failure rather than an empty or
wrong answer — exactly as `ast_unparse_exact` already implements it three
macros away in the same file.

## What is wrong

`ast_unparse_from`, `ast_unparse` and `ast_unparse_code` all rebuild source text
by walking a node table and applying spacing rules from
`ast_unparse_rules(<language>)`. Handed a `duckdb` AST they produce a row of
text that looks like source and is not:

    input   SELECT a, b FROM t WHERE a = 1
    output  a b t a 1

Every keyword, punctuation mark and operator is gone. The caller gets no error,
no warning and no diagnostic column — one row, one plausible-looking string.

Two independent reasons the `duckdb` adapter cannot be served by this path, both
already written down in the source:

- `src/unparse_rules.cpp` holds 53 rule-map entries and **neither `duckdb` nor
  `sql` is among them**, so there are no language rules to apply.
- The adapter's tree is an **AST, not a CST**: it has no keyword, punctuation or
  comment nodes at all, so even a complete rule set could not reconstruct the
  text. `src/sql_macros/ast_unparse.sql:502-515` says this, under a heading that
  reads "NOT SERVED: the `duckdb` language adapter".

So the information needed to produce the right answer is absent, and the macro
answers anyway.

## The measurement

`ast_unparse_rules('duckdb')` returns the universal rules and nothing else:

| call | rows | tagged `*` | tagged with a language |
|---|---|---|---|
| `ast_unparse_rules('duckdb')` | 15 | 15 | **0** |
| `ast_unparse_rules('python')` | 23 | 15 | 8 |

(The 15 are the `TIGHT_BEFORE`/`TIGHT_AFTER` rules for `, ; : ) ] } ( [ { . ->
::`. The stage's plan said 6; that was wrong and is corrected in the plan.)

**All the rules-based bodies are affected, not just one.** The three carry their
own ~100 lines each — `ast_unparse_from` at `ast_unparse.sql:16`, `ast_unparse`
at `:114`, `ast_unparse_code` at `:212` — so this had to be measured per body
rather than inferred from one:

| body | over a `duckdb` AST | result |
|---|---|---|
| `ast_unparse_from('<table>')` | 1 row | `a b t a 1` |
| `ast_unparse_code('SELECT a, b FROM t WHERE a = 1','duckdb')` | 1 row | `a b t a 1` |
| `ast_unparse('<path>')` | not reachable | `duckdb` claims no file extension and this macro has no `language :=` parameter, so no path resolves to it |

`ast_unparse_custom(ast_table, rules_table)` shares the same walk and so has the
same exposure for any rules table that does not cover the language.

**The contrast that makes this a defect rather than a limitation.** The
byte-exact family, in the same file, refuses the same input by design:

    SELECT * FROM ast_unparse_exact('test/data/sql/example.sql','duckdb');
    -> Invalid Input Error: ast_unparse_exact: every node reports
       start_byte = end_byte = 0, so this parser surfaces no byte positions and
       there is nothing to slice. ... Parse with a tree-sitter language.

Same adapter, same absent information, opposite behaviour. One says so; the
other guesses.

## The contract it violates

- `tracker/features/044-v2-m1-contract.md:16` — the conformance kit requires
  "raw-AST-join agreement, and **no silent-empty (#89)**". This is worse than a
  silent empty: an empty result at least cannot be mistaken for an answer.
- `tracker/roadmap/m2-stage3-macro-split-plan.md:60` — "The #89 principle makes
  0 rows mean 'searched correctly, not there'." Text that is not the source
  means neither.
- `ast_unparse_exact`'s own refusal, quoted above, is the house standard for
  this exact input. The rules-based path does not meet the standard its sibling
  sets.

## Why it is not fixed here

M2 stage 3 is constrained to **no behaviour change** for the existing macros,
and this fix is nothing but a behaviour change: a call that returns a row today
would raise. It also touches the rules-based unparse path shared by four
macros, which is outside task 7's scope (a test task) and outside the stage's.

The task that found it was explicitly told to pin it and not fix it.

## The intended shape: loud failure

Follow `ast_unparse_exact`, which solved this class for its own surface:

1. **Detect the condition from the rules, not from the language name.** A
   language with zero rows tagged with its own name in `ast_unparse_rules()` has
   no rule set, whatever it is called. Keying on the string `'duckdb'` would
   leave `sql` — also absent from the 53 — and any future native adapter
   silently broken in the same way.
2. **Raise, naming the language and the reason**, in the register
   `ast_unparse.sql:737` already uses: that this adapter's tree carries no
   keyword, punctuation or comment nodes, so no spacing rule can reconstruct
   the text, and that a tree-sitter language is needed.
3. **Decide the empty-input case deliberately.** A legitimately tiny parse of a
   served language must not be caught by the same guard. `ast_unparse_exact`'s
   own guard carries an `n_rows > 1` conjunct for precisely this, with a comment
   explaining it; the same care is needed here.

A guard that only checks "are there language rules" is probably enough, and is
certainly cheaper than teaching the duckdb adapter to emit a CST.

## Tests

`test/sql/core_alone.test` section 9 already holds the red-in-waiting. It pins

    1   a b t a 1

labelled a KNOWN DEFECT carrying this id, and the comment above it states that
the assertion is MEANT to become a `statement error` when the fix lands. That
file's header rule — "a pin that moves is a finding to investigate, never a
value to update" — does **not** apply to that block, and the comment says so.

New tests the fix needs:

1. The section 9 pin becomes `statement error` with the new message.
2. The same input through `ast_unparse_code`, which is a second body with the
   same defect and is currently pinned in section 6 at
   `1  5324034c8399fdeabcd8973cdead1ac1`. That pin moves too.
3. A served language must be unaffected: `ast_unparse_from` over the python
   fixture is pinned in section 6 at `1  f56867bdb1fefc8835a13034bef15a33` and
   must NOT move. That is the control which proves the guard is scoped.
4. A one-node parse of a served language, so the empty-input conjunct of point 3
   above has a witness.

## Cost

A macro-body change in a core `.sql` file, so: regenerate
`src/include/embedded_sql_macros.hpp`, a full extension rebuild, and the unparse
suites plus `test/sql/core_alone.test` (190 assertions). The boundary gate must
stay at 105 macros / 69 edges / 0 violations; the edge count moves only if the
guard calls another macro.
