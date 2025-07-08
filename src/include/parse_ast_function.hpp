#pragma once

#include "duckdb.hpp"
#include "duckdb/function/scalar_function.hpp"

namespace duckdb {

class ParseASTFunction {
public:
    static void Register(ExtensionLoader &loader);
    
private:
    static void ParseASTScalarFunction(DataChunk &args, ExpressionState &state, Vector &result);
};

} // namespace duckdb