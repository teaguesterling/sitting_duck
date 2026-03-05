# Sitting Duck — AI Agent Skill Guide

**Extension:** Sitting Duck (DuckDB)
**Purpose:** Parse source code into ASTs and query them with SQL
**Languages:** 27 (auto-detected from file extension)

---

## Quick Start

```sql
LOAD sitting_duck;

-- Parse files into a flat AST table
SELECT * FROM read_ast('src/**/*.py');

-- Parse a code string directly
SELECT * FROM parse_ast('def hello(): pass', 'python');

-- Materialize for reuse with macros
CREATE TABLE ast AS SELECT * FROM read_ast('src/**/*.py', ignore_errors := true);
```

Most macro functions take a **table name** (string) as their first argument. Materialize your AST first, then pass the table name:

```sql
CREATE TABLE ast AS SELECT * FROM read_ast('src/**/*.py');
SELECT * FROM ast_function_metrics('ast');
SELECT * FROM ast_definitions('ast');
```

---

## Core Functions

### `read_ast(file_patterns, [language], [options...])`

Main parsing entry point. Returns a flat table of AST nodes.

```sql
-- Single file or glob
SELECT * FROM read_ast('main.py');
SELECT * FROM read_ast('src/**/*.py');

-- Pattern array (DuckDB-consistent, like read_csv)
SELECT * FROM read_ast(['src/**/*.py', 'lib/**/*.js', 'main.cpp']);

-- With options
SELECT * FROM read_ast('**/*.*', ignore_errors := true, peek_size := 200);
```

**Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `file_patterns` | VARCHAR or LIST(VARCHAR) | required | File path(s) or glob pattern(s) |
| `language` | VARCHAR | auto-detect | Language override |
| `ignore_errors` | BOOLEAN | false | Skip files with parse errors |
| `peek_size` | INTEGER | 120 | Characters in peek field |
| `peek_mode` | VARCHAR | 'auto' | Peek extraction: 'auto', 'chars', 'lines' |

**Key columns returned:**

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | BIGINT | Unique node ID (DFS pre-order) |
| `type` | VARCHAR | Language-specific node type (e.g., `function_definition`) |
| `name` | VARCHAR | Extracted name/identifier |
| `semantic_type` | UTINYINT | Universal semantic category (0-255) |
| `flags` | UTINYINT | Semantic flags (IS_CONSTRUCT, IS_EMBODIED) |
| `parent_id` | BIGINT | Parent node ID |
| `depth` | UINTEGER | Tree depth (0 = root) |
| `descendant_count` | UINTEGER | Total descendants (subtree size) |
| `file_path` | VARCHAR | Source file |
| `language` | VARCHAR | Detected language |
| `start_line`, `end_line` | UINTEGER | Location (1-based) |
| `peek` | VARCHAR | Source code snippet |

### `parse_ast(source_code, language)`

Parse a code string directly. Same output schema as `read_ast`.

```sql
SELECT * FROM parse_ast('if x > 0: print(x)', 'python');
```

### `ast_supported_languages()`

List all supported languages with their file extensions.

---

## Semantic Type System

Every AST node has a `semantic_type` (UTINYINT) providing cross-language meaning. Use predicate macros instead of raw codes when possible.

### Semantic Predicate Macros

These are the primary way to filter nodes. They work identically across all 27 languages.

**Definitions:**
| Macro | Matches |
|-------|---------|
| `is_definition(st)` | Any definition (function, class, variable, module, type) |
| `is_function_definition(st)` | Function/method definitions |
| `is_class_definition(st)` | Class/struct/interface definitions |
| `is_variable_definition(st)` | Variable/constant definitions |
| `is_module_definition(st)` | Module/namespace definitions |
| `is_type_definition(st)` | Type alias / typedef definitions |

**Computation:**
| Macro | Matches |
|-------|---------|
| `is_function_call(st)` | Function/method calls |
| `is_member_access(st)` | Property/member access |

