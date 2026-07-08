#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/settings.hpp"
#include "dynamic_library_loader.hpp"
#include "language_adapter.hpp"
#include "language_config_json.hpp"
#include "tree_sitter_wrappers.hpp"

namespace duckdb {

struct RegisterLanguageBindData : public TableFunctionData {
	string name;
	string library_path;
	string config_path;
	vector<string> extensions;
	vector<string> aliases;
	string symbol;
	bool overwrite = false;
};

struct RegisterLanguageGlobalState : public GlobalTableFunctionState {
	bool done = false;
};

static bool IsValidLanguageName(const string &name) {
	if (name.empty() || name[0] < 'a' || name[0] > 'z') {
		return false;
	}
	for (char c : name) {
		if (!((c >= 'a' && c <= 'z') || StringUtil::CharacterIsDigit(c) || c == '_')) {
			return false;
		}
	}
	return true;
}

static string ReadFileToString(ClientContext &context, const string &path) {
	auto &fs = FileSystem::GetFileSystem(context);
	auto handle = fs.OpenFile(path, FileFlags::FILE_FLAGS_READ);
	auto size = fs.GetFileSize(*handle);
	string content;
	content.resize(size);
	fs.Read(*handle, (void *)content.data(), size);
	return content;
}

static unique_ptr<FunctionData> RegisterLanguageBind(ClientContext &context, TableFunctionBindInput &input,
                                                     vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs[0].IsNull() || input.inputs[1].IsNull()) {
		throw BinderException("register_language: name and library path cannot be NULL");
	}

	auto result = make_uniq<RegisterLanguageBindData>();
	result->name = input.inputs[0].GetValue<string>();
	result->library_path = input.inputs[1].GetValue<string>();
	result->symbol = "tree_sitter_" + result->name;

	for (auto &kv : input.named_parameters) {
		if (kv.first == "config") {
			result->config_path = kv.second.GetValue<string>();
		} else if (kv.first == "extensions") {
			for (auto &ext : ListValue::GetChildren(kv.second)) {
				// Store without a leading dot, lowercased to match detection
				auto extension = StringUtil::Lower(ext.ToString());
				if (!extension.empty() && extension[0] == '.') {
					extension = extension.substr(1);
				}
				if (extension.empty()) {
					throw BinderException("register_language: extensions cannot be empty");
				}
				result->extensions.push_back(extension);
			}
		} else if (kv.first == "aliases") {
			for (auto &alias : ListValue::GetChildren(kv.second)) {
				if (alias.ToString().empty()) {
					throw BinderException("register_language: aliases cannot be empty");
				}
				result->aliases.push_back(alias.ToString());
			}
		} else if (kv.first == "symbol") {
			result->symbol = kv.second.GetValue<string>();
		} else if (kv.first == "overwrite") {
			result->overwrite = kv.second.GetValue<bool>();
		}
	}

	names = {"language", "abi_version", "node_type_count", "status"};
	return_types = {LogicalType::VARCHAR, LogicalType::INTEGER, LogicalType::INTEGER, LogicalType::VARCHAR};
	return std::move(result);
}

static unique_ptr<GlobalTableFunctionState> RegisterLanguageInit(ClientContext &context,
                                                                 TableFunctionInitInput &input) {
	return make_uniq<RegisterLanguageGlobalState>();
}

static void RegisterLanguageFunction(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &state = data_p.global_state->Cast<RegisterLanguageGlobalState>();
	if (state.done) {
		output.SetCardinality(0);
		return;
	}
	state.done = true;
	auto &bind_data = data_p.bind_data->Cast<RegisterLanguageBindData>();

	// Loading a grammar library executes arbitrary native code, so it is gated
	// behind the same option as DuckDB's own external-access surfaces.
	if (!Settings::Get<EnableExternalAccessSetting>(context)) {
		throw PermissionException("register_language requires permission to load native code, "
		                          "but enable_external_access is false");
	}

	if (!IsValidLanguageName(bind_data.name)) {
		throw InvalidInputException("Invalid language name '%s': must match [a-z][a-z0-9_]*", bind_data.name);
	}

	// All validation happens before any registry mutation: a failure below
	// leaves previously registered languages untouched.
	void *handle = DynamicLibraryLoader::LoadGrammarLibrary(bind_data.library_path);
	void *symbol_address = DynamicLibraryLoader::ResolveSymbol(handle, bind_data.symbol, bind_data.library_path);

	typedef const TSLanguage *(*GrammarLanguageFn)();
	auto language_fn = reinterpret_cast<GrammarLanguageFn>(symbol_address);
	const TSLanguage *language = language_fn();
	if (!language) {
		throw InvalidInputException("Symbol '%s' in grammar library '%s' returned a null language", bind_data.symbol,
		                            bind_data.library_path);
	}

	uint32_t abi_version = ts_language_version(language);
	if (!IsCompatibleLanguageAbi(abi_version)) {
		throw InvalidInputException("Incompatible language version for " + bind_data.name +
		                            ". Expected: " + std::to_string(TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION) +
		                            " to " + std::to_string(TREE_SITTER_LANGUAGE_VERSION) +
		                            ", Got: " + std::to_string(abi_version));
	}

	// Smoke-parse so a garbage library fails at registration, not mid-query
	{
		TSParserWrapper parser;
		parser.SetLanguage(language, bind_data.name);
		parser.ParseString("");
	}

	auto info = make_shared_ptr<DynamicLanguageInfo>();
	info->name = bind_data.name;
	info->aliases = bind_data.aliases;
	info->extensions = bind_data.extensions;
	info->language = language;
	if (!bind_data.config_path.empty()) {
		auto config_text = ReadFileToString(context, bind_data.config_path);
		info->node_configs = ParseLanguageConfigJSON(config_text, bind_data.name);
	}
	auto node_type_count = static_cast<int32_t>(info->node_configs.size());

	auto &registry = LanguageAdapterRegistry::GetInstance();
	registry.RegisterDynamicLanguage(shared_ptr<const DynamicLanguageInfo>(std::move(info)), bind_data.overwrite);

	output.SetValue(0, 0, Value(bind_data.name));
	output.SetValue(1, 0, Value::INTEGER(static_cast<int32_t>(abi_version)));
	output.SetValue(2, 0, Value::INTEGER(node_type_count));
	output.SetValue(3, 0, Value("registered"));
	output.SetCardinality(1);
}

void RegisterLanguageRegistrationFunction(ExtensionLoader &loader) {
	TableFunction function("register_language", {LogicalType::VARCHAR, LogicalType::VARCHAR}, RegisterLanguageFunction,
	                       RegisterLanguageBind, RegisterLanguageInit);
	function.named_parameters["config"] = LogicalType::VARCHAR;
	function.named_parameters["extensions"] = LogicalType::LIST(LogicalType::VARCHAR);
	function.named_parameters["aliases"] = LogicalType::LIST(LogicalType::VARCHAR);
	function.named_parameters["symbol"] = LogicalType::VARCHAR;
	function.named_parameters["overwrite"] = LogicalType::BOOLEAN;
	loader.RegisterFunction(function);
}

} // namespace duckdb
