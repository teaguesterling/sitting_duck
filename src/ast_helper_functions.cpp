#include "ast_helper_functions.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/common/types/value.hpp"
#include <sstream>

namespace duckdb {

struct ASTHelperData : public TableFunctionData {
	string json_data;
	unique_ptr<ASTType> ast;
	vector<ASTNode> nodes;
	idx_t current_idx = 0;

	ASTHelperData(string json_data) : json_data(std::move(json_data)) {
	}
};

unique_ptr<ASTType> ASTHelperFunction::ParseASTFromJSON(const string &json) {
	// For now, return a simple empty AST since we're transitioning to a better design
	// In production, we would use DuckDB's JSON functions or a proper parser

	// Extract basic info from JSON string (simplified parsing)
	string file_path = "unknown";
	string language = "unknown";

	// Find file_path in JSON
	size_t fp_pos = json.find("\"file_path\":\"");
	if (fp_pos != string::npos) {
		fp_pos += 13;
		size_t end_pos = json.find("\"", fp_pos);
		if (end_pos != string::npos) {
			file_path = json.substr(fp_pos, end_pos - fp_pos);
		}
	}

	// Find language in JSON
	size_t lang_pos = json.find("\"language\":\"");
	if (lang_pos != string::npos) {
		lang_pos += 12;
		size_t end_pos = json.find("\"", lang_pos);
		if (end_pos != string::npos) {
			language = json.substr(lang_pos, end_pos - lang_pos);
		}
	}

	auto ast = make_uniq<ASTType>(file_path, language);

	// For now, return empty AST - in production we'd parse the nodes array
	// This is a temporary implementation while we refactor to the hybrid design

	return ast;
}

// AST Functions implementation
TableFunction ASTFunctionsFunction::GetFunction() {
	TableFunction function("ast_functions", {LogicalType::BLOB}, Execute, Bind);
	function.name = "ast_functions";
	return function;
}

unique_ptr<FunctionData> ASTFunctionsFunction::Bind(ClientContext &context, TableFunctionBindInput &input,
                                                    vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.size() != 1) {
		throw BinderException("ast_functions requires exactly 1 argument: ast");
	}

	// Get the JSON data from input
	auto json_data = input.inputs[0].GetValue<string>();

	// Define output columns for functions
	names = {"name", "start_line", "end_line", "parameter_count", "is_method", "parent_class"};
	return_types = {LogicalType::VARCHAR, LogicalType::INTEGER, LogicalType::INTEGER,
	                LogicalType::INTEGER, LogicalType::BOOLEAN, LogicalType::VARCHAR};

	return make_uniq<ASTHelperData>(json_data);
}

void ASTFunctionsFunction::Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->CastNoConst<ASTHelperData>();

	// Parse AST on first execution
	if (!data.ast && data.current_idx == 0) {
		data.ast = ParseASTFromJSON(data.json_data);

		// Find all function nodes
		for (const auto &node : data.ast->GetNodes()) {
			if (node.type == "function_definition" || node.type == "method_definition") {
				data.nodes.push_back(node);
			}
		}
	}

	idx_t count = 0;
	auto name_data = FlatVector::GetData<string_t>(output.data[0]);
	auto start_line_data = FlatVector::GetData<int32_t>(output.data[1]);
	auto end_line_data = FlatVector::GetData<int32_t>(output.data[2]);
	auto param_count_data = FlatVector::GetData<int32_t>(output.data[3]);
	auto is_method_data = FlatVector::GetData<bool>(output.data[4]);
	auto parent_class_data = FlatVector::GetData<string_t>(output.data[5]);
	auto &parent_class_validity = FlatVector::Validity(output.data[5]);

	while (data.current_idx < data.nodes.size() && count < STANDARD_VECTOR_SIZE) {
		const auto &func_node = data.nodes[data.current_idx];

		// Set basic info
		name_data[count] = StringVector::AddString(output.data[0], func_node.name);
		start_line_data[count] = func_node.start_line;
		end_line_data[count] = func_node.end_line;

		// Count parameters (simplified - would need proper AST traversal)
		param_count_data[count] = 0; // TODO: implement parameter counting

		// Check if it's a method
		is_method_data[count] = false;
		parent_class_validity.SetInvalid(count);

		if (func_node.parent_id >= 0) {
			auto parent = data.ast->GetNodeById(func_node.parent_id);
			if (parent && parent->type == "class_definition") {
				is_method_data[count] = true;
				parent_class_data[count] = StringVector::AddString(output.data[5], parent->name);
				parent_class_validity.SetValid(count);
			}
		}

		count++;
		data.current_idx++;
	}

	output.SetCardinality(count);

	// Reset if done
	if (data.current_idx >= data.nodes.size()) {
		data.current_idx = 0;
	}
}

// AST Classes implementation
TableFunction ASTClassesFunction::GetFunction() {
	TableFunction function("ast_classes", {LogicalType::BLOB}, Execute, Bind);
	function.name = "ast_classes";
	return function;
}

