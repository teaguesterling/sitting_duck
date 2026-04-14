#include "parse_ast_list_function.hpp"
#include "unified_ast_backend.hpp"
#include "semantic_type_logical_type.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/planner/expression/bound_function_expression.hpp"

namespace duckdb {

//==============================================================================
// Bind Data
//==============================================================================

struct ParseASTListBindData : public FunctionData {
	ExtractionConfig config;

	ParseASTListBindData() : config() {
	}

	explicit ParseASTListBindData(const ExtractionConfig &config) : config(config) {
	}

	unique_ptr<FunctionData> Copy() const override {
		return make_uniq<ParseASTListBindData>(config);
	}

	bool Equals(const FunctionData &other_p) const override {
		auto &other = other_p.Cast<ParseASTListBindData>();
		return config.context == other.config.context && config.source == other.config.source &&
		       config.structure == other.config.structure && config.peek == other.config.peek &&
		       config.peek_size == other.config.peek_size;
	}
};

//==============================================================================
// Build the LIST(STRUCT(...)) return type
//==============================================================================

static LogicalType BuildASTStructType(const ExtractionConfig &config) {
	auto types = UnifiedASTBackend::GetFlatDynamicTableSchema(config);
	auto names = UnifiedASTBackend::GetFlatDynamicTableColumnNames(config);

	child_list_t<LogicalType> struct_children;
	for (idx_t i = 0; i < types.size(); i++) {
		// Replace SEMANTIC_TYPE custom logical type with plain UTINYINT
		if (IsSemanticType(types[i])) {
			struct_children.push_back(make_pair(names[i], LogicalType::UTINYINT));
		} else {
			struct_children.push_back(make_pair(names[i], types[i]));
		}
	}

	return LogicalType::LIST(LogicalType::STRUCT(std::move(struct_children)));
}

//==============================================================================
// Convert ASTResult to LIST(STRUCT(...)) Value
//==============================================================================

static Value ConvertASTResultToList(const ASTResult &result, const ExtractionConfig &config) {
	auto types = UnifiedASTBackend::GetFlatDynamicTableSchema(config);
	auto names = UnifiedASTBackend::GetFlatDynamicTableColumnNames(config);

	// Build the struct element type (with SEMANTIC_TYPE replaced)
	child_list_t<LogicalType> struct_children;
	for (idx_t i = 0; i < types.size(); i++) {
		if (IsSemanticType(types[i])) {
			struct_children.push_back(make_pair(names[i], LogicalType::UTINYINT));
		} else {
			struct_children.push_back(make_pair(names[i], types[i]));
		}
	}
	auto element_type = LogicalType::STRUCT(struct_children);

	vector<Value> node_values;
	node_values.reserve(result.nodes.size());

	for (const auto &node : result.nodes) {
		child_list_t<Value> fields;

		// Core: always present
		fields.push_back(make_pair("node_id", Value::BIGINT(node.node_id)));
		fields.push_back(make_pair("type", Value(node.type_raw)));

		// Context fields
		if (config.context != ContextLevel::NONE) {
			if (config.context >= ContextLevel::NODE_TYPES_ONLY) {
				fields.push_back(make_pair("semantic_type", Value::UTINYINT(node.semantic_type)));
				fields.push_back(make_pair("flags", Value::UTINYINT(node.universal_flags)));
			}
			if (config.context >= ContextLevel::NORMALIZED) {
				fields.push_back(make_pair("name", Value(node.name_raw)));
				fields.push_back(
				    make_pair("qualified_name", UnifiedASTBackend::QualifiedNameValue(node.name_qualified_segments)));
			}
			if (config.context >= ContextLevel::NATIVE) {
				// signature_type
				fields.push_back(make_pair("signature_type", node.native.signature_type.empty()
				                                                 ? Value()
				                                                 : Value(node.native.signature_type)));

				// parameters: LIST of STRUCT{name, type}
				vector<Value> param_structs;
				if (node.native_extraction_attempted) {
					for (const auto &param : node.native.parameters) {
						child_list_t<Value> param_fields;
						param_fields.emplace_back("name", Value(param.name));
						param_fields.emplace_back("type", Value(param.type));
						param_structs.push_back(Value::STRUCT(std::move(param_fields)));
					}
				}
				fields.push_back(make_pair(
				    "parameters",
				    Value::LIST(LogicalType::STRUCT({{"name", LogicalType::VARCHAR}, {"type", LogicalType::VARCHAR}}),
				                std::move(param_structs))));

				// modifiers: LIST(VARCHAR)
				vector<Value> modifier_values;
				if (node.native_extraction_attempted) {
					for (const auto &mod : node.native.modifiers) {
						modifier_values.push_back(Value(mod));
					}
				}
				fields.push_back(make_pair("modifiers", Value::LIST(LogicalType::VARCHAR, std::move(modifier_values))));

				// annotations
				fields.push_back(make_pair("annotations",
				                           node.native.annotations.empty() ? Value() : Value(node.native.annotations)));
			}
		}

		// Source fields
		if (config.source != SourceLevel::NONE) {
			fields.push_back(make_pair("file_path", Value(result.source.file_path)));
			fields.push_back(make_pair("language", Value(result.source.language)));

			if (config.source >= SourceLevel::LINES_ONLY) {
				fields.push_back(make_pair("start_line", Value::UINTEGER(static_cast<uint32_t>(node.start_line))));
				fields.push_back(make_pair("end_line", Value::UINTEGER(static_cast<uint32_t>(node.end_line))));
			}

			if (config.source >= SourceLevel::FULL) {
				fields.push_back(make_pair("start_column", Value::UINTEGER(static_cast<uint32_t>(node.start_column))));
				fields.push_back(make_pair("end_column", Value::UINTEGER(static_cast<uint32_t>(node.end_column))));
			}
		}

		// Structure fields
		if (config.structure != StructureLevel::NONE) {
			if (config.structure >= StructureLevel::MINIMAL) {
				// parent_id: NULL for root nodes (parent_index < 0)
				if (node.parent_index < 0) {
					fields.push_back(make_pair("parent_id", Value()));
				} else {
					fields.push_back(make_pair("parent_id", Value::BIGINT(node.parent_index)));
				}
				fields.push_back(make_pair("depth", Value::UINTEGER(static_cast<uint32_t>(node.node_depth))));
			}

			if (config.structure >= StructureLevel::FULL) {
				fields.push_back(make_pair("sibling_index", Value::INTEGER(node.legacy_sibling_index)));
				fields.push_back(
				    make_pair("children_count", Value::UINTEGER(static_cast<uint32_t>(node.legacy_children_count))));
				fields.push_back(make_pair("descendant_count",
				                           Value::UINTEGER(static_cast<uint32_t>(node.legacy_descendant_count))));

				// scope STRUCT<current, function, class, module, stack>
				fields.push_back(make_pair("scope", UnifiedASTBackend::ScopeValue(node.scope)));
			}
		}

		// Peek
		if (config.peek != PeekLevel::NONE) {
			fields.push_back(make_pair("peek", node.peek.empty() ? Value() : Value(node.peek)));
		}

		node_values.push_back(Value::STRUCT(std::move(fields)));
	}

	return Value::LIST(element_type, std::move(node_values));
}

//==============================================================================
// Bind Function
//==============================================================================

static unique_ptr<FunctionData> ParseASTListBind(ClientContext &context, ScalarFunction &bound_function,
                                                 vector<unique_ptr<Expression>> &arguments) {
	return make_uniq<ParseASTListBindData>();
}

//==============================================================================
// Execute Function
//==============================================================================

static void ParseASTListExecute(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &bind_data = state.expr.Cast<BoundFunctionExpression>().bind_info->Cast<ParseASTListBindData>();
	auto &code_vector = args.data[0];
	auto &language_vector = args.data[1];

	idx_t count = args.size();

	// Flatten input vectors for uniform access
	code_vector.Flatten(count);
	language_vector.Flatten(count);

	auto code_data = FlatVector::GetData<string_t>(code_vector);
	auto language_data = FlatVector::GetData<string_t>(language_vector);
	auto &code_validity = FlatVector::Validity(code_vector);
	auto &language_validity = FlatVector::Validity(language_vector);

	for (idx_t i = 0; i < count; i++) {
		if (!code_validity.RowIsValid(i) || !language_validity.RowIsValid(i)) {
			FlatVector::SetNull(result, i, true);
			continue;
		}

		auto code = code_data[i].GetString();
		auto language = language_data[i].GetString();

		try {
			auto ast_result = UnifiedASTBackend::ParseToASTResult(code, language, "<inline>", bind_data.config);
			auto list_value = ConvertASTResultToList(ast_result, bind_data.config);
			result.SetValue(i, list_value);
		} catch (const InvalidInputException &) {
			// User-input problems (unsupported language, etc.) — NULL this row rather than
			// failing the entire query. Let InternalException and other bugs propagate.
			FlatVector::SetNull(result, i, true);
		}
	}
}

//==============================================================================
// Registration
//==============================================================================

void RegisterParseASTListFunction(ExtensionLoader &loader) {
	ExtractionConfig default_config;
	auto return_type = BuildASTStructType(default_config);

	ScalarFunction func("parse_ast_list", {LogicalType::VARCHAR, LogicalType::VARCHAR}, return_type,
	                    ParseASTListExecute, ParseASTListBind);

	loader.RegisterFunction(func);
}

} // namespace duckdb
