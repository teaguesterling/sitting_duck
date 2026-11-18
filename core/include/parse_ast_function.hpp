#pragma once

#include "duckdb.hpp"

namespace duckdb {

class ParseASTFunction {
public:
	static void Register(ExtensionLoader &loader);
};

} // namespace duckdb
