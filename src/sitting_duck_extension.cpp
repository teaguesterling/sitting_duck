#define DUCKDB_EXTENSION_MAIN

#include "sitting_duck_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/extension_helper.hpp"
#include "duckdb/function/pragma_function.hpp"
#include "duckdb/main/connection.hpp"
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
void RegisterLanguageRegistrationFunction(ExtensionLoader &loader);
// Temporarily disabled:
// void RegisterASTObjectsFunction(ExtensionLoader &loader);
// void RegisterASTHelperFunctions(ExtensionLoader &loader);

static void PragmaEnableDynamicPredicates(ClientContext &context, const FunctionParameters &parameters) {
	bool loaded = ExtensionHelper::TryAutoLoadExtension(DatabaseInstance::GetDatabase(context), "func_apply");
	if (!loaded) {
		throw InvalidInputException(
		    "sitting_duck_enable_dynamic_predicates: func_apply extension is not available.\n"
		    "Install it with: INSTALL func_apply FROM community;\n"
		    "Then restart your session and run: PRAGMA sitting_duck_enable_dynamic_predicates;");
	}
	auto conn = make_uniq<Connection>(DatabaseInstance::GetDatabase(context));
	conn->Query("CREATE OR REPLACE MACRO ast_dispatch_predicate(fn, node, arg) AS "
	            "(apply(fn, node, arg)::BOOLEAN)");
}

static void LoadInternal(ExtensionLoader &loader) {
	// Best-effort load of core_functions for the user's convenience (bitwise
	// operators etc. in DuckDB v1.5+ live there). Our own embedded SQL macros
	// deliberately do NOT depend on it (issue #82): this call resolves
	// core_functions from ~/.duckdb/extensions/<version>/<platform>/, which
	// only works when the running version stamp matches an installed release
	// (e.g. a clean-tag or OVERRIDE_GIT_DESCRIBE build). In a from-source dev
	// build the stamp is a git-describe string (or the v0.0.1 shallow-clone
	// fallback), no such directory exists, and this call silently no-ops —
	// so anything registered in LoadInternal must use always-available
	// functions only. CI enforces that via the from-source-load job.
	// TryAutoLoadAvailableExtension is noexcept and doesn't respect the user's
	// autoload_known_extensions setting (unlike TryAutoLoadExtension).
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

	// Register register_language() for runtime-loaded tree-sitter grammars
	RegisterLanguageRegistrationFunction(loader);

	// Register SQL macros for natural AST querying (depends on functions above)
	RegisterASTSQLMacros(loader);

	// Register pragma to enable dynamic custom predicate dispatch via func_apply
	loader.RegisterFunction(
	    PragmaFunction::PragmaStatement("sitting_duck_enable_dynamic_predicates", PragmaEnableDynamicPredicates));

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
