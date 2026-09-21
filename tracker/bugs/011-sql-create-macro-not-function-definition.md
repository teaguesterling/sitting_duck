# Bug #011: SQL adapter does not classify CREATE MACRO as a function definition

**Status:** Open
**Priority:** P2
**Found:** 2026-07-23, while validating a cross-source provenance join (sessions × AST × git blame)

## Symptom

`src/sql_macros/css_selectors.sql` parses to 10,896 nodes, but zero of them satisfy
`is_function_definition(semantic_type)`:

```sql
SELECT count(*) FILTER (WHERE is_function_definition(semantic_type)) AS func_defs,
       count(*) AS total_nodes
FROM read_ast('src/sql_macros/css_selectors.sql');
-- func_defs = 0, total_nodes = 10896
```

Every `CREATE OR REPLACE MACRO` in the file should surface as a named
`DEFINITION_FUNCTION` node (the file defines dozens of macros). C++ files in the
same query correctly return named `function_definition` nodes, so the join logic
is fine — the SQL/DuckDB adapter's semantic mapping is the gap.

## Impact

Any cross-language analysis keyed on `is_function_definition` silently omits all
SQL macro definitions — e.g. function-level attribution joins can attribute edits
in `.cpp` files but not in `src/sql_macros/*.sql`. Given the extension's own macro
layer is written in SQL, self-analysis (dogfooding, #021) undercounts.

Note PR #50 covered SQL `CREATE FUNCTION` name extraction; `CREATE MACRO`
(and `CREATE OR REPLACE MACRO ... AS TABLE`) appears not to be covered by the
same mapping.

## Expected

- `CREATE [OR REPLACE] MACRO name(...) AS ...` → semantic_type DEFINITION_FUNCTION, name populated
- Table macros (`AS TABLE`) included
- Conformance-kit check candidate for the v2 module contract ("call nodes are
  named" has a sibling: "definition nodes are classified")
