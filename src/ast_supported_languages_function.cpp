#include "duckdb.hpp"
#include "language_adapter.hpp"
#include "include/ast_file_utils.hpp"

namespace duckdb {

struct SupportedLanguagesData : public GlobalTableFunctionState {
	SupportedLanguagesData() : offset(0) {
	}
	idx_t offset;
};

static unique_ptr<FunctionData> SupportedLanguagesBind(ClientContext &context, TableFunctionBindInput &input,
                                                       vector<LogicalType> &return_types, vector<string> &names) {
	names.emplace_back("language");
	return_types.emplace_back(LogicalType::VARCHAR);

	names.emplace_back("extensions");
	return_types.emplace_back(LogicalType::LIST(LogicalType::VARCHAR));

	names.emplace_back("parser_type");
	return_types.emplace_back(LogicalType::VARCHAR);

	names.emplace_back("node_type_count");
	return_types.emplace_back(LogicalType::INTEGER);

	return nullptr;
}

static unique_ptr<GlobalTableFunctionState> SupportedLanguagesInit(ClientContext &context,
                                                                   TableFunctionInitInput &input) {
	return make_uniq<SupportedLanguagesData>();
}

static void SupportedLanguagesFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.global_state->Cast<SupportedLanguagesData>();
	auto &registry = LanguageAdapterRegistry::GetInstance();
	auto languages = registry.GetSupportedLanguages();

	idx_t count = 0;
	for (idx_t i = data.offset; i < languages.size() && count < STANDARD_VECTOR_SIZE; i++) {
		const auto &lang = languages[i];

		// Column 0: language name
		output.SetValue(0, count, Value(lang));

		// Column 1: file extensions as LIST<VARCHAR>
		auto exts = ASTFileUtils::GetSupportedExtensions(lang);
		vector<Value> ext_values;
		for (const auto &ext : exts) {
			ext_values.push_back(Value(ext));
		}
		output.SetValue(1, count, Value::LIST(LogicalType::VARCHAR, ext_values));

		// Column 2: parser type
		string parser_type = (lang == "duckdb") ? "native" : "tree-sitter";
		output.SetValue(2, count, Value(parser_type));

		// Column 3: node type count
		auto adapter = registry.CreateAdapter(lang);
		int32_t node_type_count = 0;
		if (adapter) {
			node_type_count = static_cast<int32_t>(adapter->GetNodeConfigs().size());
		}
		output.SetValue(3, count, Value::INTEGER(node_type_count));

		count++;
		data.offset++;
	}
	output.SetCardinality(count);
}

void RegisterASTSupportedLanguagesFunction(ExtensionLoader &loader) {
	TableFunction function("ast_supported_languages", {}, SupportedLanguagesFunction, SupportedLanguagesBind,
	                       SupportedLanguagesInit);
	loader.RegisterFunction(function);
}

} // namespace duckdb
