#include "dynamic_library_loader.hpp"
#include "duckdb/common/exception.hpp"

#ifdef __EMSCRIPTEN__
// No dynamic loading on WASM builds
#elif defined(_WIN32)
#include "duckdb/common/windows_util.hpp"
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace duckdb {

#ifdef __EMSCRIPTEN__

void *DynamicLibraryLoader::LoadGrammarLibrary(const string &path) {
	throw NotImplementedException("register_language: loading grammar libraries is not supported on WASM builds");
}

void *DynamicLibraryLoader::ResolveSymbol(void *handle, const string &symbol, const string &path) {
	throw NotImplementedException("register_language: loading grammar libraries is not supported on WASM builds");
}

#elif defined(_WIN32)

static string GetLastErrorMessage() {
	DWORD error_code = GetLastError();
	if (error_code == 0) {
		return "unknown error";
	}
	LPSTR buffer = nullptr;
	DWORD size =
	    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                   nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, nullptr);
	string message = size ? string(buffer, size) : "error code " + std::to_string(error_code);
	if (buffer) {
		LocalFree(buffer);
	}
	return message;
}

void *DynamicLibraryLoader::LoadGrammarLibrary(const string &path) {
	auto wide_path = WindowsUtil::UTF8ToUnicode(path.c_str());
	HMODULE module = LoadLibraryW(wide_path.c_str());
	if (!module) {
		throw InvalidInputException("Failed to load grammar library '%s': %s", path, GetLastErrorMessage());
	}
	return reinterpret_cast<void *>(module);
}

void *DynamicLibraryLoader::ResolveSymbol(void *handle, const string &symbol, const string &path) {
	auto address = GetProcAddress(static_cast<HMODULE>(handle), symbol.c_str());
	if (!address) {
		throw InvalidInputException("Failed to resolve symbol '%s' in grammar library '%s': %s", symbol, path,
		                            GetLastErrorMessage());
	}
	return reinterpret_cast<void *>(address);
}

#else

void *DynamicLibraryLoader::LoadGrammarLibrary(const string &path) {
	void *handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
	if (!handle) {
		const char *error = dlerror();
		throw InvalidInputException("Failed to load grammar library '%s': %s", path,
		                            error ? error : "unknown dlopen error");
	}
	return handle;
}

void *DynamicLibraryLoader::ResolveSymbol(void *handle, const string &symbol, const string &path) {
	dlerror(); // clear any stale error before probing
	void *address = dlsym(handle, symbol.c_str());
	if (!address) {
		const char *error = dlerror();
		throw InvalidInputException("Failed to resolve symbol '%s' in grammar library '%s': %s", symbol, path,
		                            error ? error : "symbol not found");
	}
	return address;
}

#endif

} // namespace duckdb
