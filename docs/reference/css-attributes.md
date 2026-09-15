# Attribute Selectors

Use `[attr operator value]` syntax to query AST nodes by their metadata fields. Supports four CSS attribute operators.

## Operators

| Operator | Meaning | Example |
|----------|---------|---------|
| `=` | Exact match | `[name=main]` |
| `*=` | Contains substring | `[name*=auth]` |
| `^=` | Starts with | `[name^=test_]` |
| `$=` | Ends with | `[name$=_handler]` |

Not every operator applies to every attribute: the text attributes (`name`,
`annotation`, `qualified`, `signature`, `receiver`, `peek`, `file`) support all four;
`type`/`language`/`semantic`/`params`/`line` support `=` only; `modifier` supports
`=` and `*=` (both mean "has this modifier" — modifiers is a list). An
unsupported combination (or an unknown attribute name) raises an error rather
than silently returning wrong or empty results (issue #89).

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

Supports exact match and the prefix/suffix/substring operators (a bare type
selector like `function_definition` is an exact match, #151):

```sql
-- Exact tree-sitter type (same as the bare selector `function_definition`)
SELECT * FROM ast_select('src/*.py', '[type=function_definition]');
-- Prefix / suffix / substring on the type name
SELECT * FROM ast_select('src/*.py', '[type^=if]');          -- if, if_statement, if_clause, ...
SELECT * FROM ast_select('src/*.py', '[type$=_statement]');  -- *_statement
SELECT * FROM ast_select('src/*.py', '[type*=expr]');        -- *expr*
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

## Location Attributes

Where a node is. Together with `#name` they address a single node exactly, which is
what you need to turn a node back into a selector.

### `[file]` — Source File

`file` is the node's `file_path` exactly as `read_ast()` stored it: relative or
absolute, whichever form the glob produced. The suffix operator is therefore the
portable way to name a file.

```sql
-- Calls in one file, however the path was given
SELECT name, start_line FROM ast_select('src/**/*.py', '.call[file$="app.py"]');

-- Functions under a directory
SELECT name FROM ast_select('src/**/*.py', '.func[file*="/handlers/"]');
```

### `[line]` — Start Line

```sql
-- The call that starts on line 42 of app.py
SELECT name FROM ast_select('src/**/*.py', '.call#execute[file$="app.py"][line=42]');
```

`line` compares against `start_line` and supports `=` only.

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
| `[file$=x]` | Source file path ends with | `[file$="app.py"]` |
| `[line=n]` | Start line | `[line=42]` |

---

## See Also

- [CSS Selectors Overview](css-selectors.md) — Combinators, compound selectors, API reference
- [Pseudo-Classes Reference](css-pseudo-classes.md) — Structural and modifier pseudo-classes
- [Node Type Selectors](node-type-selectors.md) — Three tiers of type specificity
