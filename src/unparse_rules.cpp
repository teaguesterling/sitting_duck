#include "unparse_rules.hpp"
#include "duckdb_compat.hpp"
#include "function_doc_helper.hpp"
#include <unordered_map>

namespace duckdb {

static std::vector<UnparseRule> InitUniversalRules() {
	std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
	rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/universal_unparse.def"
#undef DEF_UNPARSE_RULE
	return rules;
}

const std::vector<UnparseRule> &GetUniversalUnparseRules() {
	static const std::vector<UnparseRule> universal = InitUniversalRules();
	return universal;
}

static std::unordered_map<std::string, std::vector<UnparseRule>> InitLanguageRules() {
	std::unordered_map<std::string, std::vector<UnparseRule>> map;

	// Python
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/python_unparse.def"
#undef DEF_UNPARSE_RULE
		map["python"] = std::move(rules);
	}

	// JavaScript
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/javascript_unparse.def"
#undef DEF_UNPARSE_RULE
		map["javascript"] = std::move(rules);
	}

	// TypeScript
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/typescript_unparse.def"
#undef DEF_UNPARSE_RULE
		map["typescript"] = std::move(rules);
	}

	// C
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/c_unparse.def"
#undef DEF_UNPARSE_RULE
		map["c"] = std::move(rules);
	}

	// C++
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/cpp_unparse.def"
#undef DEF_UNPARSE_RULE
		map["cpp"] = rules;
		map["c++"] = std::move(rules);
	}

	// Go
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/go_unparse.def"
#undef DEF_UNPARSE_RULE
		map["go"] = std::move(rules);
	}

	// Rust
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/rust_unparse.def"
#undef DEF_UNPARSE_RULE
		map["rust"] = std::move(rules);
	}

	// Java
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/java_unparse.def"
#undef DEF_UNPARSE_RULE
		map["java"] = std::move(rules);
	}

	// Ruby
	{
		std::vector<UnparseRule> rules;
#define DEF_UNPARSE_RULE(kind, target, int_arg, str_arg) \
		rules.emplace_back(kind, target, int_arg, str_arg);
#include "language_configs/unparse/ruby_unparse.def"
#undef DEF_UNPARSE_RULE
		map["ruby"] = std::move(rules);
	}

	return map;
}

std::vector<UnparseRule> GetLanguageUnparseRules(const std::string &language) {
	static const auto lang_map = InitLanguageRules();
	auto it = lang_map.find(language);
	if (it != lang_map.end()) {
		return it->second;
	}
	return {};
}

std::vector<std::pair<std::string, UnparseRule>> GetAllUnparseRules() {
	std::vector<std::pair<std::string, UnparseRule>> all;
	for (const auto &rule : GetUniversalUnparseRules()) {
		all.emplace_back("*", rule);
	}
	static const auto lang_map = InitLanguageRules();
	for (const auto &pair : lang_map) {
		for (const auto &rule : pair.second) {
			all.emplace_back(pair.first, rule);
		}
	}
	return all;
}

// Table function Bind Data
struct UnparseRulesBindData : public TableFunctionData {
	string language_filter; // empty = all languages
};

// Table function Global State
struct UnparseRulesGlobalState : public GlobalTableFunctionState {
	std::vector<std::pair<std::string, UnparseRule>> rules;
	idx_t offset = 0;
};

static unique_ptr<FunctionData> UnparseRulesBind(ClientContext &context, TableFunctionBindInput &input,
                                                vector<LogicalType> &return_types, vector<CompatName> &names) {
	names.emplace_back("language");
	return_types.emplace_back(LogicalType::VARCHAR);

	names.emplace_back("rule");
	return_types.emplace_back(LogicalType::VARCHAR);

	names.emplace_back("target");
	return_types.emplace_back(LogicalType::VARCHAR);

	names.emplace_back("int_arg");
	return_types.emplace_back(LogicalType::BIGINT);

	names.emplace_back("str_arg");
	return_types.emplace_back(LogicalType::VARCHAR);

	auto bind_data = make_uniq<UnparseRulesBindData>();
	if (!input.inputs.empty() && !input.inputs[0].IsNull()) {
		bind_data->language_filter = StringValue::Get(input.inputs[0]);
	}
	return std::move(bind_data);
}

static unique_ptr<GlobalTableFunctionState> UnparseRulesInit(ClientContext &context, TableFunctionInitInput &input) {
	auto result = make_uniq<UnparseRulesGlobalState>();
	const auto &bind_data = input.bind_data->Cast<UnparseRulesBindData>();

	if (!bind_data.language_filter.empty()) {
		for (const auto &rule : GetUniversalUnparseRules()) {
			result->rules.emplace_back("*", rule);
		}
		for (const auto &rule : GetLanguageUnparseRules(bind_data.language_filter)) {
			result->rules.emplace_back(bind_data.language_filter, rule);
		}
	} else {
		result->rules = GetAllUnparseRules();
	}
	return std::move(result);
}

static void UnparseRulesFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.global_state->Cast<UnparseRulesGlobalState>();
	idx_t count = 0;

	while (data.offset < data.rules.size() && count < STANDARD_VECTOR_SIZE) {
		const auto &entry = data.rules[data.offset++];
		const auto &lang = entry.first;
		const auto &rule = entry.second;

		output.SetValue(0, count, Value(lang));
		output.SetValue(1, count, Value(UnparseRuleKindToString(rule.kind)));
		output.SetValue(2, count, Value(rule.target));
		output.SetValue(3, count, Value::BIGINT(rule.int_arg));
		output.SetValue(4, count, Value(rule.str_arg));
		count++;
	}

	CompatSetOutputCardinality(output, count);
}

void RegisterUnparseRulesFunction(ExtensionLoader &loader) {
	TableFunctionSet unparse_rules_set("ast_unparse_rules");
	TableFunction unparse_rules_all("ast_unparse_rules", {}, UnparseRulesFunction, UnparseRulesBind, UnparseRulesInit);
	unparse_rules_set.AddFunction(unparse_rules_all);
	TableFunction unparse_rules_lang("ast_unparse_rules", {LogicalType::VARCHAR}, UnparseRulesFunction, UnparseRulesBind, UnparseRulesInit);
	unparse_rules_set.AddFunction(unparse_rules_lang);

	RegisterDocumentedTableFunctionSet(
	    loader, unparse_rules_set,
	    "Return active unparse layout and spacing rules.",
	    {{}, {"language"}},
	    {"SELECT * FROM ast_unparse_rules()", "SELECT * FROM ast_unparse_rules('python')"},
	    {"sitting_duck", "unparse"});
}

} // namespace duckdb
