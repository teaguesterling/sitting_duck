# Feature: Tree-sitter Pattern Matching

**Priority:** P2
**Complexity:** Medium-High
**Status:** Design Phase

## Summary

Expose tree-sitter's native query language for structural pattern matching on AST nodes, enabling powerful pattern-based code search without complex SQL CTEs.

## Motivation

Currently, finding structural patterns requires either:
- Fragile text matching: `peek LIKE '%eval%'`
- Complex CTEs with parent/child joins

Tree-sitter has a built-in S-expression query language that's:
- Fast (compiled to efficient matchers)
- Language-aware (knows node types)
- Expressive (supports captures, predicates, quantifiers)

## Tree-sitter Query Language Overview

### Basic Syntax

```scheme
; Match a binary expression with two number literals
(binary_expression (number_literal) (number_literal))

; Match with field names
(assignment_expression
  left: (member_expression
    object: (call_expression)))

; Captures (what gets returned)
(function_definition
  name: (identifier) @func_name
  body: (block) @func_body)

; Anonymous nodes (operators, keywords)
(binary_expression operator: "!=" right: (null))

; Wildcards
(_)     ; any named node
_       ; any node (named or anonymous)

; Negated fields
(class_declaration name: (identifier) @class !type_parameters)
```

### Advanced Features

```scheme
; Quantifiers
(array (expression)+ @elements)    ; one or more
(object (pair)* @pairs)            ; zero or more
(call arguments: (_)? @arg)        ; zero or one

; Alternations
[(true) (false)] @boolean

; Anchors (adjacent nodes)
(block . (comment) @first_comment)  ; first child
(block (statement) @last .)         ; last child

; Predicates (filtering)
((identifier) @func (#eq? @func "eval"))
((string) @str (#match? @str "password"))
```

## Proposed API

### Option A: Direct Query Function

```sql
-- Basic: returns matching nodes
SELECT * FROM ast_query('my_ast', '(call name: (identifier) @name)')
ORDER BY start_line;

-- With captures: returns named capture groups
SELECT
    capture_name,
    node_type,
    node_text,
    start_line
FROM ast_query('my_ast', '
    (function_definition
        name: (identifier) @func_name
        body: (block) @func_body)
');
```

### Option B: Macro-based

```sql
-- Table macro version
SELECT * FROM ast_match('my_ast',
    pattern := '(call) @call',
    capture := 'call'
);

-- Scalar predicate version
SELECT * FROM my_ast
WHERE ast_matches(node_id, '(function_definition name: (identifier))');
```

### Option C: Query Builder (SQL-native)

```sql
-- Build patterns incrementally
SELECT * FROM ast_pattern_match('my_ast')
    .node('function_definition')
    .child('name', 'identifier')
    .capture('func_name');
```

## Recommended Approach: Option A

Direct query function is simplest and most powerful:

```sql
-- Function signature
ast_query(ast_table VARCHAR, query VARCHAR) -> TABLE

-- Output columns
| Column | Type | Description |
|--------|------|-------------|
| pattern_index | INTEGER | Which pattern matched (for multi-pattern queries) |
| capture_name | VARCHAR | Name of the capture (from @name) |
| node_id | BIGINT | Matched node's ID |
| node_type | VARCHAR | Matched node's type |
| node_text | VARCHAR | Matched node's text (peek) |
| start_line | INTEGER | Line number |
| end_line | INTEGER | End line |
| start_byte | INTEGER | Byte offset |
| end_byte | INTEGER | End byte offset |
```

## Implementation Plan

### Phase 1: Core Infrastructure

1. **Add TSQuery wrappers** to `tree_sitter_wrappers.hpp`:
```cpp
struct TSQueryDeleter {
    void operator()(TSQuery *query) const {
        if (query) ts_query_delete(query);
    }
};

struct TSQueryCursorDeleter {
    void operator()(TSQueryCursor *cursor) const {
        if (cursor) ts_query_cursor_delete(cursor);
    }
};

using TSQueryPtr = unique_ptr<TSQuery, TSQueryDeleter>;
using TSQueryCursorPtr = unique_ptr<TSQueryCursor, TSQueryCursorDeleter>;
```

2. **Create query compilation cache** (queries can be reused):
```cpp
class TSQueryCache {
    std::unordered_map<std::pair<string, string>, TSQueryPtr> cache_;
    // key: (language, query_string)
public:
    TSQuery* GetOrCompile(const TSLanguage* lang, const string& query);
};
```

### Phase 2: Table Function

