#define DUCKDB_EXTENSION_MAIN

#include "duckdb_ast_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_util.hpp"

namespace duckdb {

// Forward declaration of registration functions
void RegisterReadASTFunction(DatabaseInstance &instance);

static void LoadInternal(DatabaseInstance &instance) {
	// Register the read_ast table function
	RegisterReadASTFunction(instance);
}

void DuckDBASTExtension::Load(DuckDB &db) {
	LoadInternal(*db.instance);
}

string DuckDBASTExtension::Name() {
	return "duckdb_ast";
}

string DuckDBASTExtension::Version() const {
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
	db_wrapper.LoadExtension<duckdb::DuckDBASTExtension>();
}

DUCKDB_EXTENSION_API const char *duckdb_ast_version() {
	return duckdb::DuckDB::LibraryVersion();
}

}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif