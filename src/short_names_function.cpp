#include "duckdb.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"
#include "ast_sql_macros.hpp"
#include "embedded_sql_macros.hpp"

namespace duckdb {

// Helper function to get embedded SQL macro by filename
static const char* GetEmbeddedSqlMacro(const string &filename) {
    for (const auto &macro_pair : EMBEDDED_SQL_MACROS) {
        if (macro_pair.first == filename) {
            return macro_pair.second.c_str();
        }
    }
    return nullptr;
}

// Same SQL splitter as in ast_sql_macros.cpp
static vector<string> SplitSQLStatements(const string &sql) {
    vector<string> statements;
    string current_statement;
    bool in_single_quote = false;
    bool in_double_quote = false;
    
    for (size_t i = 0; i < sql.length(); i++) {
        char ch = sql[i];
        
        // Handle quotes
        if (ch == '\'' && !in_double_quote && (i == 0 || sql[i-1] != '\\')) {
            in_single_quote = !in_single_quote;
        } else if (ch == '"' && !in_single_quote && (i == 0 || sql[i-1] != '\\')) {
            in_double_quote = !in_double_quote;
        }
        
        current_statement += ch;
        
        // If we hit a semicolon outside of quotes, it's end of statement
        if (ch == ';' && !in_single_quote && !in_double_quote) {
            // Trim whitespace
            size_t start = current_statement.find_first_not_of(" \t\n\r");
            if (start != string::npos) {
                statements.push_back(current_statement.substr(start));
            }
            current_statement.clear();
        }
    }
    
    // Don't forget the last statement if it doesn't end with semicolon
    size_t start = current_statement.find_first_not_of(" \t\n\r");
    if (start != string::npos) {
        statements.push_back(current_statement.substr(start));
    }
    
    return statements;
}

static void RegisterShortNamesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
    auto &context = state.GetContext();
    auto &db = DatabaseInstance::GetDatabase(context);
    auto conn = make_uniq<Connection>(db);
    
    try {
        // Load the chain methods SQL file
        // This file contains all the unprefixed aliases for chain methods
        const char* chain_methods_sql = GetEmbeddedSqlMacro("02b_chain_methods.sql");
        
        if (!chain_methods_sql) {
            // Fall back to loading from embedded macros if available
            result.SetValue(0, Value("Chain methods SQL file not found in embedded resources"));
            return;
        }
        
        // Split and execute the chain methods SQL statements
        auto statements = SplitSQLStatements(chain_methods_sql);
        int loaded = 0;
        int failed = 0;
        
        for (const auto &statement : statements) {
            // Skip empty statements
            if (statement.empty() || statement.find_first_not_of(" \t\n\r") == string::npos) {
                continue;
            }
            
            auto load_result = conn->Query(statement);
            if (load_result->HasError()) {
                failed++;
            } else {
                loaded++;
            }
        }
        
        // Count how many macros were created
        auto count_result = conn->Query(R"(
            SELECT COUNT(*) as cnt
            FROM duckdb_functions() 
            WHERE function_name IN (
                'get_type', 'get_names', 'get_depth', 
                'filter_pattern', 'filter_has_name',
                'nav_children', 'nav_parent', 'summary',
                'count_nodes', 'first_node', 'last_node',
                'find_type', 'find_depth', 'extract_names',
                'children', 'parent', 'len', 'size'
            )
            AND function_type = 'macro'
        )");
        
        if (count_result->HasError()) {
            result.SetValue(0, Value("Chain methods loaded but could not verify: " + count_result->GetError()));
            return;
        }
        
        auto count = count_result->GetValue(0, 0).GetValue<int32_t>();
        
        string message = "Successfully loaded " + std::to_string(loaded) + " SQL statements, ";
        message += "created " + std::to_string(count) + " chain methods. ";
        if (failed > 0) {
            message += std::to_string(failed) + " statements failed. ";
        }
        message += "\nYou can now use ast(nodes).method() syntax for chaining.";
        
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