# Scripting Languages

Python, Ruby, PHP, Lua, R, and Bash support in Sitting Duck.

## Language Nuances

### Extraction Quality Summary

| Language | Functions | Classes | Calls | Variables | Body Detection | Overall |
|----------|-----------|---------|-------|-----------|----------------|---------|
| **Python** | ⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐ | ⭐⭐⭐ | Good |
| **Ruby** | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐ | ⭐⭐⭐ | Good |
| **PHP** | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐ | ⭐⭐⭐ | Good |
| **Lua** | ⭐ | ⭐ | ⭐ | ⭐ | ⭐⭐⭐ | Needs Work |
| **R** | ⭐⭐ | ⭐ | ⭐⭐ | ⭐ | ⭐⭐ | Basic |
| **Bash** | ⭐⭐ | N/A | ⭐⭐ | ⭐ | ⭐⭐⭐ | Basic |

### Implementation Notes

- **Python Classes**: Excellent class extraction including inheritance detection and abstract class identification (ABC subclasses).
- **Ruby Bodies**: Uses `body_statement` for method bodies - correctly detects all method implementations.
- **PHP Keywords**: The `function` keyword is marked as `IS_SYNTAX_ONLY` to avoid counting it as a function definition.
- **R Lambdas**: Lambda expressions use `braced_expression` bodies with ~84% detection accuracy.

### Known Limitations

- **Python Type Hints**: Type hints are parsed but not fully extracted into `signature_type`.
- **Ruby Blocks**: Block parameters (`do |x|`) are parsed but parameter extraction is limited.
- **PHP Closures**: Anonymous functions (`function() use ($var)`) are detected but captured variables not extracted.
- **Lua Parameters**: Function parameters show as empty array `[]` - needs native extractor implementation.
- **R Functions**: Dynamic typing means no type information available; variable extraction is minimal.
- **Bash**: No class support (N/A); function and variable extraction is basic.

### Body Detection Details

| Language | Body Type | Accuracy | Notes |
|----------|-----------|----------|-------|
| Python | `block` | ⭐⭐⭐ | Indentation-based blocks work correctly |
| Ruby | `body_statement` | ⭐⭐⭐ | Fixed in recent update |
| PHP | `compound_statement` | ⭐⭐⭐ | `function` keyword properly excluded |
| Lua | `block` | ⭐⭐⭐ | Works correctly |
| R | `braced_expression` | ⭐⭐ | ~84% accuracy for lambdas |
| Bash | `compound_statement` | ⭐⭐⭐ | Works correctly |

---

## Python

**Extensions:** `.py`
**Identifier:** `'python'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Functions |
| `async_function_definition` | DEFINITION_FUNCTION | Async functions |
| `class_definition` | DEFINITION_CLASS | Classes |
| `decorated_definition` | METADATA_ANNOTATION | Decorated items |
| `import_statement` | EXTERNAL_IMPORT | Imports |
| `import_from_statement` | EXTERNAL_IMPORT | From imports |

### Examples

```sql
-- Find all functions
SELECT name, start_line
FROM read_ast('**/*.py')
WHERE type = 'function_definition';

-- Find decorated functions
SELECT name, peek
FROM read_ast('**/*.py')
WHERE type = 'function_definition'
  AND parent_id IN (SELECT node_id FROM read_ast('**/*.py') WHERE type = 'decorated_definition');

-- Find class methods
SELECT c.name as class_name, f.name as method_name
FROM read_ast('example.py') c
JOIN read_ast('example.py') f ON f.parent_id = c.node_id
WHERE c.type = 'class_definition'
  AND f.type = 'function_definition';
```

---

## Ruby

**Extensions:** `.rb`
**Identifier:** `'ruby'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `method` | DEFINITION_FUNCTION | Methods |
| `class` | DEFINITION_CLASS | Classes |
| `module` | DEFINITION_MODULE | Modules |
| `call` | COMPUTATION_CALL | Method calls |

### Examples

```sql
-- Find methods
SELECT name, start_line
FROM read_ast('**/*.rb')
WHERE type = 'method';

-- Find classes
SELECT name
FROM read_ast('**/*.rb')
WHERE type = 'class';

-- Find modules
SELECT name
FROM read_ast('**/*.rb')
WHERE type = 'module';
```

---

## PHP

**Extensions:** `.php`
**Identifier:** `'php'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Functions |
| `method_declaration` | DEFINITION_FUNCTION | Methods |
| `class_declaration` | DEFINITION_CLASS | Classes |
| `interface_declaration` | DEFINITION_CLASS | Interfaces |
| `namespace_definition` | DEFINITION_MODULE | Namespaces |

### Examples

```sql
-- Find classes
SELECT name, start_line
FROM read_ast('**/*.php')
WHERE type = 'class_declaration';

-- Find methods
SELECT name
FROM read_ast('**/*.php')
WHERE type = 'method_declaration';

-- Find namespaces
SELECT peek
FROM read_ast('**/*.php')
WHERE type = 'namespace_definition';
```

---

## Lua

**Extensions:** `.lua`
**Identifier:** `'lua'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Functions |
| `local_function` | DEFINITION_FUNCTION | Local functions |
| `variable_declaration` | DEFINITION_VARIABLE | Variables |
| `function_call` | COMPUTATION_CALL | Calls |

### Examples

```sql
-- Find functions
SELECT name, start_line
FROM read_ast('**/*.lua')
WHERE type IN ('function_declaration', 'local_function');

-- Find local variables
SELECT name
FROM read_ast('**/*.lua')
WHERE type = 'variable_declaration';
```

---

## R

**Extensions:** `.r`, `.R`
**Identifier:** `'r'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Functions |
| `left_assignment` | DEFINITION_VARIABLE | Assignments |
| `call` | COMPUTATION_CALL | Function calls |

### Examples

```sql
-- Find function definitions
SELECT name, start_line
FROM read_ast('**/*.R')
WHERE type = 'function_definition';

-- Find variable assignments
SELECT peek
FROM read_ast('analysis.R')
WHERE type = 'left_assignment';
```

---

## Bash

**Extensions:** `.sh`, `.bash`
**Identifier:** `'bash'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Functions |
| `variable_assignment` | DEFINITION_VARIABLE | Assignments |
| `command` | COMPUTATION_CALL | Commands |
| `if_statement` | FLOW_CONDITIONAL | If statements |
| `for_statement` | FLOW_LOOP | For loops |

### Examples

```sql
-- Find functions
SELECT name, start_line
FROM read_ast('**/*.sh')
WHERE type = 'function_definition';

-- Find variable assignments
SELECT peek
FROM read_ast('script.sh')
WHERE type = 'variable_assignment';

-- Find commands
SELECT peek
FROM read_ast('script.sh')
WHERE type = 'command';
```

## Cross-Scripting Analysis

```sql
-- Compare scripting language patterns
SELECT
    language,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 244 THEN 1 END) as variables,
    COUNT(*) as total_nodes
FROM read_ast(['**/*.py', '**/*.rb', '**/*.php', '**/*.lua'], ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;
```
