#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Register the duckdb_ast_register_short_names() scalar function
void RegisterDuckDBASTShortNamesFunction(DatabaseInstance &db);

} // namespace duckdb