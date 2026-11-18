#include "duckdb.hpp"
#include "duckdb_compat.hpp"
#include "function_doc_helper.hpp"
#include "duckdb/function/table_function.hpp"
#include "include/language_adapter.hpp"
#include "include/node_config.hpp"
#include "include/semantic_types.hpp"

namespace duckdb {

// =============================================================================
// ast_type_map() — Discover node type mappings across languages
// =============================================================================
//
// Returns one row per DEF_TYPE entry across all (or a specific) language.
// Exposes the relationship between tree-sitter node types, semantic types,
// extraction strategies and flags.
//
// This is the DECLARATION SURFACE of a language module: it reports what the
// loaded map holds, not what any source file says, so it answers for a module
// whose table is assembled by the preprocessor or that ships no sources at all
// (v2.0 M3 moves language modules out of this repository). Both strategy
// columns come from the same map the extractors read.
//
// Usage:
//   SELECT * FROM ast_type_map();                          -- all languages
//   SELECT * FROM ast_type_map('python');                  -- one language
//   SELECT * FROM ast_type_map()
//     WHERE native_strategy = 'function_with_params';     -- who declares a strategy
//   SELECT * FROM ast_type_map() WHERE node_type = 'for_statement'; -- reverse lookup

struct TypeMapBindData : public TableFunctionData {
	string language_filter; // empty = all languages
};

struct TypeMapGlobalState : public GlobalTableFunctionState {
	// Precomputed rows: (language, node_type, semantic_type_name, kind, flags,
	// name_role, is_scope, is_syntax, name_strategy, native_strategy)
	struct Row {
		string language;
		string node_type;
		uint8_t semantic_type_byte;
		string semantic_type_name;
		string kind;
		string super_type;
		uint8_t flags;
		string name_role;
		bool is_scope;
		bool is_syntax;
		string name_strategy;
		string native_strategy;
	};

	vector<Row> rows;
	idx_t current_row = 0;
};

static string GetKindName(uint8_t semantic_type) {
	uint8_t kind_bits = semantic_type & 0xF0;
	switch (kind_bits) {
	case SemanticTypes::DEFINITION:
		return "definition";
	case SemanticTypes::LITERAL:
		return "literal";
	case SemanticTypes::NAME:
		return "name";
	case SemanticTypes::TYPE:
		return "type";
	case SemanticTypes::FLOW_CONTROL:
		return "flow";
	case SemanticTypes::ERROR_HANDLING:
		return "error";
	case SemanticTypes::EXTERNAL:
		return "external";
	case SemanticTypes::EXECUTION:
		return "statement";
	case SemanticTypes::ORGANIZATION:
		return "block";
	case SemanticTypes::METADATA:
		return "comment";
	case SemanticTypes::PATTERN:
		return "pattern";
	default:
		// Check super-kind for computation/transform
		uint8_t super_kind = semantic_type & 0xC0;
		if (super_kind == SemanticTypes::COMPUTATION) {
			if ((semantic_type & 0xF0) == SemanticTypes::OPERATOR)
				return "operator";
			if ((semantic_type & 0xF0) == SemanticTypes::TRANSFORM)
				return "transform";
			return "access";
		}
		if (kind_bits == SemanticTypes::PARSER_SPECIFIC)
			return "syntax";
		return "unknown";
	}
}

// <<< BEGIN GENERATED TAXONOMY: type_map_name_tables >>>
// Generated from spec/taxonomy/taxonomy.yaml by scripts/generate_taxonomy.py.
// Edit the spec, not this block.
static string GetNameRoleString(uint8_t flags) {
	uint8_t role = (flags & ASTNodeFlags::NAME_ROLE_MASK) >> 1;
	switch (role) {
	case 0:
		return "";
	case 1:
		return "reference";
	case 2:
		return "declaration";
	case 3:
		return "definition";
	default:
		return "";
	}
}

static string GetExtractionStrategyName(ExtractionStrategy strategy) {
	switch (strategy) {
	case ExtractionStrategy::NONE:
		return "none";
	case ExtractionStrategy::NODE_TEXT:
		return "node_text";
	case ExtractionStrategy::FIRST_CHILD:
		return "first_child";
	case ExtractionStrategy::FIND_IDENTIFIER:
		return "find_identifier";
	case ExtractionStrategy::FIND_PROPERTY:
		return "find_property";
	case ExtractionStrategy::FIND_ASSIGNMENT_TARGET:
		return "find_assignment_target";
	case ExtractionStrategy::FIND_QUALIFIED_IDENTIFIER:
		return "find_qualified_identifier";
	case ExtractionStrategy::FIND_IN_DECLARATOR:
		return "find_in_declarator";
	case ExtractionStrategy::FIND_CALL_TARGET:
		return "find_call_target";
	case ExtractionStrategy::CUSTOM:
		return "custom";
	default:
		return "unknown";
	}
}

static string GetNativeExtractionStrategyName(NativeExtractionStrategy strategy) {
	switch (strategy) {
	case NativeExtractionStrategy::NONE:
		return "none";
	case NativeExtractionStrategy::FUNCTION_WITH_PARAMS:
		return "function_with_params";
	case NativeExtractionStrategy::FUNCTION_WITH_DECORATORS:
		return "function_with_decorators";
	case NativeExtractionStrategy::ARROW_FUNCTION:
		return "arrow_function";
	case NativeExtractionStrategy::ASYNC_FUNCTION:
		return "async_function";
	case NativeExtractionStrategy::CLASS_WITH_INHERITANCE:
		return "class_with_inheritance";
	case NativeExtractionStrategy::CLASS_WITH_METHODS:
		return "class_with_methods";
	case NativeExtractionStrategy::VARIABLE_WITH_TYPE:
		return "variable_with_type";
	case NativeExtractionStrategy::GENERIC_FUNCTION:
		return "generic_function";
	case NativeExtractionStrategy::METHOD_DEFINITION:
		return "method_definition";
	case NativeExtractionStrategy::CONSTRUCTOR_DEFINITION:
		return "constructor_definition";
	case NativeExtractionStrategy::INTERFACE_DEFINITION:
		return "interface_definition";
	case NativeExtractionStrategy::ENUM_DEFINITION:
		return "enum_definition";
	case NativeExtractionStrategy::IMPORT_STATEMENT:
		return "import_statement";
	case NativeExtractionStrategy::EXPORT_STATEMENT:
		return "export_statement";
	case NativeExtractionStrategy::FUNCTION_CALL:
		return "function_call";
	case NativeExtractionStrategy::CUSTOM:
		return "custom";
	default:
		return "unknown";
	}
}
// <<< END GENERATED TAXONOMY: type_map_name_tables >>>

static unique_ptr<FunctionData> TypeMapBind(ClientContext &context, TableFunctionBindInput &input,
                                            vector<LogicalType> &return_types, vector<CompatName> &names) {
	auto bind_data = make_uniq<TypeMapBindData>();

	// Optional language parameter
	if (!input.inputs.empty() && !input.inputs[0].IsNull()) {
		bind_data->language_filter = input.inputs[0].GetValue<string>();
	}

	// Output schema
	// native_strategy sits beside name_strategy, and before flags, because the
	// two strategies are the third and fourth DEF_TYPE arguments and belong
	// together. Appending it after flags would have kept positional callers
	// working, but it would also have put the fourth argument of DEF_TYPE after
	// the fifth, which is the kind of ordering nobody can explain later.
	names = {"language",  "node_type",     "semantic_type",   "kind", "name_role", "is_scope",
	         "is_syntax", "name_strategy", "native_strategy", "flags"};
	return_types = {LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR, LogicalType::VARCHAR,
	                LogicalType::VARCHAR, LogicalType::BOOLEAN, LogicalType::BOOLEAN, LogicalType::VARCHAR,
	                LogicalType::VARCHAR, LogicalType::UTINYINT};

	return std::move(bind_data);
}

static unique_ptr<GlobalTableFunctionState> TypeMapInit(ClientContext &context, TableFunctionInitInput &input) {
	auto &bind_data = input.bind_data->Cast<TypeMapBindData>();
	auto state = make_uniq<TypeMapGlobalState>();

	auto &registry = LanguageAdapterRegistry::GetInstance();
	auto languages = registry.GetSupportedLanguages();

	for (const auto &lang : languages) {
		// Filter by language if specified
		if (!bind_data.language_filter.empty() && lang != bind_data.language_filter) {
			continue;
		}

		auto adapter = registry.CreateAdapter(lang);
		if (!adapter)
			continue;

		const auto &configs = adapter->GetNodeConfigs();
		for (const auto &entry : configs) {
			TypeMapGlobalState::Row row;
			row.language = lang;
			row.node_type = entry.first;
			row.semantic_type_byte = entry.second.semantic_type;
			row.semantic_type_name = SemanticTypes::GetSemanticTypeName(entry.second.semantic_type & 0xFC);
			row.kind = GetKindName(entry.second.semantic_type);
			row.flags = entry.second.flags;
			row.name_role = GetNameRoleString(entry.second.flags);
			row.is_scope = (entry.second.flags & ASTNodeFlags::IS_SCOPE) != 0;
			row.is_syntax = (entry.second.flags & ASTNodeFlags::IS_SYNTAX_ONLY) != 0;
			row.name_strategy = GetExtractionStrategyName(entry.second.name_strategy);
			// The capability scripts/conformance_kit.sh had to derive by reading
			// languages/*/*_types.def, because nothing exposed it at run time.
			// That scan attributes a row to a language by the file's own
			// basename, so it cannot see a table the preprocessor assembles:
			// typescript_types.def #includes javascript's, and the scan found 5
			// of the 13 function-shaped declarations this map holds.
			row.native_strategy = GetNativeExtractionStrategyName(entry.second.native_strategy);
			state->rows.push_back(std::move(row));
		}
	}

	// Sort by language, then kind, then node_type for consistent output
	std::sort(state->rows.begin(), state->rows.end(),
	          [](const TypeMapGlobalState::Row &a, const TypeMapGlobalState::Row &b) {
		          if (a.language != b.language)
			          return a.language < b.language;
		          if (a.kind != b.kind)
			          return a.kind < b.kind;
		          return a.node_type < b.node_type;
	          });

	return std::move(state);
}

static void TypeMapFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &state = data_p.global_state->Cast<TypeMapGlobalState>();

