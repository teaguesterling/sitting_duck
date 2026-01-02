# Extended Wildcard Rules for Pattern Matching

## Overview

Extend the `%__<...>__%` syntax using HTML/XML as an embedded DSL for wildcard constraints. This leverages tree-sitter HTML for parsing, giving us a well-defined syntax for free.

## Design Insight

Instead of inventing custom syntax that requires regex parsing, we use HTML attributes:

```sql
-- Old (custom regex parsing)
%__X<*,type=identifier,descendants<10>__%

-- New (parsed by tree-sitter HTML)
%__<X* type=identifier max-descendants=10>__%
```

Tree-sitter HTML handles:
- Unquoted attributes: `type=identifier`
- Boolean attributes: `variadic`, `negate`
- Special characters as attributes: `*`, `+`, `~`, `?`, `**`
- Nested elements for complex constraints

## Syntax Reference

### Wildcard Modifiers (Boolean Attributes)

| Modifier | Meaning | Example |
|----------|---------|---------|
| `*` | Variadic: 0+ siblings | `<BODY*>` |
| `+` | Variadic: 1+ siblings | `<ARGS+>` |
| `~` | Negate: exclude matches | `<SKIP~ type=comment>` |
| `?` | Optional: 0 or 1 | `<DOCSTRING?>` |
| `**` | Recursive: any depth | `<BODY** contains=return>` |

Modifiers can combine: `<NOISE*~>` = variadic + negate

### Anonymous Wildcards

Use `_` or omit name entirely:

```xml
%__<*>__%           <!-- anonymous variadic 0+ -->
%__<+>__%           <!-- anonymous variadic 1+ -->
%__<_ type=identifier>__%  <!-- anonymous with constraint -->
```

### Constraint Attributes

| Attribute | Example | Description |
|-----------|---------|-------------|
| `type=T` | `type=identifier` | Match specific AST type |
| `types=T1,T2` | `types=integer,float` | Match any of these types |
| `not-type=T` | `not-type=comment` | Exclude type |
| `name=V` | `name=self` | Exact name match |
| `name-pattern=P` | `name-pattern=^test_` | Name matches regex |
| `semantic=K` | `semantic=LITERAL` | Semantic type kind |
| `min-descendants=N` | `min-descendants=10` | Minimum complexity |
| `max-descendants=N` | `max-descendants=50` | Maximum complexity |
| `min-children=N` | `min-children=2` | Minimum direct children |
| `max-children=N` | `max-children=5` | Maximum direct children |
| `min-depth=N` | `min-depth=1` | Minimum relative depth |
| `max-depth=N` | `max-depth=3` | Maximum relative depth |

### Nested Constraints (Phase 2)

```xml
%__<BODY*>
  <contains type=try_statement/>
  <contains type=finally_clause/>
  <not-contains type=bare_except/>
</BODY>__%
```

### Pattern Containment (Phase 2)

```xml
%__<BODY*>
  <contains lang=python>
    if not __COND__:
        raise __ERR__
  </contains>
</BODY>__%
```

## Examples

### Simple Patterns

```sql
-- Variadic body, any content
'def __F__(__):
    %__<BODY*>__%
    return __Y__'

-- Variadic excluding comments
'def __F__(__):
    %__<BODY* not-type=comment>__%
    return __Y__'

-- Only match identifier wildcards
'__F__(%__<X type=identifier>__%)'

-- Match simple expressions (low complexity)
'__F__(%__<X max-descendants=10>__%)'
```

### Security Patterns

```sql
-- Functions with error handling
'def __F__(__):
    %__<BODY*>
      <contains type=try_statement/>
    </BODY>__%'

-- Functions WITHOUT error handling (negate)
'def __F__(__):
    %__<BODY*>
      <not-contains type=try_statement/>
    </BODY>__%'
```

### Complex Nested Patterns (Phase 2)

```sql
-- Functions with specific error handling pattern
'def __F__(__):
    %__<BODY*>
      <contains lang=python>
        try:
            __
        except __EX__:
            raise __EX__
      </contains>
    </BODY>__%'
```

## Implementation

### Phase 1: Basic HTML Wildcards

**Scope:**
- Parse `%__<...>__%` regions
- Extract tag name as wildcard name (or `_` for anonymous)
- Parse modifier attributes: `*`, `+`, `~`, `?`
- Parse simple constraint attributes: `type`, `name`, `descendants`, etc.
- Apply constraints in pattern matching CTEs

