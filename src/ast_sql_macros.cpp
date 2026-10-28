#include "duckdb.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/main/database.hpp"
#include "duckdb/main/connection.hpp"
#include "duckdb/main/extension_helper.hpp"
#include "duckdb/catalog/catalog.hpp"
#include "duckdb/catalog/catalog_entry/function_entry.hpp"
#include "duckdb/main/database_manager.hpp"
#include "duckdb/main/client_context.hpp"
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
	// Ensure core_functions extension is loaded - needed for bitwise operators (&, |) used in SQL macros
	ExtensionHelper::TryAutoLoadExtension(loader.GetDatabaseInstance(), "core_functions");

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

	// #170: give the SQL macros a description and examples in duckdb_functions().
	//
	// The C++-registered functions have carried both since #170; the 96 SQL
	// macros carried neither, which is the half that was never done. It matters
	// because duckdb_functions() is the only surface an agent connected to a
	// database can read -- the README and the registry's extended_description
	// are not reachable from SQL.
	//
	// Macros created through a Connection land in the ATTACHED database
	// (memory.main), not the system catalog, so the lookup uses the default
	// catalog. Looking in Catalog::GetSystemCatalog finds nothing, and with
	// OnEntryNotFound::RETURN_NULL that miss is silent.
	//
	// MacroCatalogEntry derives from FunctionEntry, whose `descriptions` vector
	// is what duckdb_functions() reads (ExtractFunctionData), so decorating the
	// entry after creation is enough -- the macros keep their SQL definitions.
	{
		auto &context = *conn->context;
		context.RunFunctionInTransaction([&]() {
			// Macros created through a Connection live in the ATTACHED database,
			// not the system catalog.
			auto &catalog = Catalog::GetCatalog(context, DatabaseManager::GetDefaultDatabase(context));
			for (const auto &doc : EMBEDDED_MACRO_DOCS) {
				// A macro is registered as exactly one of these two. The
				// templated overload is the one that can return null -- the
				// CatalogType overload throws, which would make "look for a
				// scalar macro first" impossible.
				// Use the CatalogType overload, not the templated GetEntry<T>: the
				// template ODR-uses T::Name, which is not exported to extensions
				// and fails at LINK time with an undefined reference to
				// ScalarMacroCatalogEntry::Name.
				optional_ptr<CatalogEntry> found =
				    catalog.GetEntry(context, CatalogType::MACRO_ENTRY, DEFAULT_SCHEMA, doc.name,
				                     OnEntryNotFound::RETURN_NULL);
				if (!found) {
					found = catalog.GetEntry(context, CatalogType::TABLE_MACRO_ENTRY, DEFAULT_SCHEMA, doc.name,
					                         OnEntryNotFound::RETURN_NULL);
				}
				if (!found) {
					// DERIVATION INVARIANT, not a soft failure: EMBEDDED_MACRO_DOCS
					// is generated from the same .sql files that define the macros,
					// so a documented macro that does not exist means the generator
					// and the loader disagree. Throwing here is the same choice the
					// taxonomy and embedded-header sync checks make -- a silent skip
					// is how this class of bug stays invisible.
					throw InternalException(
					    "sitting_duck: documented macro '%s' is not in the catalog; "
					    "EMBEDDED_MACRO_DOCS is out of sync with src/sql_macros/ "
					    "(re-run scripts/embed_sql_macros.py)",
					    doc.name);
				}
				FunctionDescription description;
				description.description = doc.description;
				// FunctionDescription::examples is a duckdb::vector; the generated
				// table is a std::vector, so copy rather than assign.
				for (const auto &example : doc.examples) {
					description.examples.push_back(example);
				}
				description.categories.push_back("sitting_duck");
				found->Cast<FunctionEntry>().descriptions.push_back(std::move(description));
			}
		});
	}

	// Register no-op stub for custom predicate dispatch.
	// Replaced with real apply()-based dispatch by PRAGMA sitting_duck_enable_dynamic_predicates.
	conn->Query("CREATE OR REPLACE MACRO ast_dispatch_predicate(fn, node, arg) AS (false)");
}

} // namespace duckdb
