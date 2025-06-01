# Self-Analysis Dogfooding

## Vision
Use the DuckDB AST extension to analyze its own codebase - the ultimate dogfooding test.

## Example Queries We Want to Enable

### 1. Find Schema Definition Patterns
```sql
-- Find all places where we define DuckDB schemas
SELECT 
    file_path,
    function_name,
    ast_extract_subtree(ast, func_node) as implementation
FROM read_ast_objects('src/**/*.cpp')
WHERE ast_contains_pattern(ast, 'make_pair.*LogicalType');
```

### 2. Find Missing Taxonomy Field Exposure
```sql
-- Find functions that create schemas but don't include taxonomy fields
WITH schema_functions AS (
    SELECT * FROM read_ast_objects('src/**/*.cpp')
    WHERE ast_contains_calls(ast, ['make_pair', 'push_back'])
),
taxonomy_fields AS (
    SELECT * FROM schema_functions
    WHERE ast_contains_identifiers(ast, ['kind', 'semantic_id', 'universal_flags'])
)
SELECT 
    s.file_path,
    'Missing taxonomy' as issue
FROM schema_functions s
LEFT JOIN taxonomy_fields t ON s.file_path = t.file_path
WHERE t.file_path IS NULL;
```

### 3. Analyze Our Own Architecture
```sql
-- How many parser-related functions do we have?
SELECT 
    language,
    count(*) as functions,
    avg(complexity_score) as avg_complexity
FROM read_ast_objects('src/**/*.cpp'),
     ast_to_complexity_metrics(ast)
WHERE function_name LIKE '%Parser%'
GROUP BY language;
```

### 4. Track Technical Debt
```sql
-- Find all TODO/FIXME comments with context
SELECT 
    file_path,
    line_number,
    surrounding_function,
    comment_text
FROM read_ast_objects('src/**/*.cpp')
WHERE ast_contains_pattern(ast, '(TODO|FIXME|HACK)');
```

### 5. Cross-Language Consistency
```sql
-- Are our language handlers implemented consistently?
WITH handlers AS (
    SELECT * FROM read_ast_objects('src/language_handler.cpp')
    WHERE class_name LIKE '%LanguageHandler'
)
SELECT 
    class_name,
    [method.name for method in ast_get_methods(ast)] as methods,
    length(methods) as method_count
FROM handlers
ORDER BY method_count;
```

## Benefits
1. **Validation** - If our tool can analyze itself, it works on real C++ code
2. **Discovery** - Find patterns and inconsistencies in our own codebase  
3. **Maintenance** - Automatically track technical debt and TODOs
4. **Documentation** - Generate architectural insights from the code itself

## Implementation Steps
1. **Get taxonomy fields exposed** (prerequisite)
2. **Test on simple C++ files** first
3. **Build up complex queries** incrementally
4. **Create saved views** for common analyses
5. **Integration with development workflow** (CI checks, etc.)

## Success Criteria
- Can find all functions that should expose taxonomy fields
- Can analyze our parser ownership refactor automatically
- Can track the evolution of our codebase over time
- Developers actually use it for code exploration

## Future Applications
- **Code review assistance** - "Show me all functions similar to this one"
- **Refactoring validation** - "Did this change affect the expected files?"
- **Architecture enforcement** - "New parsers must implement these methods"
- **Performance analysis** - "Which functions have grown too complex?"