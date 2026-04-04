# Attribute Selectors

Use `[attr operator value]` syntax to query AST nodes by their metadata fields. Supports four CSS attribute operators.

## Operators

| Operator | Meaning | Example |
|----------|---------|---------|
| `=` | Exact match | `[name=main]` |
| `*=` | Contains substring | `[name*=auth]` |
| `^=` | Starts with | `[name^=test_]` |
| `$=` | Ends with | `[name$=_handler]` |

## Core Attributes

These correspond to columns in the `read_ast()` output.

### `[name]` — Node Name

```sql
-- Exact name match (equivalent to #name shorthand)
SELECT * FROM ast_select('src/*.py', 'function_definition[name=main]');

-- Name starts with
SELECT name FROM ast_select('src/*.py', 'function_definition[name^=test_]');

-- Name ends with
SELECT name FROM ast_select('src/*.py', 'function_definition[name$=_handler]');

-- Name contains
SELECT name FROM ast_select('src/*.py', 'function_definition[name*=auth]');
```

### `[type]` — Node Type

```sql
-- Filter by exact tree-sitter type
SELECT * FROM ast_select('src/*.py', '[type=function_definition]');
```

### `[language]` — Language

```sql
-- Only Python files (useful with multi-language globs)
SELECT name FROM ast_select('src/**/*', '.func[language=python]');
```

### `[semantic]` — Semantic Type

```sql
-- By semantic type name
SELECT name FROM ast_select('src/*.py', '[semantic=DEFINITION_FUNCTION]');
```

## Native Extraction Attributes

These query the rich metadata that Sitting Duck extracts from each node.

### `[modifier]` — Modifier Flags

```sql
-- Functions with async modifier
SELECT name FROM ast_select('src/*.js', '.func[modifier=async]');

-- Static methods
SELECT name FROM ast_select('src/*.java', '.func[modifier=static]');
```

### `[annotation]` — Decorators / Annotations

```sql
-- Decorated with a specific decorator
SELECT name FROM ast_select('src/*.py', '.func[annotation*=pytest]');

-- Any route-decorated function
SELECT name FROM ast_select('src/*.py', '.func[annotation*=route]');
```

### `[qualified]` — Qualified / Dotted Name

```sql
-- Functions in a specific namespace
SELECT name FROM ast_select('src/*.py', '.func[qualified*=auth.]');

-- Methods on a specific class
SELECT name FROM ast_select('src/*.py', '.func[qualified^=UserService.]');
```

### `[signature]` — Return Type / Signature

```sql
-- Functions returning a specific type
SELECT name FROM ast_select('src/*.ts', '.func[signature=Promise]');

-- Functions with int return type
SELECT name FROM ast_select('src/*.py', '.func[signature=int]');
```

### `[params]` — Parameter Count

```sql
-- Functions with exactly 2 parameters
SELECT name FROM ast_select('src/*.py', '.func[params=2]');

-- Zero-parameter functions
SELECT name FROM ast_select('src/*.py', '.func[params=0]');
```

### `[peek]` — Source Text Content

```sql
-- Strings containing SQL keywords
SELECT name, peek FROM ast_select('src/*.py', 'string[peek*=SELECT]');

-- Comments mentioning TODO
SELECT peek FROM ast_select('src/*.py', 'comment[peek*=TODO]');
```

## Quick Reference

| Attribute | Meaning | Example |
|---|---|---|
| `[name=x]` | Exact name | `function_definition[name=main]` |
| `[name^=test_]` | Name starts with | `[name^=test_]` |
| `[name$=_handler]` | Name ends with | `[name$=_handler]` |
| `[name*=auth]` | Name contains | `[name*=auth]` |
| `[modifier=x]` | Has modifier | `[modifier=async]` |
| `[annotation*=x]` | Annotation contains | `[annotation*=pytest]` |
| `[qualified*=x]` | Qualified name contains | `[qualified*=auth.]` |
| `[signature=x]` | Signature/return type | `[signature=int]` |
| `[params=n]` | Parameter count | `[params=0]` |
| `[peek*=x]` | Source text contains | `[peek*=SELECT]` |
| `[language=x]` | Language filter | `[language=python]` |
| `[semantic=x]` | Semantic type | `[semantic=FUNCTION]` |

---

## See Also

- [CSS Selectors Overview](index.md) — Combinators, compound selectors, API reference
- [Pseudo-Classes Reference](pseudo-classes.md) — Structural and modifier pseudo-classes
- [Node Type Selectors](node-types.md) — Three tiers of type specificity