unique_ptr<FunctionData> ASTClassesFunction::Bind(ClientContext &context, TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.size() != 1) {
		throw BinderException("ast_classes requires exactly 1 argument: ast");
	}

	auto json_data = input.inputs[0].GetValue<string>();

	names = {"name", "start_line", "end_line", "method_count", "base_classes"};
	return_types = {LogicalType::VARCHAR, LogicalType::INTEGER, LogicalType::INTEGER, LogicalType::INTEGER,
	                LogicalType::LIST(LogicalType::VARCHAR)};

	return make_uniq<ASTHelperData>(json_data);
}

void ASTClassesFunction::Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->CastNoConst<ASTHelperData>();

	// Parse AST on first execution
	if (!data.ast && data.current_idx == 0) {
		data.ast = ParseASTFromJSON(data.json_data);

		for (const auto &node : data.ast->GetNodes()) {
			if (node.type == "class_definition") {
				data.nodes.push_back(node);
			}
		}
	}

	idx_t count = 0;
	auto name_data = FlatVector::GetData<string_t>(output.data[0]);
	auto start_line_data = FlatVector::GetData<int32_t>(output.data[1]);
	auto end_line_data = FlatVector::GetData<int32_t>(output.data[2]);
	auto method_count_data = FlatVector::GetData<int32_t>(output.data[3]);

	while (data.current_idx < data.nodes.size() && count < STANDARD_VECTOR_SIZE) {
		const auto &class_node = data.nodes[data.current_idx];

		name_data[count] = StringVector::AddString(output.data[0], class_node.name);
		start_line_data[count] = class_node.start_line;
		end_line_data[count] = class_node.end_line;

		// Count methods
		int32_t method_count = 0;
		auto children = data.ast->GetChildren(class_node.node_id);
		for (const auto &child : children) {
			if (child.type == "function_definition") {
				method_count++;
			}
		}
		method_count_data[count] = method_count;

		// Base classes - simplified for now
		ListVector::SetListSize(output.data[4], 0);
		auto list_entries = FlatVector::GetData<list_entry_t>(output.data[4]);
		list_entries[count].offset = 0;
		list_entries[count].length = 0;

		count++;
		data.current_idx++;
	}

	output.SetCardinality(count);

	if (data.current_idx >= data.nodes.size()) {
		data.current_idx = 0;
	}
}

// AST Imports implementation
TableFunction ASTImportsFunction::GetFunction() {
	TableFunction function("ast_imports", {LogicalType::BLOB}, Execute, Bind);
	function.name = "ast_imports";
	return function;
}

unique_ptr<FunctionData> ASTImportsFunction::Bind(ClientContext &context, TableFunctionBindInput &input,
                                                  vector<LogicalType> &return_types, vector<string> &names) {
	if (input.inputs.size() != 1) {
		throw BinderException("ast_imports requires exactly 1 argument: ast");
	}

	auto json_data = input.inputs[0].GetValue<string>();

	names = {"module", "names", "alias", "line", "is_from_import"};
	return_types = {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR), LogicalType::VARCHAR,
	                LogicalType::INTEGER, LogicalType::BOOLEAN};

	return make_uniq<ASTHelperData>(json_data);
}

void ASTImportsFunction::Execute(ClientContext &context, TableFunctionInput &data_p, DataChunk &output) {
	auto &data = data_p.bind_data->CastNoConst<ASTHelperData>();

	// Parse AST on first execution
	if (!data.ast && data.current_idx == 0) {
		data.ast = ParseASTFromJSON(data.json_data);

		for (const auto &node : data.ast->GetNodes()) {
			if (node.type == "import_statement" || node.type == "import_from_statement") {
				data.nodes.push_back(node);
			}
		}
	}

	idx_t count = 0;
	auto module_data = FlatVector::GetData<string_t>(output.data[0]);
	auto alias_data = FlatVector::GetData<string_t>(output.data[2]);
	auto line_data = FlatVector::GetData<int32_t>(output.data[3]);
	auto is_from_data = FlatVector::GetData<bool>(output.data[4]);
	auto &alias_validity = FlatVector::Validity(output.data[2]);

	while (data.current_idx < data.nodes.size() && count < STANDARD_VECTOR_SIZE) {
		const auto &import_node = data.nodes[data.current_idx];

		// For now, use simplified extraction
		module_data[count] =
		    StringVector::AddString(output.data[0], import_node.name.empty() ? "unknown" : import_node.name);
		line_data[count] = import_node.start_line;
		is_from_data[count] = import_node.type == "import_from_statement";

		// No alias for now
		alias_validity.SetInvalid(count);

		// Empty names list for now
		ListVector::SetListSize(output.data[1], 0);
		auto list_entries = FlatVector::GetData<list_entry_t>(output.data[1]);
		list_entries[count].offset = 0;
		list_entries[count].length = 0;

		count++;
		data.current_idx++;
	}

	output.SetCardinality(count);

	if (data.current_idx >= data.nodes.size()) {
		data.current_idx = 0;
	}
}

void RegisterASTHelperFunctions(ExtensionLoader &loader) {
	loader.RegisterFunction(ASTFunctionsFunction::GetFunction());
	loader.RegisterFunction(ASTClassesFunction::GetFunction());
	loader.RegisterFunction(ASTImportsFunction::GetFunction());
}

} // namespace duckdb
