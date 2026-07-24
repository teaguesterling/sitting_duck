#include "dynamic_library_loader.hpp"
#include "duckdb/common/exception.hpp"

#ifndef __EMSCRIPTEN__
#include "duckdb/common/dl.hpp"
#endif

namespace duckdb {

#ifdef __EMSCRIPTEN__

void *DynamicLibraryLoader::LoadGrammarLibrary(const string &path) {
	throw NotImplementedException("register_language: loading grammar libraries is not supported on WASM builds");
}

void *DynamicLibraryLoader::ResolveSymbol(void *handle, const string &symbol, const string &path) {
	throw NotImplementedException("register_language: loading grammar libraries is not supported on WASM builds");
}

#else

void *DynamicLibraryLoader::LoadGrammarLibrary(const string &path) {
	void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		throw InvalidInputException("Failed to load grammar library '%s': %s", path, GetDLError());
	}
	return handle;
}

void *DynamicLibraryLoader::ResolveSymbol(void *handle, const string &symbol, const string &path) {
	void *address = dlsym(handle, symbol.c_str());
	if (!address) {
		throw InvalidInputException("Failed to resolve symbol '%s' in grammar library '%s': %s", symbol, path,
		                            GetDLError());
	}
	return address;
}

#endif

} // namespace duckdb
