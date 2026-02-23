#include "duckdb.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/connection.hpp"
#include "embedded_sql_macros.hpp"
#include <sstream>
#include <vector>

namespace duckdb {

// Simple SQL statement splitter - splits on semicolons that aren't in quotes or comments
static vector<string> SplitSQLStatements(const string &sql) {
	vector<string> statements;
	string current_statement;
	bool in_single_quote = false;
	bool in_double_quote = false;
	bool in_line_comment = false;

	for (size_t i = 0; i < sql.length(); i++) {
		char ch = sql[i];

		// Check for line comment start (-- outside of quotes)
		if (!in_single_quote && !in_double_quote && !in_line_comment && ch == '-' && i + 1 < sql.length() &&
		    sql[i + 1] == '-') {
			in_line_comment = true;
		}

		// End line comment on newline
		if (in_line_comment && (ch == '\n' || ch == '\r')) {
			in_line_comment = false;
		}

		// Handle quotes (only when not in a comment)
		if (!in_line_comment) {
			if (ch == '\'' && !in_double_quote && (i == 0 || sql[i - 1] != '\\')) {
				in_single_quote = !in_single_quote;
			} else if (ch == '"' && !in_single_quote && (i == 0 || sql[i - 1] != '\\')) {
				in_double_quote = !in_double_quote;
			}
		}

		current_statement += ch;

		// If we hit a semicolon outside of quotes and comments, it's end of statement
		if (ch == ';' && !in_single_quote && !in_double_quote && !in_line_comment) {
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

void RegisterASTSQLMacros(ExtensionLoader &loader) {
	// Get a connection to execute SQL
	auto conn = make_uniq<Connection>(loader.GetDatabaseInstance());

	int total_statements = 0;
	int successful_statements = 0;

	// Load embedded SQL macros (excluding chain methods which are loaded on demand)
	for (const auto &macro_pair : EMBEDDED_SQL_MACROS) {
		const string &filename = macro_pair.first;
		const string &sql_content = macro_pair.second;

		// Skip chain methods - these are loaded by duckdb_ast_register_short_names()
		if (filename == "02b_chain_methods.sql") {
			continue;
		}

		// Split the SQL content into individual statements
		auto statements = SplitSQLStatements(sql_content);

		for (size_t i = 0; i < statements.size(); i++) {
			const auto &statement = statements[i];

			// Skip empty statements or comments
			if (statement.empty() || statement.find_first_not_of(" \t\n\r") == string::npos) {
				continue;
			}

			total_statements++;

			auto result = conn->Query(statement);
			if (result->HasError()) {
				// Throw error with context about which statement failed
				throw InvalidInputException("Failed to register macro from %s (statement %d/%d):\n"
				                            "Statement: %.200s...\n"
				                            "Error: %s",
				                            filename.c_str(), (int)(i + 1), (int)statements.size(), statement.c_str(),
				                            result->GetError().c_str());
			}
			successful_statements++;
		}
	}
}

} // namespace duckdb