3. **Create `ast_query` table function**:
```cpp
struct ASTQueryData : public TableFunctionData {
    string ast_table;
    string query_string;
    TSQueryPtr query;
    TSQueryCursorPtr cursor;
    // ... state for iteration
};

TableFunction ASTQueryFunction::GetFunction() {
    TableFunction func("ast_query",
        {LogicalType::VARCHAR, LogicalType::VARCHAR},
        Execute, Bind);
    return func;
}
```

4. **Execute query on each file's AST**:
   - Re-parse source (we don't store TSTree)
   - Run query cursor on tree
   - Yield captures as rows

### Phase 3: Integration with Existing AST Tables

5. **Add node_id mapping**:
   - Query results need to map back to our `node_id` column
   - TSNode position → find matching node in AST table

6. **Handle multi-file AST tables**:
   - Group by file_path
   - Parse each file separately
   - Run query on each tree

## Key Design Decisions

### 1. Re-parsing vs Storing Trees

**Challenge**: We flatten AST to tables; tree-sitter queries need TSTree.

**Options**:
- A) Re-parse source when running queries (simple, stateless)
- B) Store serialized TSTree alongside table (fast queries, more storage)
- C) Cache parsed trees during query execution (memory trade-off)

**Recommendation**: Start with (A), optimize to (C) later.

### 2. Query Language Exposure

**Options**:
- A) Raw tree-sitter S-expression syntax
- B) Simplified/translated syntax
- C) SQL-native pattern builder

**Recommendation**: (A) - leverage existing tree-sitter query ecosystem, documentation, and tooling. Users can use nvim/helix query files directly.

### 3. Predicate Handling

Tree-sitter predicates (`#eq?`, `#match?`) are not executed by the C library - we must implement them.

**Required predicates**:
- `#eq?` - string equality
- `#match?` - regex match
- `#any-of?` - match any in list

### 4. Cross-Language Queries

**Challenge**: Node types differ between languages.

**Options**:
- A) Queries are language-specific (user must know types)
- B) Translate queries using semantic types

**Recommendation**: (A) initially, (B) as future enhancement.

## Example Use Cases

### Security Scanning

```sql
-- Find eval/exec calls in Python
SELECT * FROM ast_query('python_code', '
    (call function: (identifier) @func
     (#any-of? @func "eval" "exec" "compile"))
');

-- Find SQL string concatenation (injection risk)
SELECT * FROM ast_query('python_code', '
    (binary_expression
        left: (string) @sql
        operator: "+"
        right: (_) @dynamic
        (#match? @sql "SELECT|INSERT|UPDATE|DELETE"))
');
```

### Code Structure Analysis

```sql
-- Find functions with try/except
SELECT * FROM ast_query('python_code', '
    (function_definition
        name: (identifier) @func_name
        body: (block (try_statement)))
');

-- Find async functions
SELECT * FROM ast_query('python_code', '
    (function_definition
        "async" @async_kw
        name: (identifier) @func_name)
');
```

### Refactoring Support

```sql
-- Find old API usage patterns
SELECT * FROM ast_query('python_code', '
    (call
        function: (attribute
            object: (identifier) @obj
            attribute: (identifier) @method)
        (#eq? @obj "client")
        (#eq? @method "get_sync"))
');
```

## Performance Considerations

1. **Query Compilation**: Cache compiled queries (they're reusable across files)
2. **Tree Re-parsing**: Parse trees are cached during single query execution
3. **Match Limit**: Support `ts_query_cursor_set_match_limit()` for large codebases
4. **Timeout**: Support query timeout via `ts_query_cursor_set_timeout_micros()`

## Dependencies

- Tree-sitter API already included via `third_party/tree-sitter`
- TSQuery API available in `tree_sitter/api.h`
- All 24 language parsers already linked

## Testing Strategy

1. **Unit tests**: Query compilation, capture extraction
2. **Language tests**: One test per language with known patterns
3. **Error handling**: Invalid queries, empty results, timeout
4. **Performance**: Large file handling, query cache effectiveness

## Open Questions

1. Should we support multiple patterns per query call?
2. How to handle query errors (syntax errors in pattern)?
3. Should captures include full subtree or just matched node?
4. Integration with `ast_definitions`, `ast_function_scope` macros?

## References

- [Tree-sitter Query Syntax](https://tree-sitter.github.io/tree-sitter/using-parsers/queries/1-syntax.html)
- [Tree-sitter Query API](https://tree-sitter.github.io/tree-sitter/using-parsers/queries/4-api.html)
- [Tree-sitter Predicates](https://tree-sitter.github.io/tree-sitter/using-parsers/queries/3-predicates-and-directives.html)

## Related Features

- #analysis-helpers-v2 §3 (original proposal)
- #structural-analysis-macros (complementary)
