# Node Type Selectors

Sitting Duck offers three levels of type specificity for matching AST nodes. Choose the right level depending on whether you need cross-language portability or tree-sitter precision.

## Three Tiers of Type Selectors

```sql
-- These are different:
.if                -- ALL conditionals (if + elif + else + switch + case + match)
if                 -- just if and if_* variants
if_statement       -- exactly if_statement (most specific)
```

### Dotted selectors (`.semantic`) — Cross-language, broadest

Match by semantic category. Works identically across all 27 languages:

```sql
.func              -- all function definitions (any language)
.if                -- all conditionals: if, elif, else, switch, case, match
.loop              -- all loops: for, while, do-while
.call              -- all function/method calls
.import            -- all import statements
```

`.if` includes elif, else, switch, case — the entire conditional family. Both `.func` and `.FUNC` work (case-insensitive).

See [Semantic Type Aliases](semantic-aliases.md) for the full alias table (~80 aliases).

### Bare keywords — Language-specific, medium

Match by tree-sitter node type prefix. `if` matches `if` (the keyword token) AND `if_statement`, `if_clause`, etc.:

```sql
if                 -- if keyword + if_statement + if_clause
return             -- return keyword + return_statement
for                -- for keyword + for_statement + for_in_clause
class              -- class keyword + class_definition
```

Bare keywords are narrower than dotted selectors but broader than exact types.

### Exact types — Tree-sitter specific, narrowest

Match the exact tree-sitter node type name:

```sql
function_definition       -- exactly this type, nothing else
method_definition         -- exactly this type
class_declaration         -- exactly this type (JS/TS)
```

## Discovering Node Types

Use `ast_type_map()` to explore what node types are available and how they map to semantic categories:

```sql
-- All Python function-related types
SELECT node_type, semantic_type, name_role, is_scope
FROM ast_type_map('python')
WHERE kind = 'definition';
```

```
assignment                DEFINITION_VARIABLE   definition   false
async_function_definition DEFINITION_FUNCTION   definition   true
class_definition          DEFINITION_CLASS      definition   true
function_definition       DEFINITION_FUNCTION   definition   true
lambda                    DEFINITION_FUNCTION   definition   true
...
```

```sql
-- What does `for_statement` map to across languages?
SELECT language, semantic_type, is_scope
FROM ast_type_map()
WHERE node_type = 'for_statement'
ORDER BY language;
```

```sql
-- All node types that create scopes
SELECT language, node_type, semantic_type
FROM ast_type_map()
WHERE is_scope
ORDER BY language, node_type;
```

```sql
-- All reference-type nodes in JavaScript
SELECT node_type, semantic_type
FROM ast_type_map('javascript')
WHERE name_role = 'reference';
```

---

## See Also

- [Semantic Type Aliases](semantic-aliases.md) — Full alias table for `.semantic` selectors
- [CSS Selectors Overview](css-selectors.md) — Combinators, compound selectors, API reference
- [Pseudo-Classes Reference](css-pseudo-classes.md) — Filter by structure, position, scope, and more
