-- Natural AST Queries for AI Agents
-- With DuckDB 1.3+ json_each and SQL macros

LOAD json;
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Load the improved SQL macros
-- [In practice, these would be loaded by the extension]

-- Example 1: Simple function listing
SELECT nodes.ast_function_names() as functions
FROM read_ast_objects('app.py', 'python');

-- Example 2: Find complex functions
SELECT func.*
FROM read_ast_objects('app.py', 'python') AS ast,
     json_each(ast.nodes.ast_function_complexity()) AS func
WHERE json_extract(func.value, '$.lines')::INTEGER > 50;

-- Example 3: Search for print statements
SELECT 
    je.key as node_index,
    json_extract(je.value, '$.start.line') as line_number
FROM read_ast_objects('script.py', 'python') AS ast,
     json_each(ast.nodes) AS je
WHERE json_extract_string(je.value, '$.type') = 'call'
  AND json_extract_string(je.value, '$.name') = 'print';

-- Example 4: Find all SQL queries in Python code
SELECT DISTINCT sql_string
FROM read_ast_objects('database.py', 'python') AS ast,
     json_each(ast.nodes.ast_strings()) AS str
WHERE str.value::VARCHAR LIKE '%SELECT%' 
   OR str.value::VARCHAR LIKE '%INSERT%'
   OR str.value::VARCHAR LIKE '%UPDATE%';

-- Example 5: Analyze code structure across multiple files
WITH code_analysis AS (
    SELECT 
        file_path,
        nodes.ast_summary() as summary,
        json_array_length(nodes.ast_todos()) as todo_count
    FROM (
        SELECT * FROM read_ast_objects('src/main.py', 'python')
        UNION ALL
        SELECT * FROM read_ast_objects('src/utils.py', 'python')
        UNION ALL
        SELECT * FROM read_ast_objects('src/models.py', 'python')
    )
)
SELECT 
    file_path,
    json_extract(summary, '$.total_nodes') as total_nodes,
    json_extract(summary, '$.functions') as functions,
    todo_count
FROM code_analysis;

-- Example 6: Find unused imports (simplified)
WITH imports AS (
    SELECT json_extract_string(je.value, '$.name') as import_name
    FROM read_ast_objects('module.py', 'python') AS ast,
         json_each(ast.nodes) AS je
    WHERE json_extract_string(je.value, '$.type') = 'import_statement'
),
identifiers AS (
    SELECT DISTINCT json_extract_string(je.value, '$.content') as identifier
    FROM read_ast_objects('module.py', 'python') AS ast,
         json_each(ast.nodes) AS je
    WHERE json_extract_string(je.value, '$.type') = 'identifier'
)
SELECT i.import_name as potentially_unused
FROM imports i
WHERE i.import_name NOT IN (SELECT identifier FROM identifiers);

-- Example 7: Extract API endpoints from Flask/FastAPI code
SELECT DISTINCT
    json_extract_string(dec.value, '$.name') as decorator,
    json_extract_string(func.value, '$.name') as endpoint_function,
    json_extract(func.value, '$.start.line') as line_number
FROM read_ast_objects('api.py', 'python') AS ast,
     json_each(ast.nodes) AS func,
     json_each(ast.nodes) AS dec
WHERE json_extract_string(func.value, '$.type') = 'function_definition'
  AND json_extract_string(dec.value, '$.type') = 'decorator'
  AND json_extract_string(dec.value, '$.name') IN ('route', 'get', 'post', 'put', 'delete')
  AND json_extract(dec.value, '$.parent_id') = json_extract(func.value, '$.id');

-- Example 8: Natural chaining with dot notation
SELECT 
    -- Count different node types
    json_extract(nodes.ast_type_counts(), '$.function_definition') as num_functions,
    json_extract(nodes.ast_type_counts(), '$.class_definition') as num_classes,
    -- Get all TODOs
    nodes.ast_todos() as todos,
    -- Check complexity
    CASE 
        WHEN node_count > 1000 THEN 'Large file - consider splitting'
        WHEN node_count > 500 THEN 'Medium complexity'
        ELSE 'Good size'
    END as recommendation
FROM read_ast_objects('large_module.py', 'python');