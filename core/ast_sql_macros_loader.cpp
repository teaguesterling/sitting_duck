#include "duckdb.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/common/file_system.hpp"
#include <fstream>
#include <sstream>

namespace duckdb {

static string ReadSQLFile(const string &filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		throw IOException("Failed to open SQL macro file: " + filename);
	}

	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

static void LoadSQLMacroFile(Connection &conn, const string &filename) {
	try {
		string sql_content = ReadSQLFile(filename);
		auto result = conn.Query(sql_content);
		if (result->HasError()) {
			// Log error but don't fail extension load
			// Some macros might fail if dependencies aren't available
			// TODO: Add proper logging
		}
	} catch (const Exception &e) {
		// File might not exist in all installations
		// TODO: Add proper logging
	}
}

void RegisterASTSQLMacros(DatabaseInstance &instance) {
	// Get a connection to execute SQL
	auto conn = make_uniq<Connection>(instance);

	// First, register the core macros that are always needed
	// These are still hardcoded for reliability
	vector<string> core_macro_definitions = {// Find all nodes of a specific type (supports list parameters)
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

	                                         // Safe find type (returns empty array instead of NULL)
	                                         R"(
        CREATE OR REPLACE MACRO ast_safe_find_type(nodes, node_type) AS (
            COALESCE(ast_find_type(nodes, node_type), '[]'::JSON)
        )
        )"};

	// Register core macros
	for (const auto &macro_sql : core_macro_definitions) {
		auto result = conn->Query(macro_sql);
		if (result->HasError()) {
			// Log error but don't fail extension load
			continue;
		}
	}

	// Then load additional macros from SQL files
	// These paths would need to be adjusted based on installation location
	vector<string> macro_files = {"sql_macros/core_macros.sql", "sql_macros/source_macros.sql",
	                              "sql_macros/structure_macros.sql", "sql_macros/extract_macros.sql",
	                              "sql_macros/ai_macros.sql"};

	for (const auto &file : macro_files) {
		LoadSQLMacroFile(*conn, file);
	}
}

} // namespace duckdb
