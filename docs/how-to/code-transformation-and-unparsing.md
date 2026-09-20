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
SELECT ast_unparse(transformed) AS refactored_code;
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

You can customize the indentation width or line endings directly in `ast_unparse()`:

```sql
-- Unparse with 2-space indentation (e.g. for JavaScript/JSON/YAML)
SELECT ast_unparse(parsed_ast, indent_size := 2);

-- Unparse with hard tabs (e.g. for Go or Makefile)
SELECT ast_unparse(parsed_ast, use_tabs := true);

-- Unparse with CRLF Windows newlines
SELECT ast_unparse(parsed_ast, newline := '\r\n');
```

---

## Example 3: Verifying the Roundtrip Guarantee

Sitting Duck's unparser satisfies the **pseudo-identity property**:

$$\text{parse}(\text{unparse}(\text{parse}(S))) \equiv \text{parse}(S)$$

You can test this property directly in SQL:

```sql
WITH original AS (
    SELECT * FROM parse_ast('def multiply(x, y): return x * y', 'python')
),
unparsed AS (
    SELECT ast_unparse(original) AS code
),
reparsed AS (
    SELECT * FROM parse_ast((SELECT code FROM unparsed), 'python')
)
-- Verify that node structure, types, and identifiers match exactly
SELECT
    o.node_id,
    o.type AS original_type,
    r.type AS roundtrip_type,
    o.name AS original_name,
    r.name AS roundtrip_name
FROM original o
FULL OUTER JOIN reparsed r ON o.node_id = r.node_id;
```

---

## See Also

- [Unparse Reference Guide](../reference/unparse.md) - Complete reference for unparse parameters and rules
- [AST Output Schema](../reference/output-schema.md) - Column specifications for AST tables
- [Common Queries](common-queries.md) - Common SQL queries for code inspection