**Control flow:**
| Macro | Matches |
|-------|---------|
| `is_conditional(st)` | if / switch / match |
| `is_loop(st)` | for / while / do |
| `is_jump(st)` | return / break / continue / throw |

**Operators:**
| Macro | Matches |
|-------|---------|
| `is_assignment(st)` | Assignment operators |
| `is_comparison(st)` | Comparison operators |
| `is_arithmetic(st)` | Arithmetic operators |
| `is_logical(st)` | Logical operators (and/or/not) |

**Literals:**
| Macro | Matches |
|-------|---------|
| `is_literal(st)` | Any literal |
| `is_string_literal(st)` | String literals |
| `is_number_literal(st)` | Numeric literals |
| `is_boolean_literal(st)` | Boolean literals |

**Organization:**
| Macro | Matches |
|-------|---------|
| `is_block(st)` | Blocks / scopes |
| `is_list(st)` | Parameter/argument lists |

**External:**
| Macro | Matches |
|-------|---------|
| `is_import(st)` | Import statements |
| `is_export(st)` | Export statements |
| `is_foreign(st)` | Foreign function interfaces |

**Metadata:**
| Macro | Matches |
|-------|---------|
| `is_comment(st)` | Comments |
| `is_annotation(st)` | Decorators / annotations |
| `is_directive(st)` | Preprocessor directives |

**Types:**
| Macro | Matches |
|-------|---------|
| `is_type_primitive(st)` | Primitive types (int, str) |
| `is_type_composite(st)` | Composite types (List[int]) |
| `is_type_reference(st)` | Type references |
| `is_type_generic(st)` | Generic/template types |

### Semantic Type Conversion Functions

```sql
semantic_type_to_string(code)  -- UTINYINT -> 'DEFINITION_FUNCTION'
get_super_kind(code)           -- Top-level category
get_kind(code)                 -- Subcategory
is_semantic_type(code, name)   -- Check by name string
is_kind(code, kind_name)       -- Check kind by name string
```

### Flag Helper Functions

```sql
is_construct(flags)   -- True for semantic constructs (not punctuation/tokens)
is_embodied(flags)    -- True if node has a body/implementation
has_body(flags)       -- Alias for is_embodied
is_syntax_only(flags) -- True for pure syntax tokens (delimiters, punctuation)
```

### Common Semantic Type Codes (Quick Reference)

When performance matters, use raw codes instead of predicate macros:

| Code | Name | Description |
|------|------|-------------|
| 240 | DEFINITION_FUNCTION | Function/method definitions |
| 248 | DEFINITION_CLASS | Class/struct/interface |
| 244 | DEFINITION_VARIABLE | Variable/constant definitions |
| 252 | DEFINITION_MODULE | Module/namespace |
| 208 | COMPUTATION_CALL | Function/method calls |
| 212 | COMPUTATION_ACCESS | Member/property access |
| 144 | FLOW_CONDITIONAL | if/switch/match |
| 148 | FLOW_LOOP | for/while loops |
| 152 | FLOW_JUMP | return/break/continue |
| 48 | EXTERNAL_IMPORT | Import/include |
| 80 | NAME_IDENTIFIER | Identifiers |
| 32 | METADATA_COMMENT | Comments |
| 128 | EXECUTION_STATEMENT | Expression statements |

