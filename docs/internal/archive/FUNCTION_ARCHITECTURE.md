# DuckDB AST Extension Function Architecture

## Design Intent
The AST extension provides a systematic 2x2 matrix of functions based on:
- **Input Source**: String content vs File path(s)  
- **Output Format**: Flat table vs AST struct

## Function Matrix

| Input \ Output | Flat Table | AST Struct |
|----------------|------------|------------|
| **String** | `parse_ast()` | `parse_ast_objects()` |
| **File(s)** | `read_ast()` | `read_ast_objects()` |

## Function Purposes

### `parse_ast(code, language)` → TABLE
**Intent**: Parse string content into flat table for SQL analysis
**Use Case**: Quick analysis of code snippets, inline parsing in queries
```sql
-- Analyze a code snippet
SELECT type, count(*) FROM parse_ast('def foo(): return 1', 'python') GROUP BY type;
```

### `read_ast(filepath, language)` → TABLE  
**Intent**: Parse file into flat table for SQL analysis
**Use Case**: Traditional SQL analysis of single files
```sql
-- Find all functions in a file
SELECT name FROM read_ast('src/parser.py', 'python') WHERE type = 'function_definition';
```

### `parse_ast_objects(code, language)` → AST
**Intent**: Parse string content into AST struct for method chaining
**Use Case**: Fluent API on code snippets, scalar context
```sql
-- Method chaining on code snippet
SELECT parse_ast_objects('def hello(): pass', 'python').get_functions().to_names();
```

### `read_ast_objects(file_pattern)` → AST
**Intent**: Parse file(s) into AST struct for method chaining  
**Use Case**: Cross-file analysis, complex AST operations
```sql
-- Method chaining across files
SELECT ast.get_functions().filter_public().to_signatures() 
FROM read_ast_objects('src/*.py');
```

### `to_ast(code, language)` → AST (Scalar)
**Intent**: Inline AST creation for scalar contexts
**Use Case**: Quick AST access in SELECT clauses, WHERE conditions
```sql
-- Inline AST usage
SELECT to_ast('x = 1', 'python').nodes[1].name;
```

## Design Rationale

### Why This Matrix?
1. **Input flexibility** - Sometimes you have code strings, sometimes file paths
2. **Output flexibility** - Sometimes you want SQL tables, sometimes AST objects
3. **Composability** - Each function has a single, clear responsibility
4. **Systematic naming** - The pattern is predictable and memorable

### Why Not JSON?
- JSON was prototyping scaffolding, not the final interface
- AST structs provide native DuckDB operations (filtering, aggregation, etc.)
- Method chaining requires structured data, not strings
- Performance: no string parsing overhead

### Meta: The Original Problem
This tool was originally designed to extract concept-oriented annotations like:
- `@intent` - What was the developer trying to accomplish?
- `@implementation_status` - Is this complete, TODO, deprecated?  
- `@risk` - What could go wrong with this code?
- `@decision` - Why was this approach chosen?
- `@invariant` - What assumptions does this code make?

The irony: we're now using the tool to document its own intent!

## Implementation Status
- ✅ `parse_ast()` - Complete (returns JSON, needs table format)
- ✅ `read_ast()` - Complete  
- ✅ `read_ast_objects()` - Complete (wrong format, needs AST struct)
- ❌ `parse_ast_objects()` - Not implemented
- ❌ `to_ast()` - Not implemented

## Next Steps
1. Unify all functions on single backend with taxonomy
2. Implement missing `parse_ast_objects()` and `to_ast()`
3. Update `parse_ast()` to return table instead of JSON
4. Update `read_ast_objects()` to return true AST struct
5. Test the complete matrix with real workflows