**Implementation approach:**
1. Regex extract `%__<(.+?)>__%` regions from pattern
2. For each region, parse with `parse_ast(region_content, 'html')`
3. Walk HTML AST to extract:
   - Tag name → wildcard name
   - Boolean attributes → modifiers
   - Key-value attributes → constraints
4. Build wildcard rules struct
5. Apply constraints in `matched_candidates` and `captures_raw` CTEs

**Files to modify:**
- `src/sql_macros/pattern_matching.sql`

### Phase 2: Nested Patterns

**Scope:**
- Parse `<contains>` and `<not-contains>` child elements
- Support `type=T` attribute for simple containment
- Support `lang=L` attribute with text content for pattern containment
- Recursive pattern matching for nested patterns

**Implementation approach:**
1. After Phase 1 parsing, check for child elements
2. For `<contains type=T>`: verify descendant exists with type T
3. For `<contains lang=L>pattern</contains>`:
   - Extract pattern text from element content
   - Run nested `ast_match` on candidate subtree
   - Verify at least one match exists
4. For `<not-contains>`: invert the check

**Complexity:** Medium-high. May benefit from C++ implementation for performance.

### Phase 3: Advanced Features (Future)

- `**` recursive depth matching
- `<ancestor>` constraints (match must be inside X)
- `<sibling>` constraints (match must be adjacent to X)
- Performance optimizations with indexing

## Processing Pipeline

```
User Pattern String
        ↓
[1] Find %__<...>__% regions (regex)
        ↓
[2] Parse each region with tree-sitter HTML
        ↓
[3] Extract wildcard rules from HTML AST:
    - tag_name → name (or _ for anonymous)
    - attribute * + ~ ? ** → modifiers
    - attribute key=value → constraints
    - child elements → nested constraints (Phase 2)
        ↓
[4] Clean pattern: replace %__<X...>__% with __X__
        ↓
[5] Parse cleaned pattern with target language parser
        ↓
[6] Match with constraints applied
```

## Backward Compatibility

The new syntax coexists with the old:

| Old Syntax | New Syntax | Both Work? |
|------------|------------|------------|
| `__X__` | `__X__` | ✅ Same |
| `__` | `__` | ✅ Same |
| `%__X<*>__%` | `%__<X*>__%` | ✅ Both work |
| `%__<*>__%` | `%__<*>__%` | ✅ Same |

We can support both during transition, eventually deprecating the old `%__X<rules>__%` form.

## Testing Strategy

1. **Parsing tests:** Verify HTML parsing extracts correct rules
2. **Modifier tests:** Each modifier (`*`, `+`, `~`, `?`) works correctly
3. **Constraint tests:** Each attribute constraint filters correctly
4. **Combination tests:** Multiple modifiers/constraints together
5. **Backward compatibility:** Old syntax still works
6. **Phase 2 tests:** Nested patterns match correctly

## Open Questions

1. **Self-closing vs not:** Allow both `<X*/>` and `<X*>`?
2. **Case sensitivity:** Tag names uppercase only? Allow lowercase?
3. **Attribute value types:** How to specify numeric comparisons? `max-descendants=10` vs `descendants<10`?
4. **Error messages:** How to report invalid wildcard syntax helpfully?

## Implementation Notes

### parse_ast_objects() Would Help

A `parse_ast_objects(source, lang)` scalar function returning AST as a nested struct
would enable using HTML parsing in macros without hitting DuckDB's "table function
cannot contain subqueries" limitation. Currently we use regex parsing instead.

Example of what would be possible with `parse_ast_objects`:
```sql
CREATE OR REPLACE MACRO parse_html_wildcard(html_str) AS (
    WITH ast AS (SELECT parse_ast_objects(html_str, 'html') as tree)
    SELECT {
        name: tree.root.children[0].name,
        attrs: tree.root.children[0].attributes
    }
    FROM ast
);
```

Instead, Phase 1 uses regex-based parsing which handles simple cases well.

## Related

- Current implementation: `src/sql_macros/pattern_matching.sql`
- Documentation: `docs/guide/pattern-matching.md`
- C++ proposal: `tracker/features/030-pattern-matching-cpp.md`
