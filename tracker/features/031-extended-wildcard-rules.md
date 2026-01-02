# Extended Wildcard Rules for Pattern Matching

## Overview

Extend the `%__X<rules>__%` syntax with additional constraints for more powerful pattern matching.

## Current Implementation

| Rule | Example | Status |
|------|---------|--------|
| `*` | `%__<*>__%` | Implemented |
| `+` | `%__<+>__%` | Implemented |
| `type=T` | `%__X<type=T>__%` | Documented, needs verification |

## Proposed Rules

### Tier 1: Easy (filter on existing columns)

These can be implemented in SQL by adding conditions to the pattern matching CTEs.

| Rule | Example | Use Case |
|------|---------|----------|
| `type=T` | `%__X<type=identifier>__%` | Match specific AST type |
| `types=T1,T2` | `%__X<types=integer,float>__%` | Match multiple types |
| `not_type=T` | `%__X<not_type=comment>__%` | Exclude type |
| `descendants<N` | `%__X<descendants<10>__%` | Simple expressions only |
| `descendants>N` | `%__X<descendants>50>__%` | Complex nodes only |
| `children=N` | `%__X<children=2>__%` | Exact child count |
| `name=V` | `%__X<name=self>__%` | Exact name match |
| `name~P` | `%__X<name~^_>__%` | Name matches regex |
| `semantic=K` | `%__X<semantic=LITERAL>__%` | Cross-language semantic kind |

**Implementation approach:** Parse rules in `extract_wildcard_rules`, pass to pattern CTE, add WHERE conditions.

### Tier 2: Medium (require enhanced tracking)

| Rule | Example | Use Case |
|------|---------|----------|
| `depth<N` | `%__X<*,depth<3>__%` | Limit variadic depth reach |
| `depth>N` | `%__X<*,depth>1>__%` | Skip shallow matches |
| `sibling<N` | `%__X<sibling<3>__%` | Position constraints |

**Implementation approach:** Track relative depth/position in capture phase.

### Tier 3: Hard (require tree traversal)

These require checking descendants, which is expensive in SQL but natural in C++.

| Rule | Example | Use Case |
|------|---------|----------|
| `contains=T` | `%__X<contains=return_statement>__%` | Node has descendant of type |
| `contains=T1,T2` | `%__X<contains=try,finally>__%` | Multiple required descendants |
| `not_contains=T` | `%__X<not_contains=raise>__%` | Node lacks descendant type |
| `**` | `%__X<**>__%` | Recursive any-depth matching |

**Implementation approach:** Best done in C++ with tree traversal. SQL version possible but O(n) per node.

## Example Patterns with New Rules

### Security: Functions without error handling
```sql
-- Functions that don't contain try statements
SELECT * FROM ast_match('code',
    'def __F__(__): %__BODY<*,not_contains=try_statement>__%',
    'python');
```

### Complexity: Simple functions only
```sql
-- Functions with less than 20 AST nodes
SELECT * FROM ast_match('code',
    'def __F__(__): %__BODY<*,descendants<20>__%',
    'python');
```

### Structure: Classes with constructors
```sql
-- Python classes that have __init__
SELECT * FROM ast_match('code',
    'class __C__: %__<*,contains=__init__>__%',
    'python');
```

### Cross-language: Any literal argument
```sql
-- Function calls with literal arguments
SELECT * FROM ast_match('code',
    '__F__(%__X<semantic=LITERAL>__%)',
    'python',
    match_by := 'semantic_type');
```

## Syntax Design

### Rule Combining
Multiple rules separated by commas:
```
%__X<*,type=identifier,descendants<10>__%
```

### Comparison Operators
- `=` exact match: `type=identifier`
- `~` regex match: `name~^test_`
- `<` less than: `descendants<10`
- `>` greater than: `descendants>5`
- `<=`, `>=` also supported

### Negation
Prefix with `not_`:
- `not_type=comment`
- `not_contains=raise`

## Implementation Roadmap

### Phase 1: Tier 1 Rules (SQL)
1. Implement rule parsing in `extract_wildcard_rules`
2. Add `type=T` filtering to pattern matching
3. Add `descendants<N` filtering
4. Add `name=V` and `name~P` filtering
5. Add `semantic=K` filtering

### Phase 2: Tier 2 Rules (SQL)
1. Track relative depth in variadic matching
2. Implement `depth<N` constraint
3. Implement `sibling<N` constraint

### Phase 3: Tier 3 Rules (C++)
1. Design C++ pattern matching infrastructure
2. Implement `contains=T` with tree traversal
3. Implement `**` recursive matching
4. Optimize for large codebases

## Open Questions

1. **Syntax for OR logic:** `types=T1,T2` vs `type=T1|T2`?
2. **Case sensitivity:** Should `name=foo` match `FOO`?
3. **Regex flavor:** DuckDB's `regexp_matches` vs simpler glob?
4. **Performance:** Index hints for large-scale matching?

## Related

- Pattern matching implementation: `src/sql_macros/pattern_matching.sql`
- C++ proposal: `tracker/features/030-pattern-matching-cpp.md`
- Documentation: `docs/guide/pattern-matching.md`
