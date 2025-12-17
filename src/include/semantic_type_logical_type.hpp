#pragma once

#include "duckdb.hpp"

namespace duckdb {

// Get the SEMANTIC_TYPE logical type (UTINYINT with alias)
LogicalType SemanticTypeLogicalType();

// Check if a type is SEMANTIC_TYPE
bool IsSemanticType(const LogicalType &type);

// Register the SEMANTIC_TYPE and its cast functions
void RegisterSemanticTypeLogicalType(ExtensionLoader &loader);

} // namespace duckdb
