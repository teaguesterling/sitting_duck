# Feature: Analysis Helpers v2

**Priority:** Medium
**Complexity:** Mixed
**Status:** Proposed

## Summary

Additional helper functions identified during self-analysis of sitting_duck's codebase.

---

## 1. `ast_call_arguments(table, call_node_id)` → TABLE ✅

**Priority:** P1
**Complexity:** Low (SQL macro)
**Status:** Implemented in d14ed52

Extract arguments from a function call node.

### Problem

Currently requires regex on `peek` to extract call arguments:
```sql
regexp_extract(peek, 'RegisterFunction\(([^)]+)\)', 1)
```

### Proposed Solution

```sql
SELECT * FROM ast_call_arguments('my_ast', 42);
-- Returns:
-- arg_position | arg_node_id | arg_name | arg_type | arg_peek
-- 0            | 43          | func     | identifier | some_func
-- 1            | 44          | NULL     | string_literal | "hello"
```

### Implementation Notes

Use `ast_children` to get children of the call node, filter to argument_list, then get its children. The argument nodes are direct children of the arguments/argument_list node.

---

## 2. `read_lines` with `file_path` column ✅

**Priority:** P1
**Complexity:** Low (SQL macro change)
**Status:** Implemented in d14ed52

### Problem

`read_lines()` doesn't return the file path, requiring manual hardcoding:
```sql
SELECT line, 'file_a.sql' as file FROM read_lines('file_a.sql')
UNION ALL
SELECT line, 'file_b.sql' as file FROM read_lines('file_b.sql')
```

### Proposed Solution

Add `file_path` column to match `read_ast` convention:
```sql
SELECT line_number, line, file_path FROM read_lines('src/**/*.sql');
```

### Standardization

Use `file_path` (not `filename`) to match:
- `read_ast` output column
- All structural analysis macros
- Consistent with full path semantics

Update all file utility macros:
- `read_lines(path)` → add `file_path` column
- `read_lines_range(path, start, end)` → add `file_path` column
- `read_lines_context(path, center, context)` → add `file_path` column

---

## 3. Tree Pattern Matching

**Priority:** P2
**Complexity:** High
**Status:** Needs Design

### Problem

Finding structural patterns currently requires:
- `peek LIKE '%pattern%'` (text-based, fragile)
- Complex CTEs with parent/child joins

### Vision

Match AST subtree patterns, not just text:

```sql
-- Find: function calls where first argument is a string literal
SELECT * FROM ast_match_pattern('my_ast',
    pattern := 'call[argument_list > string_literal:first]'
);

-- Find: if statements with else clause containing return
SELECT * FROM ast_match_pattern('my_ast',
    pattern := 'if_statement[else_clause > return_statement]'
);
```

### Design Questions

1. **Pattern syntax:** XPath-like? CSS selector-like? Custom DSL?
2. **Matching semantics:** First match? All matches? With captures?
3. **Performance:** Can we leverage `descendant_count` for pruning?
4. **Composability:** How do patterns interact with predicates?

### Possible Approaches

1. **Tree-sitter queries:** Leverage tree-sitter's native query syntax (S-expression based)
2. **SQL-native:** Express patterns as nested CTEs (verbose but familiar)
3. **Custom DSL:** Design a pattern language optimized for our use case
4. **Hybrid:** Simple patterns in SQL, complex patterns via tree-sitter

### Examples of Desired Patterns

```
# Direct child
call > identifier

# Descendant (any depth)
function_definition >> return_statement

# With predicate
call[name='eval']

# Sibling
if_statement + else_clause

# First/last child
argument_list > *:first
argument_list > *:last

# Negation
function_definition:not(>> try_statement)
```

**Recommendation:** Defer detailed design until we have more use cases. Consider leveraging tree-sitter's query language if available.

---

## 4. `ast_definitions(table)` → TABLE

**Priority:** P2
**Complexity:** Low (SQL macro)

### Problem

No single macro to get all definitions with metadata. Currently must filter manually:
```sql
SELECT * FROM my_ast WHERE is_definition(semantic_type);
```

### Proposed Solution

```sql
SELECT * FROM ast_definitions('my_ast');
-- Returns:
-- name | definition_type | language | file_path | start_line | end_line | node_id
-- foo  | function        | python   | src/a.py  | 10         | 25       | 42
-- Bar  | class           | python   | src/a.py  | 30         | 100      | 150
-- x    | variable        | python   | src/a.py  | 5          | 5        | 10
```

### Output Columns

| Column | Description |
|--------|-------------|
| `name` | Definition name |
| `definition_type` | `function`, `class`, `variable`, `module`, `type` |
| `language` | Source language |
| `file_path` | Source file |
| `start_line`, `end_line` | Location |
| `node_id` | For joining back to AST |
| `type` | Original AST node type |

### Implementation

```sql
CREATE OR REPLACE MACRO ast_definitions(ast_table) AS TABLE
    SELECT
        name,
        CASE
            WHEN is_function_definition(semantic_type) THEN 'function'
            WHEN is_class_definition(semantic_type) THEN 'class'
            WHEN is_variable_definition(semantic_type) THEN 'variable'
            WHEN is_module_definition(semantic_type) THEN 'module'
            WHEN is_type_definition(semantic_type) THEN 'type'
            ELSE 'other'
        END as definition_type,
        language,
        file_path,
        start_line,
        end_line,
        node_id,
        type
    FROM query_table(ast_table)
    WHERE is_definition(semantic_type)
      AND name IS NOT NULL AND name != ''
    ORDER BY file_path, start_line;
```

---

## 5. `ast_peek_contains_any(peek, patterns)` → BOOLEAN

**Priority:** P3
**Complexity:** Low (C++ scalar function)

### Problem

Multi-pattern peek matching is verbose:
```sql
WHERE peek LIKE '%eval%' OR peek LIKE '%exec%' OR peek LIKE '%system%'
```

### Proposed Solution

```sql
WHERE ast_peek_contains_any(peek, ['eval', 'exec', 'system'])
```

### Implementation Notes

- C++ scalar function for performance
- Case-sensitive by default, optional `case_insensitive` parameter
- Returns true if peek contains ANY of the patterns

### Alternative

Could be a general-purpose `string_contains_any(str, patterns)` utility function, not AST-specific.

---

## Implementation Order

1. **P1:** `read_lines` file_path column (quick fix)
2. **P1:** `ast_call_arguments` (useful for introspection)
3. **P2:** `ast_definitions` (complements existing macros)
4. **P3:** `ast_peek_contains_any` (nice-to-have)
5. **Future:** Tree pattern matching (needs design)

---

## Related Work

- Structural Analysis Macros (implemented)
- Security audit uses peek-based pattern matching
- Dead code detection uses definition enumeration
