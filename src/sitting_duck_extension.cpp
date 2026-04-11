#define DUCKDB_EXTENSION_MAIN

#include "sitting_duck_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_helper.hpp"
// #include "read_ast_objects_hybrid.hpp" // Removed
#include "ast_sql_macros.hpp"
// #include "short_names_function.hpp" // Removed
#include "parse_ast_function.hpp"
#include "parse_ast_list_function.hpp"
#include "semantic_type_functions.hpp"
#include "semantic_type_logical_type.hpp"

namespace duckdb {

// Forward declaration of registration functions
void RegisterReadASTFunction(ExtensionLoader &loader); // Streaming-based implementation
// void RegisterReadASTStreamingFunction(ExtensionLoader &loader);  // Removed - redundant
// void RegisterReadASTObjectsHybridFunction(ExtensionLoader &loader); // Removed - unused
void RegisterASTSQLMacros(ExtensionLoader &loader);
// void RegisterDuckDBASTShortNamesFunction(ExtensionLoader &loader); // Removed
void RegisterASTSupportedLanguagesFunction(ExtensionLoader &loader);
void RegisterASTTypeMapFunction(ExtensionLoader &loader);
// Temporarily disabled:
// void RegisterASTObjectsFunction(ExtensionLoader &loader);
// void RegisterASTHelperFunctions(ExtensionLoader &loader);

static void LoadInternal(ExtensionLoader &loader) {
	// Force-load core_functions before we register our SQL macros. In
	// DuckDB v1.5+ the bitwise operators (`&`, `|`, `<<`, etc.) live in
	// core_functions, and pattern_matching.sql uses `&` when comparing
	// masked-off semantic type bits. If core_functions isn't loaded yet
	// the macro registration fails at parse time with
	//   Catalog Error: Scalar Function with name "&" is not in the catalog,
	//     but it exists in the core_functions extension.
	//
	// NOTE: this call alone is not yet sufficient to make `LOAD sitting_duck`
	// work on a vanilla CLI without `LOAD core_functions` first — deferred
	// investigation. Kept in place as the starting point for the follow-up.
	// TryAutoLoadAvailableExtension is noexcept and doesn't respect the user's
	// autoload_known_extensions setting (unlike TryAutoLoadExtension), so it's
	// the right API shape even if more plumbing is still needed.
	ExtensionHelper::TryAutoLoadAvailableExtension(loader.GetDatabaseInstance(), "core_functions");

	// Register SEMANTIC_TYPE logical type and its cast functions (must be first)
	RegisterSemanticTypeLogicalType(loader);

	// Register the read_ast table function (streaming-based)
	RegisterReadASTFunction(loader);

	// RegisterReadASTStreamingFunction(loader); // Removed - redundant with read_ast

	// RegisterReadASTObjectsHybridFunction(loader); // Removed - unused by CLI/queries

	// Register the parse_ast scalar function
	ParseASTFunction::Register(loader);

	// Register parse_ast_list scalar function
	RegisterParseASTListFunction(loader);

	// Register semantic type utility functions (must be before SQL macros that depend on them)
	RegisterSemanticTypeFunctions(loader);

	// Register supported languages function
	RegisterASTSupportedLanguagesFunction(loader);

	// Register ast_type_map() for node type discovery
	RegisterASTTypeMapFunction(loader);

	// Register SQL macros for natural AST querying (depends on functions above)
	RegisterASTSQLMacros(loader);

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

DUCKDB_CPP_EXTENSION_ENTRY(sitting_duck, loader) {
	duckdb::LoadInternal(loader);
}

DUCKDB_EXTENSION_API const char *sitting_duck_version() {
	return duckdb::DuckDB::LibraryVersion();
}
}

#ifndef DUCKDB_EXTENSION_MAIN
#error DUCKDB_EXTENSION_MAIN not defined
#endif
