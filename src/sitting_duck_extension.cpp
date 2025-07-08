#define DUCKDB_EXTENSION_MAIN

#include "sitting_duck_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_helper.hpp"
// #include "read_ast_objects_hybrid.hpp" // Removed
#include "ast_sql_macros.hpp"
// #include "short_names_function.hpp" // Removed
#include "parse_ast_function.hpp"
#include "semantic_type_functions.hpp"

namespace duckdb {

// Forward declaration of registration functions
void RegisterReadASTFunction(ExtensionLoader &loader);           // Streaming-based implementation
// void RegisterReadASTStreamingFunction(ExtensionLoader &loader);  // Removed - redundant
// void RegisterReadASTObjectsHybridFunction(ExtensionLoader &loader); // Removed - unused
void RegisterASTSQLMacros(ExtensionLoader &loader);
// void RegisterDuckDBASTShortNamesFunction(ExtensionLoader &loader); // Removed
void RegisterASTSupportedLanguagesFunction(ExtensionLoader &loader);
// Temporarily disabled:
// void RegisterASTObjectsFunction(ExtensionLoader &loader);
// void RegisterASTHelperFunctions(ExtensionLoader &loader);

static void LoadInternal(ExtensionLoader &loader) {
	// Register the read_ast table function (streaming-based)
	RegisterReadASTFunction(loader);
	
	// RegisterReadASTStreamingFunction(loader); // Removed - redundant with read_ast
	
	// RegisterReadASTObjectsHybridFunction(loader); // Removed - unused by CLI/queries
	
	// Register the parse_ast scalar function
	ParseASTFunction::Register(loader);
	
	// Register SQL macros for natural AST querying
	RegisterASTSQLMacros(loader);
	
	// RegisterDuckDBASTShortNamesFunction(loader); // Removed - short names not needed
	
	// Register semantic type utility functions
	RegisterSemanticTypeFunctions(loader);
	
	// Register supported languages function
	RegisterASTSupportedLanguagesFunction(loader);
	
	// Short names system removed for simplicity
	
	// TODO: Re-enable once we fix the issues
	// Register AST helper functions  
	// RegisterASTHelperFunctions(loader);
}

void SittingDuckExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
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
	db_wrapper.LoadStaticExtension<duckdb::SittingDuckExtension>();
}

DUCKDB_EXTENSION_API const char *duckdb_ast_version() {
	return duckdb::DuckDB::LibraryVersion();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif