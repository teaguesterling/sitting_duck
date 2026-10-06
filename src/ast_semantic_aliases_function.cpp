#include "duckdb.hpp"
#include "duckdb_compat.hpp"
#include "function_doc_helper.hpp"
#include "include/semantic_types.hpp"
#include "include/semantic_aliases.hpp"

namespace duckdb {

// ast_semantic_aliases() — the selector vocabulary, as a table.
//
// Exists because the vocabulary used to be unreadable control flow (#188): a 44-arm
// if/else-if cascade that nothing could enumerate. That made two different things
// impossible at once — telling a mistyped class from a valid one that matched nothing,
// and documenting the set at all (22 valid classes went unmentioned in the reference).
//
// Exposing it as a table gives both a single source of truth: the selector engine
// validates `.class` names against this, and the docs are generated from it, so neither
// can drift from the other.

struct SemanticAliasesData : public GlobalTableFunctionState {
	SemanticAliasesData() : offset(0) {
	}
	idx_t offset;
};

static unique_ptr<FunctionData> SemanticAliasesBind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<CompatName> &names) {
	names.emplace_back("alias");
	return_types.emplace_back(LogicalType::VARCHAR);

	//! The user-facing spelling, e.g. `.fn` — what you actually type in a selector.
	names.emplace_back("selector");
	return_types.emplace_back(LogicalType::VARCHAR);

	names.emplace_back("resolves_to");
	return_types.emplace_back(LogicalType::VARCHAR);

	//! exact | kind | quadrant — how broadly the alias matches.
	names.emplace_back("match_kind");
	return_types.emplace_back(LogicalType::VARCHAR);

	names.emplace_back("code");
	return_types.emplace_back(LogicalType::UTINYINT);

	names.emplace_back("mask");
	return_types.emplace_back(LogicalType::UTINYINT);

	return nullptr;
}

static unique_ptr<GlobalTableFunctionState> SemanticAliasesInit(ClientContext &context,
                                                                TableFunctionInitInput &input) {
	return make_uniq<SemanticAliasesData>();
}

static void SemanticAliasesFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.global_state->Cast<SemanticAliasesData>();

	idx_t count = 0;
	for (idx_t i = data.offset; i < SEMANTIC_ALIAS_COUNT && count < STANDARD_VECTOR_SIZE; i++) {
		const auto &entry = SEMANTIC_ALIASES[i];

		string alias(entry.alias);
		output.SetValue(0, count, Value(alias));

		// The selector spelling: lowercase, dot-prefixed.
		string selector = "." + StringUtil::Lower(alias);
		output.SetValue(1, count, Value(selector));

		// What it resolves to, named at the right level of the taxonomy.
		string resolves_to;
		string match_kind;
		switch (entry.mask) {
		case 0xFF:
			match_kind = "exact";
			resolves_to = SemanticTypes::GetSemanticTypeName(entry.code);
			break;
		case 0xF0:
			match_kind = "kind";
			resolves_to = SemanticTypes::GetKindName(SemanticTypes::GetKind(entry.code));
			break;
		default:
			match_kind = "quadrant";
			resolves_to = SemanticTypes::GetSuperKindName(SemanticTypes::GetSuperKind(entry.code));
			break;
		}
		output.SetValue(2, count, Value(resolves_to));
		output.SetValue(3, count, Value(match_kind));
		output.SetValue(4, count, Value::UTINYINT(entry.code));
		output.SetValue(5, count, Value::UTINYINT(entry.mask));

		count++;
		data.offset++;
	}
	CompatSetOutputCardinality(output, count);
}

void RegisterASTSemanticAliasesFunction(ExtensionLoader &loader) {
	TableFunction function("ast_semantic_aliases", {}, SemanticAliasesFunction, SemanticAliasesBind,
	                       SemanticAliasesInit);
	RegisterDocumentedTableFunction(
	    loader, function,
	    "Return every semantic class usable in an ast_select selector (e.g. .fn, .call, .def), "
	    "what it resolves to, and how broadly it matches.",
	    {}, {"SELECT selector, resolves_to FROM ast_semantic_aliases() ORDER BY selector"},
	    {"sitting_duck", "metadata"});
}

} // namespace duckdb
