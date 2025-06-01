# Taxonomy Fields Exposure Audit

## Problem
We've implemented the KIND taxonomy system in C++ and created SQL macros that expect taxonomy fields (`kind`, `universal_flags`, `semantic_id`), but it's unclear which functions actually expose these fields.

## Investigation Needed
1. **Which functions populate taxonomy fields?**
   - `read_ast()` - flat table format
   - `read_ast_objects()` - struct array format  
   - `parse_ast()` - JSON format
   - Internal functions like `read_ast_function.cpp`

2. **Which functions expose taxonomy fields in their schema?**
   - Current `read_ast` schema: `node_id, type, normalized_type, name, file_path, start_line, ...`
   - Current `read_ast_objects` schema: `STRUCT(node_id, type, name, start_line, ...)`
   - Missing: `kind, universal_flags, semantic_id, super_type, arity_bin`

3. **Are taxonomy fields being calculated but not exposed?**
   - Code in `read_ast_function.cpp` shows taxonomy calculation
   - But output schemas don't include these fields
   - Need to verify if fields are computed but dropped

## Expected Behavior
Users should be able to query:
```sql
-- This should work but currently fails
SELECT 
    [n for n in nodes if n.kind = 4] as functions  -- FUNCTION_DEF
FROM read_ast_objects('file.py');

-- Or with macros:
SELECT filter_by_kind(ast.nodes, 4) as functions
FROM (SELECT pack(file_path, language, nodes) as ast FROM read_ast_objects('file.py'));
```

## Action Items
1. **Audit all read/parse functions** to see which expose taxonomy
2. **Update schemas** to include taxonomy fields where missing
3. **Test taxonomy macros** with real data once fields are exposed
4. **Document** which functions provide which fields

## Meta-Goal
Eventually use THIS extension to analyze its own codebase:
```sql
-- Find all functions that should expose taxonomy fields
SELECT 
    file_path,
    ast_extract_subtree(ast, func_node) as implementation
FROM read_ast_objects('src/**/*.cpp')
WHERE /* function that creates DuckDB schemas and should include taxonomy */
```

## Priority
High - Our new taxonomy macros are useless without the underlying data fields.