	idx_t count = 0;
	while (count < STANDARD_VECTOR_SIZE && state.current_row < state.rows.size()) {
		const auto &row = state.rows[state.current_row];

		output.SetValue(0, count, Value(row.language));
		output.SetValue(1, count, Value(row.node_type));
		output.SetValue(2, count, Value(row.semantic_type_name));
		output.SetValue(3, count, Value(row.kind));
		output.SetValue(4, count, row.name_role.empty() ? Value(LogicalType::VARCHAR) : Value(row.name_role));
		output.SetValue(5, count, Value::BOOLEAN(row.is_scope));
		output.SetValue(6, count, Value::BOOLEAN(row.is_syntax));
		output.SetValue(7, count, Value(row.name_strategy));
		output.SetValue(8, count, Value(row.native_strategy));
		output.SetValue(9, count, Value::UTINYINT(row.flags));

		count++;
		state.current_row++;
	}

	CompatSetOutputCardinality(output, count);
}

void RegisterASTTypeMapFunction(ExtensionLoader &loader) {
	TableFunctionSet type_map_set("ast_type_map");
	TableFunction type_map("ast_type_map", {}, TypeMapFunction, TypeMapBind, TypeMapInit);
	type_map_set.AddFunction(type_map);
	TableFunction type_map_lang("ast_type_map", {LogicalType::VARCHAR}, TypeMapFunction, TypeMapBind, TypeMapInit);
	type_map_set.AddFunction(type_map_lang);

	RegisterDocumentedTableFunctionSet(
	    loader, type_map_set, "Return mapping of AST node types to semantic types and extraction strategies.",
	    {{}, {"language"}}, {"SELECT * FROM ast_type_map()", "SELECT * FROM ast_type_map('python')"},
	    {"sitting_duck", "metadata"});
}

} // namespace duckdb