For the complete type hierarchy, see [API_REFERENCE.md](API_REFERENCE.md#semantic-type-system).

---

## Tree Navigation Macros

All navigation macros take a **table name** (string) as the first argument. Materialize your AST first:

```sql
CREATE TABLE ast AS SELECT * FROM read_ast('src/**/*.py', ignore_errors := true);
```

### Subtree and Ancestor Queries

#### `ast_descendants(table, node_id)` — Get all descendants of a node

Uses DFS pre-order `node_id` + `descendant_count` for O(1) range lookup (no recursion).

```sql
-- Get everything inside function node 42
SELECT * FROM ast_descendants('ast', 42);
```

#### `ast_ancestors(table, node_id)` — Get path from node to root

Recursive CTE walking `parent_id` upward.

```sql
-- What contains this node?
SELECT type, name, depth FROM ast_ancestors('ast', 100);
```

#### `ast_children(table, parent_node_id)` — Get immediate children

```sql
SELECT * FROM ast_children('ast', 42);
```

#### `ast_siblings(table, node_id)` — Get sibling nodes (same parent, excluding self)

```sql
SELECT * FROM ast_siblings('ast', 42);
```

### Line-Based Navigation

#### `ast_containing_line(table, line_number)` — All nodes containing a line

Returns nodes ordered by specificity (smallest span first).

```sql
-- What's on line 25?
SELECT type, name, depth FROM ast_containing_line('ast', 25);
```

#### `ast_in_range(table, start_line, end_line)` — All nodes within a line range

```sql
SELECT * FROM ast_in_range('ast', 10, 50);
```

### Definition-Aware Queries

#### `ast_definitions(table)` — All named definitions with unified categories

Returns definitions with a `definition_type` column ('function', 'class', 'variable', 'module', 'type'). Filters to construct nodes with non-empty names.

```sql
SELECT name, definition_type, file_path, start_line
FROM ast_definitions('ast')
WHERE definition_type = 'function';
```

#### `ast_class_members(table, class_node_id)` — Direct members of a class

Returns methods, fields, and nested classes that are direct members (not locals inside method bodies).

```sql
-- Get members of a specific class
SELECT name, type, start_line FROM ast_class_members('ast', 15);
```

#### `ast_call_arguments(table, call_node_id)` — Arguments of a function call

Returns argument nodes with positional info, excluding punctuation.

```sql
SELECT arg_position, arg_name, arg_type, arg_peek
FROM ast_call_arguments('ast', 200);
```

### Scope-Aware Analysis

#### `ast_function_scope(table, function_node_id)` — Nodes inside a function, excluding nested functions

Critical for accurate analysis. Without this, metrics for an outer function would include inner function internals.

```sql
-- Get only this function's body, not nested function bodies
SELECT * FROM ast_function_scope('ast', 42);
```

#### `ast_function_metrics(table)` — Complexity metrics for all functions

Returns one row per function with scope-aware metrics that exclude nested function internals.

```sql
SELECT
    file_path, name, language,
    start_line, end_line, lines,
    cyclomatic,          -- conditionals + loops + 1
    conditionals, loops, -- individual counts
    return_count,
    max_depth            -- relative to function
FROM ast_function_metrics('ast')
WHERE cyclomatic > 10
ORDER BY cyclomatic DESC;
```

#### `ast_nesting_analysis(table)` — Depth statistics per function

```sql
SELECT name, max_depth, avg_depth, deep_nodes, total_nodes
FROM ast_nesting_analysis('ast')
WHERE max_depth > 5;
```

### Search and Audit

#### `ast_functions_containing(table, target_type)` — Find functions containing a node type

```sql
-- Which functions use try/except?
SELECT func_name, file_path, match_line, match_peek
FROM ast_functions_containing('ast', 'try_statement');

-- Which functions call a specific name?
SELECT * FROM ast_functions_containing('ast', 'call')
WHERE match_name = 'dangerous_func';
```

#### `ast_security_audit(table)` — Automated security concern detection

Scans for common security anti-patterns (code injection, command injection, deserialization, weak crypto, etc.) across languages.

```sql
SELECT file_path, start_line, function_name, risk_category, risk_level, finding
FROM ast_security_audit('ast')
WHERE risk_level = 'high';
```

#### `ast_dead_code(table)` — Find potentially unreferenced definitions

Heuristic dead code detection. Best used on the entire codebase (cross-file analysis).

```sql
SELECT name, definition_type, file_path, start_line
FROM ast_dead_code('ast');
```

---

## Pattern Matching

The `ast_match` macro enables pattern-by-example matching: write a code snippet with wildcard placeholders, and find all matching AST structures.

### Basic Usage

```sql
CREATE TABLE ast AS SELECT * FROM read_ast('src/**/*.py');

-- Find all calls to a dangerous function, capture the argument
SELECT * FROM ast_match('ast', 'dangerous_func(__X__)', 'python');

-- Access captured nodes
SELECT
    ast_capture(captures, 'X').name AS arg_name,
    ast_capture(captures, 'X').peek AS arg_code
FROM ast_match('ast', 'dangerous_func(__X__)', 'python');
```

### Wildcard Syntax

| Pattern | Meaning |
|---------|---------|
| `__X__` | Named wildcard — captures the matched node as 'X' |
| `__` | Anonymous wildcard — matches any node, no capture |
| `%__X<*>__%` | Named variadic — captures 0 or more siblings as list |
| `%__X<+>__%` | Named variadic — captures 1 or more siblings as list |

Named wildcards use UPPERCASE (`__NAME__`). Python dunders like `__init__` use lowercase, so they don't conflict.

### Parameters

```sql
ast_match(
    ast_table,              -- Table name (string)
    pattern_str,            -- Code pattern with wildcards
    lang := 'python',       -- Language for parsing the pattern
    match_syntax := false,  -- If true, punctuation must also match
    match_by := 'type',     -- 'type' (language-specific) or 'semantic_type' (cross-language)
    depth_fuzz := 0         -- Allow +/- N depth levels (for cross-language matching)
)
```

### Examples

```sql
-- Find 3-arg calls with literal 2 in the middle
SELECT * FROM ast_match('ast', '__F__(__, 2, __X__)', 'python');

-- Cross-language: match by semantic type instead of tree-sitter type
SELECT * FROM ast_match('ast', '__F__(__X__)', 'python', match_by := 'semantic_type');

-- Find functions whose body ends with a return
SELECT * FROM ast_match('ast',
    'def __F__(__):
        %__BODY<*>__%
        return __Y__',
    'python');

-- Same-name constraint: __X__ appears twice, both must match the same source text
SELECT * FROM ast_match('ast', '__X__ + __X__', 'python');
```

### Working with Captures

Captures are `MAP(VARCHAR, LIST(STRUCT))`. Each capture name maps to a list of matched nodes.

```sql
-- Single wildcard capture (list of 1 element, use [1] or ast_capture)
SELECT ast_capture(captures, 'F').name AS func_name
FROM ast_match('ast', '__F__(__X__)', 'python');

-- Equivalent:
SELECT captures['F'][1].name FROM ast_match('ast', '__F__(__X__)', 'python');

-- Variadic capture (list of 0+ elements)
SELECT captures['BODY'] AS body_statements
FROM ast_match('ast',
    'def __F__(__):
        %__BODY<*>__%
        return __Y__',
    'python');
```

---

## File Utility Macros

### Source Code Extraction

```sql
-- Get source code for a node (pass file_path, start_line, end_line)
SELECT ast_get_source(file_path, start_line, end_line) AS source
FROM ast WHERE name = 'my_function' AND is_function_definition(semantic_type);

-- With line numbers
SELECT ast_get_source_numbered(file_path, start_line, end_line) AS source
FROM ast WHERE name = 'my_function';

-- Single line
SELECT ast_get_source_line('src/main.py', 42);
```

### Definition Lookup

#### `ast_source_of(file_patterns, name, [language], [kind])` — Find and display a definition's source

```sql
-- Find a function by name across files
SELECT * FROM ast_source_of('src/**/*.py', 'process_payment');

-- Narrow to a specific kind
SELECT * FROM ast_source_of('src/**/*.py', 'MyClass', kind := 'class');

-- With language filter
SELECT * FROM ast_source_of('src/**/*.py', 'handler', language := 'python', kind := 'function');
```

Returns: `file_path`, `name`, `definition_kind`, `start_line`, `end_line`, `source` (numbered).

---

## Supported Languages (27)

| Category | Languages |
|----------|-----------|
| **Web** | JavaScript (.js, .jsx), TypeScript (.ts, .tsx), HTML (.html, .htm), CSS (.css) |
| **Systems** | C (.c, .h), C++ (.cpp, .hpp, .cc, .cxx), Go (.go), Rust (.rs), Zig (.zig) |
| **Scripting** | Python (.py), Ruby (.rb), PHP (.php), Lua (.lua), R (.r, .R), Bash (.sh, .bash) |
| **Enterprise** | Java (.java), C# (.cs), Kotlin (.kt, .kts), Swift (.swift) |
| **Mobile** | Dart (.dart) |
| **Infrastructure** | HCL/Terraform (.hcl, .tf, .tfvars), JSON (.json), TOML (.toml), GraphQL (.graphql, .gql), YAML (.yaml, .yml) |
| **Documentation** | SQL (.sql), Markdown (.md, .markdown) |

Languages are auto-detected from file extensions. Override with `read_ast('file', 'language')`.

---

## Recipes

### Codebase Inventory

```sql
CREATE TABLE ast AS SELECT * FROM read_ast('**/*.*', ignore_errors := true);

SELECT language, COUNT(DISTINCT file_path) AS files,
       COUNT(*) FILTER (WHERE is_function_definition(semantic_type)) AS functions,
       COUNT(*) FILTER (WHERE is_class_definition(semantic_type)) AS classes
FROM ast
GROUP BY language
ORDER BY files DESC;
```

### Find Complex Functions

```sql
SELECT name, file_path, cyclomatic, lines, max_depth
FROM ast_function_metrics('ast')
WHERE cyclomatic > 10 OR max_depth > 6
ORDER BY cyclomatic DESC;
```

### Cross-Language Import Map

```sql
SELECT file_path, language, name, peek
FROM ast
WHERE is_import(semantic_type)
ORDER BY file_path;
```

### Find All Test Functions

```sql
SELECT name, file_path, start_line
FROM ast_definitions('ast')
WHERE definition_type = 'function'
  AND (name LIKE 'test_%' OR name LIKE '%_test' OR name LIKE '%Test%')
ORDER BY file_path, start_line;
```

### Security Scan

```sql
SELECT risk_level, risk_category, file_path, start_line, function_name, finding
FROM ast_security_audit('ast')
ORDER BY
    CASE risk_level WHEN 'high' THEN 1 WHEN 'medium' THEN 2 ELSE 3 END;
```

### Structural Pattern Search

```sql
-- Find bare except clauses (Python anti-pattern)
SELECT file_path, start_line, peek
FROM ast_match('ast', 'except: __', 'python');

-- Find functions that call themselves (potential recursion)
SELECT ast_capture(captures, 'F').name AS func_name, file_path, start_line
FROM ast_match('ast', 'def __F__(__): %__<*>__% __F__(__)', 'python');
```

### Source Code Lookup

```sql
-- Show the source of a specific function
SELECT source FROM ast_source_of('src/**/*.py', 'process_payment');
```

---

## Best Practices

1. **Materialize for macro use.** `read_ast` returns a table; macros like `ast_function_metrics` need a table name. Create a table first.

2. **Use predicate macros over raw codes.** `is_function_definition(semantic_type)` is portable and readable. Use raw codes (240) only for performance-critical queries.

3. **Always use `ignore_errors := true` for globs.** A single file with a syntax error will halt the entire query otherwise.

4. **Use `is_construct(flags)` when listing definitions.** Filters out syntax tokens that happen to share a semantic type with definitions.

5. **Scope-aware analysis matters.** Use `ast_function_metrics` or `ast_function_scope` instead of naive descendant queries — they exclude nested function internals for accurate metrics.

6. **Pattern arrays for precision.** `read_ast(['src/**/*.py', 'tests/**/*.py'])` is more efficient and explicit than `read_ast('**/*.py')`.

7. **Cross-language matching.** Use `match_by := 'semantic_type'` in `ast_match` to find patterns across languages with a single query.

---

*For the complete semantic type hierarchy and detailed column descriptions, see [API_REFERENCE.md](API_REFERENCE.md).*
