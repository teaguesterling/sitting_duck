-- Comparison of different peek modes for the same function

-- Default (auto/120 char)
SELECT 'default' as mode, LENGTH(peek) as len, peek 
FROM read_ast('src/unified_ast_backend.cpp') 
WHERE type = 'function_definition' LIMIT 1;

-- Smart mode  
SELECT 'smart' as mode, LENGTH(peek) as len, peek 
FROM read_ast('src/unified_ast_backend.cpp', peek_mode := 'smart') 
WHERE type = 'function_definition' LIMIT 1;

-- Compact mode
SELECT 'compact' as mode, LENGTH(peek) as len, peek 
FROM read_ast('src/unified_ast_backend.cpp', peek_mode := 'compact') 
WHERE type = 'function_definition' LIMIT 1;

-- Signature mode
SELECT 'signature' as mode, LENGTH(peek) as len, peek 
FROM read_ast('src/unified_ast_backend.cpp', peek_mode := 'signature') 
WHERE type = 'function_definition' LIMIT 1;

-- Line mode
SELECT 'line' as mode, LENGTH(peek) as len, peek 
FROM read_ast('src/unified_ast_backend.cpp', peek_mode := 'line') 
WHERE type = 'function_definition' LIMIT 1;

-- None mode (NULL)
SELECT 'none' as mode, LENGTH(peek) as len, peek 
FROM read_ast('src/unified_ast_backend.cpp', peek_mode := 'none') 
WHERE type = 'function_definition' LIMIT 1;