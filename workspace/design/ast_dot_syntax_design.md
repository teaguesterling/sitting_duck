# AST Dot Syntax Design - AI Agent Perspective

## What Would Feel Most Natural?

As an AI agent, I'd want to write queries like:

```sql
-- Find all print statements in a file
SELECT * FROM read_ast('file.py').find('call[name=print]')

-- Get all function definitions with their names
SELECT * FROM read_ast('file.py').functions()

-- Find all TODO comments
SELECT * FROM read_ast('file.py').comments().filter(content LIKE '%TODO%')

-- Get the complexity of functions
SELECT name, ast.count_nodes() as complexity
FROM read_ast('file.py').functions() as ast

-- Find all SQL queries in Python code
SELECT * FROM read_ast('app.py').strings().filter(content LIKE '%SELECT%')
```

## Key Principles for AI Usability

1. **Intuitive Names** - Methods should be what I'd naturally type
2. **Chainable** - Each method returns something I can continue working with
3. **No JSON Manipulation** - Hide the JSON complexity entirely
4. **Common Patterns Built-in** - functions(), classes(), imports() etc.

## Proposed Dot Syntax API

```sql
-- Starting point
read_ast(file_path, language) -> AST

-- Navigation methods
ast.children() -> AST       -- all direct children
ast.descendants() -> AST     -- all descendants  
ast.parent() -> AST          -- parent node
ast.siblings() -> AST        -- sibling nodes

-- Filtering methods
ast.find(pattern) -> AST     -- CSS-like selector: 'function > identifier'
ast.filter(condition) -> AST -- SQL-like: type = 'call' AND content = 'print'
ast.type(type_name) -> AST   -- shorthand for filter(type = type_name)

-- Extraction methods (return tables, not AST)
ast.functions() -> TABLE(name, params, body, line)
ast.classes() -> TABLE(name, methods, line)
ast.imports() -> TABLE(module, names, line)
ast.variables() -> TABLE(name, value, scope, line)
ast.comments() -> TABLE(content, line)
ast.strings() -> TABLE(content, line)
ast.calls() -> TABLE(function, args, line)

-- Analysis methods
ast.count() -> INTEGER
ast.depth() -> INTEGER
ast.stats() -> TABLE(metric, value)

-- Output methods
ast.to_table() -> TABLE      -- columnar format
ast.to_json() -> JSON        -- for custom processing
```

## Natural Query Examples

```sql
-- Find all error handling
SELECT * FROM read_ast('app.py')
  .find('try_statement')
  .to_table()

-- Get all API endpoints in a Flask app
SELECT * FROM read_ast('routes.py')
  .functions()
  WHERE name LIKE '%route%' OR decorator LIKE '@app.%'

-- Find complex functions
SELECT name, complexity FROM (
  SELECT name, body.count() as complexity 
  FROM read_ast('utils.py').functions()
) WHERE complexity > 50

-- Find all database queries
SELECT DISTINCT query FROM read_ast('models.py')
  .calls()
  WHERE function IN ('execute', 'query', 'fetchall')

-- Get all class hierarchies  
SELECT child.name, parent.name
FROM read_ast('models.py').classes() child
JOIN read_ast('models.py').classes() parent
  ON child.inherits = parent.name
```

## Why This Design?

1. **No JSON knowledge needed** - I don't need to know about `json_extract`
2. **Discoverable** - The dot notation shows available methods
3. **Composable** - Can chain operations naturally
4. **SQL-friendly** - Returns tables that work with joins, WHERE, etc.
5. **Familiar patterns** - Like jQuery selectors or DOM traversal

## Implementation Approach

This would require:
1. Custom DuckDB type for AST that supports method chaining
2. Each method returns either AST type (for chaining) or TABLE (for querying)
3. Lazy evaluation - only execute when final result is needed

Alternative: Implement as SQL macros that expand to JSON operations under the hood.