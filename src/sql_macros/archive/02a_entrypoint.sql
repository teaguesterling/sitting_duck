-- AST Entrypoint and Chaining Support
-- Provides the ast() function for natural chaining syntax

-- ===================================
-- MAIN ENTRYPOINT
-- ===================================

-- Universal entrypoint that normalizes input to struct array
CREATE OR REPLACE MACRO ast(input) AS (
    -- Simply return the input as-is since nodes from read_ast_objects is already a struct array
    -- This macro now mainly serves as a consistent entry point for chaining
    input
);

-- ===================================
-- CHAIN METHODS
-- ===================================
-- Note: Chain methods (unprefixed names) are now created dynamically
-- by the duckdb_ast_register_short_names() C++ function