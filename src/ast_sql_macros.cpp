#include "duckdb.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/connection.hpp"
#include "embedded_sql_macros.hpp"

namespace duckdb {

void RegisterASTSQLMacros(DatabaseInstance &instance) {
    // Get a connection to execute SQL
    auto conn = make_uniq<Connection>(instance);
    
    // Load embedded SQL macros
    for (const auto &macro_pair : EMBEDDED_SQL_MACROS) {
        const string &filename = macro_pair.first;
        const string &sql_content = macro_pair.second;
        
        auto result = conn->Query(sql_content);
        if (result->HasError()) {
            // Throw error to see what's failing
            throw InvalidInputException("Failed to register macro from %s: %s", filename, result->GetError());
        }
    }
    
    // If embedded macros were loaded, we're done
    return;
    
    // SQL macros to register
    vector<string> macro_definitions = {
        // Find all nodes of a specific type
        R"(
        CREATE OR REPLACE MACRO ast_find_type(nodes, node_type) AS (
            (SELECT json_group_array(je.value) 
             FROM json_each(nodes) AS je
             WHERE json_extract_string(je.value, '$.type') = node_type)
        )
        )",
        
        // Get all function names
        R"(
        CREATE OR REPLACE MACRO ast_function_names(nodes) AS (
            (SELECT json_group_array(json_extract_string(je.value, '$.name'))
             FROM json_each(nodes) AS je
             WHERE json_extract_string(je.value, '$.type') = 'function_definition'
               AND json_extract_string(je.value, '$.name') IS NOT NULL)
        )
        )",
        
        // Get all class names
        R"(
        CREATE OR REPLACE MACRO ast_class_names(nodes) AS (
            (SELECT json_group_array(json_extract_string(je.value, '$.name'))
             FROM json_each(nodes) AS je
             WHERE json_extract_string(je.value, '$.type') = 'class_definition'
               AND json_extract_string(je.value, '$.name') IS NOT NULL)
        )
        )",
        
        // Count nodes by type
        R"(
        CREATE OR REPLACE MACRO ast_type_counts(nodes) AS (
            (SELECT json_group_object(node_type, cnt)
             FROM (
                 SELECT json_extract_string(je.value, '$.type') as node_type, COUNT(*) as cnt
                 FROM json_each(nodes) AS je
                 GROUP BY node_type
             ))
        )
        )",
        
        // Get nodes at specific depth
        R"(
        CREATE OR REPLACE MACRO ast_at_depth(nodes, target_depth) AS (
            (SELECT json_group_array(je.value)
             FROM json_each(nodes) AS je
             WHERE json_extract(je.value, '$.depth')::INTEGER = target_depth)
        )
        )",
        
        // Get all identifiers
        R"(
        CREATE OR REPLACE MACRO ast_identifiers(nodes) AS (
            (SELECT json_group_array(json_extract_string(je.value, '$.name'))
             FROM json_each(nodes) AS je
             WHERE json_extract_string(je.value, '$.type') = 'identifier'
               AND json_extract_string(je.value, '$.name') IS NOT NULL)
        )
        )",
        
        // Get all string literals
        R"(
        CREATE OR REPLACE MACRO ast_strings(nodes) AS (
            (SELECT json_group_array(json_extract_string(je.value, '$.content'))
             FROM json_each(nodes) AS je
             WHERE json_extract_string(je.value, '$.type') = 'string'
               AND json_extract_string(je.value, '$.content') IS NOT NULL)
        )
        )",
        
        // Get function details with proper type casting
        R"(
        CREATE OR REPLACE MACRO ast_function_details(nodes) AS (
            (SELECT json_group_array(
                json_object(
                    'name', json_extract_string(je.value, '$.name'),
                    'start_line', json_extract(je.value, '$.start.line')::INTEGER,
                    'end_line', json_extract(je.value, '$.end.line')::INTEGER,
                    'depth', json_extract(je.value, '$.depth')::INTEGER,
                    'id', json_extract(je.value, '$.id')::INTEGER
                ))
             FROM json_each(nodes) AS je
             WHERE json_extract_string(je.value, '$.type') = 'function_definition'
               AND json_extract_string(je.value, '$.name') IS NOT NULL)
        )
        )",
        
        // Get a summary of the AST structure
        R"(
        CREATE OR REPLACE MACRO ast_summary(nodes) AS (
            json_object(
                'total_nodes', json_array_length(nodes),
                'node_types', ast_type_counts(nodes),
                'functions', ast_function_names(nodes),
                'classes', ast_class_names(nodes),
                'max_depth', (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
                              FROM json_each(nodes) AS je),
                'function_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                                  WHERE json_extract_string(je.value, '$.type') = 'function_definition'),
                'class_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                               WHERE json_extract_string(je.value, '$.type') = 'class_definition')
            )
        )
        )",
        
        // Safe find type (returns empty array instead of NULL)
        R"(
        CREATE OR REPLACE MACRO ast_safe_find_type(nodes, node_type) AS (
            COALESCE(ast_find_type(nodes, node_type), '[]'::JSON)
        )
        )",
        
        // Find nodes at a specific line
        R"(
        CREATE OR REPLACE MACRO ast_at_line(nodes, line_number) AS (
            (SELECT json_group_array(je.value)
             FROM json_each(nodes) AS je
             WHERE json_extract(je.value, '$.start.line')::INTEGER <= line_number
               AND json_extract(je.value, '$.end.line')::INTEGER >= line_number)
        )
        )",
        
        // Get nodes in line number range
        R"(
        CREATE OR REPLACE MACRO ast_in_line_range(nodes, start_line, end_line) AS (
            (SELECT json_group_array(je.value)
             FROM json_each(nodes) AS je
             WHERE json_extract(je.value, '$.start.line')::INTEGER >= start_line
               AND json_extract(je.value, '$.end.line')::INTEGER <= end_line)
        )
        )",
        
        // Find nodes containing specific text
        R"(
        CREATE OR REPLACE MACRO ast_contains_text(nodes, search_text) AS (
            (SELECT json_group_array(je.value)
             FROM json_each(nodes) AS je
             WHERE COALESCE(json_extract_string(je.value, '$.name'), '') LIKE '%' || search_text || '%'
                OR COALESCE(json_extract_string(je.value, '$.content'), '') LIKE '%' || search_text || '%')
        )
        )",
        
        // Get complexity metrics
        R"(
        CREATE OR REPLACE MACRO ast_complexity(nodes) AS (
            json_object(
                'total_nodes', json_array_length(nodes),
                'avg_depth', (SELECT AVG(json_extract(je.value, '$.depth')::INTEGER) 
                              FROM json_each(nodes) AS je),
                'max_depth', (SELECT MAX(json_extract(je.value, '$.depth')::INTEGER) 
                              FROM json_each(nodes) AS je),
                'function_count', (SELECT COUNT(*) FROM json_each(nodes) AS je 
                                  WHERE json_extract_string(je.value, '$.type') = 'function_definition'),
                'lines_of_code', (SELECT MAX(json_extract(je.value, '$.end.line')::INTEGER) 
                                 FROM json_each(nodes) AS je)
            )
        )
        )"
    };
    
    // Register each macro
    for (const auto &macro_sql : macro_definitions) {
        auto result = conn->Query(macro_sql);
        if (result->HasError()) {
            // Log error but don't fail extension load
            // Some macros might fail if json_each isn't available
            continue;
        }
    }
}

} // namespace duckdb