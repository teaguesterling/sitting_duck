#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Register semantic type utility functions
void RegisterSemanticTypeFunctions(DatabaseInstance &instance);

} // namespace duckdb