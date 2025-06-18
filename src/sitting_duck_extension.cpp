#define DUCKDB_EXTENSION_MAIN

#include "sitting_duck_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/main/extension_helper.hpp"
#include "read_ast_objects_hybrid.hpp"
#include "ast_sql_macros.hpp"
// #include "short_names_function.hpp" // Removed
#include "parse_ast_function.hpp"
#include "semantic_type_functions.hpp"

namespace duckdb {

// Forward declaration of registration functions
void RegisterReadASTFunction(DatabaseInstance &instance);           // Streaming-based implementation
// void RegisterReadASTStreamingFunction(DatabaseInstance &instance);  // Removed - redundant
// void RegisterReadASTObjectsHybridFunction(DatabaseInstance &instance); // Removed - unused
void RegisterASTSQLMacros(DatabaseInstance &instance);
// void RegisterDuckDBASTShortNamesFunction(DatabaseInstance &instance); // Removed
void RegisterASTSupportedLanguagesFunction(DatabaseInstance &instance);
// Temporarily disabled:
// void RegisterASTObjectsFunction(DatabaseInstance &instance);
// void RegisterASTHelperFunctions(DatabaseInstance &instance);

static void LoadInternal(DatabaseInstance &instance) {
	// Register the read_ast table function (streaming-based)
	RegisterReadASTFunction(instance);
	
	// RegisterReadASTStreamingFunction(instance); // Removed - redundant with read_ast
	
	// RegisterReadASTObjectsHybridFunction(instance); // Removed - unused by CLI/queries
	
	// Register the parse_ast scalar function
	ParseASTFunction::Register(instance);
	
	// Register SQL macros for natural AST querying
	RegisterASTSQLMacros(instance);
	
	// RegisterDuckDBASTShortNamesFunction(instance); // Removed - short names not needed
	
	// Register semantic type utility functions
	RegisterSemanticTypeFunctions(instance);
	
	// Register supported languages function
	RegisterASTSupportedLanguagesFunction(instance);
	
	// Short names system removed for simplicity
	
	// TODO: Re-enable once we fix the issues
	// Register AST helper functions  
	// RegisterASTHelperFunctions(instance);
}

void SittingDuckExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}

string SittingDuckExtension::Name() {
	return "sitting_duck";
}

string SittingDuckExtension::Version() const {
#ifdef EXT_VERSION_DUCKDB_AST
	return EXT_VERSION_DUCKDB_AST;
#else
	return "0.1.0";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_EXTENSION_API void duckdb_ast_init(duckdb::DatabaseInstance &db) {
	duckdb::DuckDB db_wrapper(db);
	db_wrapper.LoadExtension<duckdb::SittingDuckExtension>();
}

DUCKDB_EXTENSION_API const char *duckdb_ast_version() {
	return duckdb::DuckDB::LibraryVersion();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif