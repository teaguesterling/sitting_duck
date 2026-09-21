#pragma once

#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

namespace duckdb {

//! The argument types of a function, across the DuckDB versions this extension builds
//! against. Two shapes exist, both measured in duckdb/src/include/duckdb/function/function.hpp
//! rather than assumed:
//!
//!   v1.5.4 and earlier   SimpleFunction carries a PUBLIC member:
//!                        `vector<LogicalType> arguments;`
//!   DuckDB main (v2.0)   that member is GONE. Arguments live in a PROTECTED
//!                        FunctionSignature reached through GetSignature(), whose
//!                        parameters each carry their own type:
//!                        SimpleFunction::GetSignature() ->
//!                        FunctionSignature::GetParameters() -> FunctionParameter::GetType()
//!
//! FunctionDescription::parameter_types is still vector<LogicalType> on both lines, so
//! only the read side differs. This is what broke the DuckDB-main canary: the header
//! reached for `.arguments` directly, which does not compile on the v2.0 line.
//!
//! THERE IS DELIBERATELY NO CATCH-ALL OVERLOAD. An earlier version of this header had
//! one returning `{}`. That compiles against ANY future API and silently registers
//! functions with EMPTY parameter_types -- documentation disappearing with no build
//! anywhere reporting it. Without it, a DuckDB carrying neither shape fails to compile
//! with an error naming this function, which is the loud failure we want. Do not add a
//! fallback to make a build go green; add the shape that build actually has.
template <typename T>
auto GetArgumentTypes(const T &fn, int) -> decltype(fn.arguments) {
	return fn.arguments;
}

template <typename T>
auto GetArgumentTypes(const T &fn, long) -> decltype(fn.GetSignature(), vector<LogicalType>()) {
	vector<LogicalType> types;
	for (auto &parameter : fn.GetSignature().GetParameters()) {
		types.push_back(parameter.GetType());
	}
	return types;
}

inline void RegisterDocumentedScalarFunction(ExtensionLoader &loader, ScalarFunction func, const string &description,
                                             const vector<string> &parameter_names = {},
                                             const vector<string> &examples = {},
                                             const vector<string> &categories = {"sitting_duck"}) {
	FunctionDescription desc;
	desc.description = description;
	desc.parameter_types = GetArgumentTypes(func, 0);
	desc.parameter_names = parameter_names;
	desc.examples = examples;
	desc.categories = categories;

	CreateScalarFunctionInfo info(std::move(func));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	info.descriptions.push_back(std::move(desc));
	loader.RegisterFunction(std::move(info));
}

inline void RegisterDocumentedScalarFunctionSet(ExtensionLoader &loader, ScalarFunctionSet set,
                                                const string &description,
                                                const vector<vector<string>> &parameter_names_list = {{}},
                                                const vector<string> &examples = {},
                                                const vector<string> &categories = {"sitting_duck"}) {
	vector<FunctionDescription> descriptions;
	for (idx_t i = 0; i < set.functions.size(); i++) {
		FunctionDescription desc;
		desc.description = description;
		desc.parameter_types = GetArgumentTypes(set.functions[i], 0);
		if (i < parameter_names_list.size()) {
			desc.parameter_names = parameter_names_list[i];
		} else if (!parameter_names_list.empty()) {
			desc.parameter_names = parameter_names_list[0];
		}
		desc.examples = examples;
		desc.categories = categories;
		descriptions.push_back(std::move(desc));
	}
	CreateScalarFunctionInfo info(std::move(set));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	info.descriptions = std::move(descriptions);
	loader.RegisterFunction(std::move(info));
}

inline void RegisterDocumentedTableFunction(ExtensionLoader &loader, TableFunction func, const string &description,
                                            const vector<string> &parameter_names = {},
                                            const vector<string> &examples = {},
                                            const vector<string> &categories = {"sitting_duck"}) {
	FunctionDescription desc;
	desc.description = description;
	desc.parameter_types = GetArgumentTypes(func, 0);
	desc.parameter_names = parameter_names;
	desc.examples = examples;
	desc.categories = categories;

	CreateTableFunctionInfo info(std::move(func));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	info.descriptions.push_back(std::move(desc));
	loader.RegisterFunction(std::move(info));
}

inline void RegisterDocumentedTableFunctionSet(ExtensionLoader &loader, TableFunctionSet set, const string &description,
                                               const vector<vector<string>> &parameter_names_list = {{}},
                                               const vector<string> &examples = {},
                                               const vector<string> &categories = {"sitting_duck"}) {
	vector<FunctionDescription> descriptions;
	for (idx_t i = 0; i < set.functions.size(); i++) {
		FunctionDescription desc;
		desc.description = description;
		desc.parameter_types = GetArgumentTypes(set.functions[i], 0);
		if (i < parameter_names_list.size()) {
			desc.parameter_names = parameter_names_list[i];
		} else if (!parameter_names_list.empty()) {
			desc.parameter_names = parameter_names_list[0];
		}
		desc.examples = examples;
		desc.categories = categories;
		descriptions.push_back(std::move(desc));
	}
	CreateTableFunctionInfo info(std::move(set));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;
	info.descriptions = std::move(descriptions);
	loader.RegisterFunction(std::move(info));
}

} // namespace duckdb
