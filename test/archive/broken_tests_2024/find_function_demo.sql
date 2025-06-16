-- Demo: Find a specific function across multiple languages
-- This demonstrates the core value proposition of the AST extension

-- First, let's create some test data
.shell mkdir -p /tmp/ast_demo
.shell echo 'def hello(): print("Hello from Python")' > /tmp/ast_demo/test.py
.shell echo 'function hello() { console.log("Hello from JS"); }' > /tmp/ast_demo/test.js
.shell echo 'void hello() { printf("Hello from C++\\n"); }' > /tmp/ast_demo/test.cpp

-- Load the short names for cleaner queries
PRAGMA duckdb_ast_short_names;

-- Find all "hello" functions across languages
WITH all_files AS (
    -- Python files
    SELECT * FROM read_ast_objects('/tmp/ast_demo/test.py')
    UNION ALL
    -- JavaScript files  
    SELECT * FROM read_ast_objects('/tmp/ast_demo/test.js')
    UNION ALL
    -- C++ files
    SELECT * FROM read_ast_objects('/tmp/ast_demo/test.cpp')
),
-- Find function nodes
function_finder AS (
    SELECT 
        file_path,
        language,
        pack(file_path, language, nodes) as ast,
        nodes
    FROM all_files
)
SELECT 
    file_path,
    language,
    -- Extract just the hello function and its body
    length(extract_subtrees(
        nodes,
        list_filter(with_indices(nodes), 
            x -> x.node.type IN ('function_definition', 'function_declaration', 'function')
            AND x.node.name = 'hello'
        )
    )) as nodes_in_hello_function,
    -- Get the actual function node for inspection
    [n for n in nodes 
     WHERE n.type IN ('function_definition', 'function_declaration', 'function')
     AND n.name = 'hello'][1] as hello_function_node
FROM function_finder
WHERE list_contains(
    [n.name for n in nodes 
     WHERE n.type IN ('function_definition', 'function_declaration', 'function')],
    'hello'
);

-- Advanced: Show the implementation pattern
WITH implementations AS (
    SELECT 
        file_path,
        language,
        nodes
    FROM read_ast_objects('/tmp/ast_demo/*.py')
    UNION ALL
    SELECT * FROM read_ast_objects('/tmp/ast_demo/*.js')
)
SELECT
    language,
    count(*) as file_count,
    -- Common patterns in each language
    mode([n.type for n in nodes WHERE n.depth = 1]) as common_top_level_construct,
    -- Function definition patterns
    list_distinct([n.type for n in nodes 
                   WHERE n.type LIKE '%function%' 
                   AND n.parent_id = 0]) as function_patterns
FROM implementations
GROUP BY language;

-- The Vision: Eventually query across time
-- WITH historical_asts AS (
--     SELECT 
--         commit_hash,
--         commit_date,
--         ast
--     FROM git_ast_history('src/auth.py')
-- )
-- SELECT 
--     commit_date,
--     -- How did the authenticate function evolve?
--     length(get_functions(ast).nodes) as total_functions,
--     length(find_name(ast, 'authenticate').nodes) > 0 as has_authenticate,
--     -- Complexity over time
--     max([n.descendant_count for n in ast.nodes 
--          WHERE n.name = 'authenticate']) as authenticate_complexity
-- FROM historical_asts
-- ORDER BY commit_date;