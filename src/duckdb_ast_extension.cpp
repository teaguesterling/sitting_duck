#define DUCKDB_EXTENSION_MAIN

#include "duckdb_ast_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/main/extension_helper.hpp"
#include "read_ast_objects_hybrid.hpp"

namespace duckdb {

// Forward declaration of registration functions
void RegisterReadASTFunction(DatabaseInstance &instance);
void RegisterReadASTObjectsHybridFunction(DatabaseInstance &instance);
// Temporarily disabled:
// void RegisterASTObjectsFunction(DatabaseInstance &instance);
// void RegisterASTHelperFunctions(DatabaseInstance &instance);

static void LoadInternal(DatabaseInstance &instance) {
	// Auto-load JSON extension dependency
	ExtensionHelper::AutoLoadExtension(instance, "json");
	
	// Register the read_ast table function
	RegisterReadASTFunction(instance);
	
	// Register the hybrid read_ast_objects table function
	RegisterReadASTObjectsHybridFunction(instance);
	
	// TODO: Re-enable once we fix the issues
	// Register AST helper functions  
	// RegisterASTHelperFunctions(instance);
}

void DuckdbAstExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}

string DuckdbAstExtension::Name() {
	return "duckdb_ast";
}

string DuckdbAstExtension::Version() const {
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
	db_wrapper.LoadExtension<duckdb::DuckdbAstExtension>();
}

DUCKDB_EXTENSION_API const char *duckdb_ast_version() {
	return duckdb::DuckDB::LibraryVersion();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif