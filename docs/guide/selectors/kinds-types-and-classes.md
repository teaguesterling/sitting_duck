# Semantic Type Aliases

The `.` selector accepts ~80 aliases organized by category. These map to Sitting Duck's semantic type system, which normalizes tree-sitter node types across all 27 supported languages.

All aliases are **case-insensitive**: `.func`, `.Func`, and `.FUNC` all work.

## How the Hierarchy Works

Sitting Duck organizes node types into three levels:

1. **Kind** — The broadest category (e.g., `definition`, `control_flow`, `error_handling`)
2. **Super-type** — A subcategory within a kind (e.g., `DEFINITION_FUNCTION`, `CONTROL_CONDITIONAL`)
3. **Type** — The language-specific tree-sitter node type (e.g., `function_definition`, `if_statement`)

A `.semantic` selector matches at the super-type level by default. Kind-level aliases (like `.def`, `.flow`, `.error`) match everything in that kind.

## Definitions

| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.func` | `.fn`, `.function`, `.method` | Function/method definitions |
| `.class` | `.cls`, `.struct`, `.trait`, `.interface` | Class definitions |
| `.var` | `.variable`, `.let`, `.const` | Variable definitions |
| `.mod` | `.module`, `.package` | Module definitions |
| `.def` | `.definition` | All definitions (kind level) |

## Control Flow

| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.if` | `.cond`, `.conditional` | All conditionals |
| `.loop` | `.for`, `.while` | All loops |
| `.jump` | `.return`, `.break`, `.continue`, `.yield` | All jumps |
| `.flow` | `.control` | All control flow (kind level) |

## Error Handling

| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.try` | | Try blocks |
| `.catch` | `.except`, `.rescue` | Catch/except handlers |
| `.throw` | `.raise` | Throw/raise statements |
| `.finally` | `.ensure`, `.defer` | Finally blocks |
| `.error` | `.err` | All error handling (kind level) |

## Names and References

| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.id` | `.ident`, `.identifier` | Plain identifiers |
| `.qualified` | `.dotted` | Qualified names (module.attr) |
| `.self` | `.this` | Self/this references |
| `.call` | `.invoke` | Function/method calls |
| `.member` | `.attr`, `.field`, `.prop` | Member access |

## Literals

| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.str` | `.string` | String literals |
| `.num` | `.number` | Numeric literals |
| `.bool` | `.boolean` | Boolean literals |
| `.coll` | `.list`, `.dict`, `.array`, `.map`, `.set`, `.tuple` | Collection literals |
| `.lit` | `.literal`, `.value` | All literals (kind level) |

## Other

| Alias | Alternatives | Matches |
|-------|-------------|---------|
| `.import` | `.require`, `.use` | Import statements |
| `.export` | `.pub` | Export declarations |
| `.op` | `.operator` | All operators |
| `.arith` | `.math` | Arithmetic operators |
| `.cmp` | `.comparison` | Comparison operators |
| `.logic` | `.logical` | Logical operators |

## Discovering Mappings

Use `ast_type_map()` to see exactly which tree-sitter types map to which semantic categories for a given language:

```sql
-- What node types does .func match in Python?
SELECT node_type, semantic_type, is_scope
FROM ast_type_map('python')
WHERE semantic_type = 'DEFINITION_FUNCTION';
```

```sql
-- All semantic types available in JavaScript
SELECT DISTINCT semantic_type, kind
FROM ast_type_map('javascript')
ORDER BY kind, semantic_type;
```

---

## See Also

- [Node Type Selectors](node-types.md) — Three tiers of type specificity
- [CSS Selectors Overview](index.md) — Combinators, compound selectors, API reference
- [Pseudo-Classes Reference](pseudo-classes.md) — Filter by structure, position, scope, and more
