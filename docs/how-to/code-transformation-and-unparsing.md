# How to Transform and Unparse Code with SQL

This guide shows how to combine DuckDB's relational power with Sitting Duck's `read_ast()`, `parse_ast()`, and `ast_unparse()` to parse, transform, and regenerate source code.

---

## The Parse-Transform-Unparse Workflow

1. **Parse:** Load source code files or snippets into relational AST tables.
2. **Transform:** Apply SQL expressions, CTEs, window functions, and taxonomy predicates to modify node attributes (names, types, structure).
3. **Unparse:** Feed the modified AST table to `ast_unparse()` to generate syntactically valid code.

```
Source Code ──> parse_ast() / read_ast() ──> Relational AST Table
                                                    │
                                             SQL Transformations
                                             (RENAME, REWRITE, FILTER)
                                                    │
                                                    ▼
Reconstructed Code <── ast_unparse() <── Transformed AST Table
```

---

## Example 1: Refactoring / Renaming Identifiers

Suppose you want to rename a deprecated function across a codebase.

```sql
-- Original Python source code
WITH parsed AS (
    SELECT * FROM parse_ast('
def fetch_user_data(user_id):
    endpoint = f"/users/{user_id}"
    return request_api(endpoint)

user = fetch_user_data(42)
', 'python')
),
-- Transform: Replace the identifier name 'fetch_user_data' with 'get_user_by_id'
transformed AS (
    SELECT
        node_id,
        parent_id,
        type,
        CASE
            WHEN name = 'fetch_user_data' THEN 'get_user_by_id'
            ELSE name
        END AS name,
        semantic_type,
        flags,
        file_path,
        language,
        start_line,
        end_line,
        depth,
        sibling_index,
        children_count,
        descendant_count,
        peek
    FROM parsed
)
-- Unparse: Regenerate valid Python code
SELECT source AS refactored_code FROM ast_unparse_from('transformed');
```

---

## Example 2: Inspecting and Customizing Formatting Rules

Sitting Duck unparses according to rules discovered via `ast_unparse_rules()`.

```sql
-- Check active indentation and line break rules for Go
SELECT rule_kind, node_type, int_arg
FROM ast_unparse_rules('go')
WHERE rule_kind IN ('INDENT_CHILDREN', 'BREAK_BEFORE', 'EMPTY_LINES_BEFORE');
```

Indentation and line endings come from the rule set, and you choose a rule set with
a `preset` argument rather than with per-call formatting options. The presets are
`pep8` / `black`, `gofmt`, `prettier`, `llvm`, `google` and `rustfmt`:

```sql
-- Default rules indent with four spaces
SELECT source FROM ast_unparse_code(E'func f() int {\nreturn 1\n}\n', 'go');

-- gofmt indents with a hard tab instead
SELECT source FROM ast_unparse_code(E'func f() int {\nreturn 1\n}\n', 'go', 'gofmt');
```

A preset *is* a rule set, so you can see exactly which rules it changes:

```sql
SELECT rule, target, int_arg FROM ast_unparse_rules('python', 'black')
EXCEPT
SELECT rule, target, int_arg FROM ast_unparse_rules('python', '');
-- INDENT_STRING | * | 0
```

For anything no preset covers, pass your own rules table to `ast_unparse_custom()`.

---

## Example 3: Verifying the Roundtrip Guarantee

Sitting Duck's unparser satisfies the **pseudo-identity property**:

```
parse(unparse(parse(S)))  ==  parse(S)
```

You can test this property directly in SQL. The unparsed text has to reach
`parse_ast()` through a variable rather than a CTE, because a table function
argument cannot be a subquery (`Binder Error: Table function cannot contain
subqueries`):

```sql
CREATE OR REPLACE TABLE original AS
    SELECT * FROM parse_ast('def multiply(x, y): return x * y', 'python');

SET VARIABLE roundtrip_code = (SELECT source FROM ast_unparse_from('original'));

CREATE OR REPLACE TABLE reparsed AS
    SELECT * FROM parse_ast(getvariable('roundtrip_code'), 'python');

-- Verify that node structure, types, and identifiers match exactly.
-- The count is the assertion: 0 means the property held.
SELECT
    (SELECT count(*) FROM original)  AS original_nodes,
    (SELECT count(*) FROM reparsed)  AS roundtrip_nodes,
    (SELECT count(*)
       FROM original o FULL OUTER JOIN reparsed r ON o.node_id = r.node_id
      WHERE o.type IS DISTINCT FROM r.type
         OR o.name IS DISTINCT FROM r.name) AS mismatches;
-- original_nodes | roundtrip_nodes | mismatches
--             18 |              18 |          0
```

---

## See Also

- [Unparse Reference Guide](../reference/unparse.md) - Complete reference for unparse parameters and rules
- [AST Output Schema](../reference/output-schema.md) - Column specifications for AST tables
- [Common Queries](common-queries.md) - Common SQL queries for code inspection
