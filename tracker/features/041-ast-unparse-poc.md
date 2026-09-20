# 041 — AST unparse (reconstruct source from the tree, no `peek`)

**Status:** Completed and productized (2026-09-20).
**Goal:** Reconstruct source text from `read_ast`/`parse_ast` rows, *without* `peek`,
with declarative per-language behavior configured in `.def` files and unparse macros.

## Why not `peek`
`peek` is a truncated preview (first N chars) — lossy on long tokens (big strings,
long identifiers). Unparse must be exact at the leaf level, so it cannot use it.

## Feasibility findings (this build)
- Only text-bearing columns are `name` and `peek`. There is **no full source-text
  column**, and **no `start_column`/`end_column`** in the default view — they appear
  only with `source := 'full'` (see #100).
- **Leaf-text rule (no peek):** `COALESCE(NULLIF(name,''), type)`
  - tree-sitter *anonymous* tokens: `type` **is** the literal (`def`, `(`, `+`, `:`),
    `name` empty. Emit `type`. (free — no `.def` work)
  - *named* leaves: `name` = full, untruncated text — identifiers, and literals whose
    name strategy is `NODE_TEXT` (e.g. `integer` → `name='1'`, `string_content`).
- `node_id` is document order, so ordering leaves by `node_id` = the token stream.
- Leaves are `children_count = 0`.

## Working prototype (SQL)

### v0 — leaf-concat, line-grouped (semantically faithful, whitespace-normalized)
```sql
WITH leaves AS (
  SELECT node_id, start_line, COALESCE(NULLIF(name,''), type) AS tok
  FROM parse_ast(?, ?) WHERE children_count = 0
),
per_line AS (
  SELECT start_line, string_agg(tok, ' ' ORDER BY node_id) AS line_text
  FROM leaves GROUP BY start_line
)
SELECT string_agg(line_text, chr(10) ORDER BY start_line) FROM per_line;
-- 'def add(a, b):\n    return a + b'  ->  'def add ( a , b ) :\nreturn a + b'
```

### v1 — + language spacing rules (near-exact)
```sql
WITH leaves AS (
  SELECT node_id, start_line, COALESCE(NULLIF(name,''),type) AS tok,
         row_number() OVER (ORDER BY node_id) AS rn
  FROM parse_ast(?, ?) WHERE children_count = 0
),
seq AS (
  SELECT *, LAG(tok) OVER (ORDER BY rn) AS ptok,
            LAG(end_line) OVER (ORDER BY rn) AS p_end_line  -- END line: correct across multi-line leaves
  FROM leaves
)
SELECT string_agg(
  CASE
    WHEN ptok IS NULL                      THEN ''
    WHEN start_line > p_end_line           THEN repeat(chr(10), start_line - p_end_line)  -- preserve blank lines
    WHEN tok  IN (',',';',':',')',']','}') THEN ''   -- no space BEFORE (lang rule)
    WHEN ptok IN ('(','[','{','.')         THEN ''   -- no space AFTER  (lang rule)
    WHEN tok = '.'                         THEN ''
    ELSE ' '
  END || tok, '' ORDER BY rn)
FROM seq;   -- (needs end_line in the leaves CTE)
-- -> 'def add (a, b):\nreturn a + b'   (only 'add (' imperfect; see nuance below)

-- NEWLINE RULE, stated plainly:
--   Whitespace/newlines are NOT nodes — tree-sitter emits them as the gap between
--   nodes (Python NEWLINE/INDENT/DEDENT are hidden extras, absent from read_ast).
--   So a newline is INFERRED from position, never read: emit one when a leaf starts
--   on a later line than the previous leaf ENDED on. Compare against prev.end_line
--   (not start_line) so a multi-line string/comment doesn't trigger a spurious break;
--   emit (start_line - prev_end_line) newlines to keep blank lines.
--   Limit: leading INDENTATION is not in line numbers — it needs start_column
--   (source:='full'). Line-granularity places breaks; only columns size the gaps.
```

## Per-language `.def` instantiation path (increasing fidelity)
1. **Leaf-text coverage (required for correctness).** Every *named* leaf that is not
   self-describing needs a `NODE_TEXT` name strategy so its text is recoverable.
   - Confirmed gap: `comment` currently has no name → unparse emits the literal string
     `comment`. Fix = `NODE_TEXT` on `comment` (and any similar text-bearing leaf).
     Audit per language: comments, numeric/other literals, operators-as-named-nodes.
   - Anonymous tokens need nothing (`type` == text).
2. **Spacing / tight-token rules (fidelity).** A per-language set of "no space
   before / no space after" tokens, plus the keyword-vs-identifier-before-`(` nuance
   (`if (x)` keeps the space; `add(x)` does not). Home options: a new `DEF_SPACING`
   construct, a flag, or a companion rules table seeded from `.def`.
3. **(Future) node templates.** Per-node-type unparse templates enable transformation
   and pretty-printing (emit a *modified* tree, exact indentation). Heavier; not needed
   for a round-trip PoC.

## Byte-exact path
With `source := 'full'`, `start_column`/`end_column` are populated. Inter-token gaps
(`this.start_col - prev.end_col`, line deltas) reconstruct exact whitespace and
indentation → faithful round-trip, still without `peek`. The normalized PoC above is
the column-free fallback.

## v2 — `ast_unparse(path)` + indentation, and PSEUDO-IDENTITY (validated)

Prototype macro: `tracker/features/ast_unparse.prototype.sql` (`ast_unparse(path)`).

**The correctness bar is not byte-exactness — it is pseudo-identity:**
`parse(S)` and `parse(unparse(parse(S)))` must be the **same tree** (formatting may
differ). This is the right bar for a *rules-based* unparser (and the only bar that
still holds once the tree is transformed — which is the whole point of unparse).

**Indentation rule (needed for off-side languages).** Whitespace-insensitive
languages (JS/C/Go) round-trip on token-spacing alone. Indentation-sensitive ones
(Python) do NOT: without indentation the re-parse pulls trailing statements into the
block (verified: `function_definition` descendant_count 17 → 7). Fix, purely from the
tree (no offsets): after a newline, prefix `indent × 4 spaces`, where
`indent(leaf) = # of block ancestors`. Ancestry is computed WITHOUT recursion using the
contiguous-descendant-range invariant: `A` is an ancestor of `L` iff
`A.node_id <= L.node_id <= A.node_id + A.descendant_count`. The per-language "block
type" (`block` for Python, `statement_block` for JS/TS, `compound_statement` for C/C++,
`block` for Go/Java) is the layout knob that belongs in `.def`.

**Pseudo-identity harness** (write S → `ast_unparse` → write U → compare full
pre-order `type` streams of `read_ast(S)` vs `read_ast(U)`):

| language | result |
|---|---|
| Python (nested if inside def, trailing stmt) | PSEUDO-IDENTITY ✓ |
| JavaScript (function + block) | ✓ |
| C (function + compound stmt) | ✓ |
| Go (package + func) | ✓ |

So the strategy question resolves: **rules ARE the right strategy** — token-spacing
rules for brace languages, plus indentation-from-nesting for off-side languages. Byte
offsets (`source:='full'`) would round-trip an *unmodified* tree but can't format a
*modified* one, so rules win for a real unparser. `.def` carries two per-language knobs:
(1) leaf-text coverage (NODE_TEXT on text leaves like `comment`), (2) layout — block
type(s) for indent + the tight-token set.

## Open questions for direction
- Ship as a macro `ast_unparse(path)` (v1 spacing) now, or keep exploratory?
- Which languages to seed first for the leaf-text + spacing audit (python? the reporter's C++/JS)?
- Byte-exact (source:='full') in scope, or is normalized reconstruction the target?
