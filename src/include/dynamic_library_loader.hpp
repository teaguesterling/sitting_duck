#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Loads tree-sitter grammar shared libraries for register_language().
// Handles are intentionally never closed: adapters and in-flight parses keep
// pointers into the loaded TSLanguage, so unloading would crash the process.
class DynamicLibraryLoader {
public:
	// dlopen/LoadLibrary the given path. Throws InvalidInputException with the
	// platform error text on failure; NotImplementedException on WASM builds.
	static void *LoadGrammarLibrary(const string &path);

	// Resolve a symbol from a handle returned by LoadGrammarLibrary. Throws
	// InvalidInputException naming the symbol and library path on failure.
	static void *ResolveSymbol(void *handle, const string &symbol, const string &path);
};

} // namespace duckdb
