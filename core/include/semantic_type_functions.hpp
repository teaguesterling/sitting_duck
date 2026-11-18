#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Register semantic type utility functions
void RegisterSemanticTypeFunctions(ExtensionLoader &loader);

} // namespace duckdb
