#pragma once

#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"
#include <type_traits>

namespace duckdb {

template <typename T>
auto GetArguments(const T &fn, int) -> decltype(fn->arguments) {
	return fn->arguments;
}

template <typename T>
auto GetArguments(const T &fn, long) -> decltype(fn.arguments) {
	return fn.arguments;
}

template <typename T>
vector<LogicalType> GetArguments(const T &fn, ...) {
	return {};
}

inline void RegisterDocumentedScalarFunction(ExtensionLoader &loader, ScalarFunction func, const string &description,
                                             const vector<string> &parameter_names = {},
                                             const vector<string> &examples = {},
                                             const vector<string> &categories = {"sitting_duck"}) {
	CreateScalarFunctionInfo info(std::move(func));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	FunctionDescription desc;
	desc.description = description;
	desc.parameter_types = GetArguments(info.functions.functions[0], 0);
	desc.parameter_names = parameter_names;
	desc.examples = examples;
	desc.categories = categories;
	info.descriptions.push_back(std::move(desc));
	loader.RegisterFunction(std::move(info));
}

inline void RegisterDocumentedScalarFunctionSet(ExtensionLoader &loader, ScalarFunctionSet set,
                                                const string &description,
                                                const vector<vector<string>> &parameter_names_list = {{}},
                                                const vector<string> &examples = {},
                                                const vector<string> &categories = {"sitting_duck"}) {
	CreateScalarFunctionInfo info(std::move(set));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	for (idx_t i = 0; i < info.functions.functions.size(); i++) {
		const auto &func = info.functions.functions[i];
		FunctionDescription desc;
		desc.description = description;
		desc.parameter_types = GetArguments(func, 0);
		if (i < parameter_names_list.size()) {
			desc.parameter_names = parameter_names_list[i];
		} else if (!parameter_names_list.empty()) {
			desc.parameter_names = parameter_names_list[0];
		}
		desc.examples = examples;
		desc.categories = categories;
		info.descriptions.push_back(std::move(desc));
	}
	loader.RegisterFunction(std::move(info));
}

inline void RegisterDocumentedTableFunction(ExtensionLoader &loader, TableFunction func, const string &description,
                                            const vector<string> &parameter_names = {},
                                            const vector<string> &examples = {},
                                            const vector<string> &categories = {"sitting_duck"}) {
	CreateTableFunctionInfo info(std::move(func));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	FunctionDescription desc;
	desc.description = description;
	desc.parameter_types = GetArguments(info.functions.functions[0], 0);
	desc.parameter_names = parameter_names;
	desc.examples = examples;
	desc.categories = categories;
	info.descriptions.push_back(std::move(desc));
	loader.RegisterFunction(std::move(info));
}

inline void RegisterDocumentedTableFunctionSet(ExtensionLoader &loader, TableFunctionSet set, const string &description,
                                               const vector<vector<string>> &parameter_names_list = {{}},
                                               const vector<string> &examples = {},
                                               const vector<string> &categories = {"sitting_duck"}) {
	CreateTableFunctionInfo info(std::move(set));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	for (idx_t i = 0; i < info.functions.functions.size(); i++) {
		const auto &func = info.functions.functions[i];
		FunctionDescription desc;
		desc.description = description;
		desc.parameter_types = GetArguments(func, 0);
		if (i < parameter_names_list.size()) {
			desc.parameter_names = parameter_names_list[i];
		} else if (!parameter_names_list.empty()) {
			desc.parameter_names = parameter_names_list[0];
		}
		desc.examples = examples;
		desc.categories = categories;
		info.descriptions.push_back(std::move(desc));
	}
	loader.RegisterFunction(std::move(info));
}

} // namespace duckdb
