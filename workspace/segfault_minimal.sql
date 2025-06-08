-- Minimal reproducer for the segfault in return type extraction

-- This works:
SELECT 'Test 1: Basic join' as test;
WITH func_defs AS (
    SELECT node_id FROM read_ast('src/unified_ast_backend.cpp')
    WHERE type = 'function_definition' LIMIT 1
)
SELECT COUNT(*) FROM func_defs fd
JOIN read_ast('src/unified_ast_backend.cpp') c ON c.parent_id = fd.node_id;

-- This might cause segfault:
SELECT 'Test 2: Multiple joins' as test;
WITH func_defs AS (
    SELECT node_id FROM read_ast('src/unified_ast_backend.cpp')
    WHERE type = 'function_definition' LIMIT 2
)
SELECT COUNT(*) FROM func_defs fd
JOIN read_ast('src/unified_ast_backend.cpp') d ON d.parent_id = fd.node_id
LEFT JOIN read_ast('src/unified_ast_backend.cpp') gc ON gc.parent_id = d.node_id;

-- This is the suspected problem:
SELECT 'Test 3: Triple join with conditions' as test;
WITH func_defs AS (
    SELECT node_id FROM read_ast('src/unified_ast_backend.cpp')
    WHERE type = 'function_definition' LIMIT 2
)
SELECT COUNT(*) FROM func_defs fd
JOIN read_ast('src/unified_ast_backend.cpp') d ON d.parent_id = fd.node_id
LEFT JOIN read_ast('src/unified_ast_backend.cpp') gc ON gc.parent_id = d.node_id
LEFT JOIN read_ast('src/unified_ast_backend.cpp') pc ON pc.parent_id = d.node_id;