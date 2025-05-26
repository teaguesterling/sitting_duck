#include "duckdb.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"

namespace duckdb {

static void RegisterShortNamesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &context = state.GetContext();
    auto &db = DatabaseInstance::GetDatabase(context);
    auto conn = make_uniq<Connection>(db);
    
    try {
        // Query for all ast_* macros (excluding internal ones)
        auto query_result = conn->Query(R"(
            SELECT function_name 
            FROM duckdb_functions() 
            WHERE function_name LIKE 'ast_%' 
              AND function_type = 'macro' 
              AND function_name NOT LIKE '_ast_internal_%'
            ORDER BY function_name
        )");
        
        if (query_result->HasError()) {
            result.SetValue(0, Value("Error querying functions: " + query_result->GetError()));
            return;
        }
        
        int created = 0;
        int failed = 0;
        string error_details = "";
        
        // For now, create simple aliases - we'll enhance this to handle parameters properly later
        while (auto chunk = query_result->Fetch()) {
            for (idx_t i = 0; i < chunk->size(); i++) {
                auto func_name = chunk->GetValue(0, i).ToString();
                if (func_name.substr(0, 4) == "ast_") {
                    auto short_name = func_name.substr(4); // Remove "ast_" prefix
                    
                    // TODO: Handle parameter lists properly
                    // For now, create basic aliases for parameterless functions
                    string alias_sql = "CREATE OR REPLACE MACRO " + short_name + "() AS " + func_name + "()";
                    
                    auto alias_result = conn->Query(alias_sql);
                    if (alias_result->HasError()) {
                        failed++;
                        if (error_details.length() < 200) { // Keep error message reasonable
                            error_details += short_name + " (failed), ";
                        }
                    } else {
                        created++;
                    }
                }
            }
        }
        
        string message = "Created " + std::to_string(created) + " short name aliases";
        if (failed > 0) {
            message += ", " + std::to_string(failed) + " failed";
            if (!error_details.empty()) {
                message += " (" + error_details + ")";
            }
        }
        message += ". Note: Parameter handling is not yet implemented.";
        
        result.SetValue(0, Value(message));
        
    } catch (const std::exception &e) {
        result.SetValue(0, Value("Error: " + string(e.what())));
    }
}

void RegisterDuckDBASTShortNamesFunction(DatabaseInstance &db) {
    // Register the function
    ScalarFunction short_names_func(
        "duckdb_ast_register_short_names",
        {}, // No input parameters
        LogicalType::VARCHAR, // Returns a message
        RegisterShortNamesFunction
    );
    
    ExtensionUtil::RegisterFunction(db, short_names_func);
}

} // namespace duckdb