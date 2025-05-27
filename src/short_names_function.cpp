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
            throw InvalidInputException("Chain methods SQL file not found in embedded resources");
        }
        
        // Split and execute the chain methods SQL statements
        auto statements = SplitSQLStatements(chain_methods_sql);
        
        for (const auto &statement : statements) {
            // Skip empty statements
            if (statement.empty() || statement.find_first_not_of(" \t\n\r") == string::npos) {
                continue;
            }
            
            auto load_result = conn->Query(statement);
            if (load_result->HasError()) {
                throw InvalidInputException("Failed to register short names: " + load_result->GetError());
            }
        }
        
        // Function returns void - no output
        
    } catch (const std::exception &e) {
        throw InvalidInputException("Error registering short names: " + string(e.what()));
    }
}

void RegisterDuckDBASTShortNamesFunction(DatabaseInstance &db) {
    // Register the function
    ScalarFunction short_names_func(
        "duckdb_ast_register_short_names",
        {}, // No input parameters
        LogicalType::SQLNULL, // Returns void/NULL - no output
        RegisterShortNamesFunction
    );
    
    ExtensionUtil::RegisterFunction(db, short_names_func);
}

} // namespace duckdb