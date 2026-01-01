#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include "semantic_types.hpp"

namespace duckdb {

struct SemanticTypeCodesBindData : public TableFunctionData {
	SemanticTypeCodesBindData() = default;
};

struct SemanticTypeCodesGlobalState : public GlobalTableFunctionState {
	SemanticTypeCodesGlobalState() : current_code(0) {
	}
	uint8_t current_code;
};

static unique_ptr<FunctionData> SemanticTypeCodesBind(ClientContext &context, TableFunctionBindInput &input,
                                                      vector<LogicalType> &return_types, vector<string> &names) {
	return_types = {
	    LogicalType::UTINYINT, // code
	    LogicalType::VARCHAR,  // super_kind_name
	    LogicalType::VARCHAR,  // kind_name
	    LogicalType::VARCHAR,  // super_type_name
	    LogicalType::VARCHAR   // full_name
	};
	names = {"code", "super_kind_name", "kind_name", "super_type_name", "full_name"};

	return make_uniq<SemanticTypeCodesBindData>();
}

static unique_ptr<GlobalTableFunctionState> SemanticTypeCodesInit(ClientContext &context,
                                                                  TableFunctionInitInput &input) {
	return make_uniq<SemanticTypeCodesGlobalState>();
}

static void SemanticTypeCodesFunction(ClientContext &context, TableFunctionInput &data, DataChunk &output) {
	auto &global_state = data.global_state->Cast<SemanticTypeCodesGlobalState>();

	// Check if we're done
	if (global_state.current_code > 252) {
		output.SetCardinality(0);
		return;
	}

	idx_t output_count = 0;

	// Get output vectors
	auto code_vec = FlatVector::GetData<uint8_t>(output.data[0]);
	auto super_kind_vec = FlatVector::GetData<string_t>(output.data[1]);
	auto kind_vec = FlatVector::GetData<string_t>(output.data[2]);
	auto super_type_vec = FlatVector::GetData<string_t>(output.data[3]);
	auto full_name_vec = FlatVector::GetData<string_t>(output.data[4]);

	while (output_count < STANDARD_VECTOR_SIZE && global_state.current_code <= 252) {
		uint8_t code = global_state.current_code;

		// Extract bit fields
		uint8_t super_kind_bits = (code & 0xC0) >> 6; // bits 6-7
		uint8_t kind_bits = (code & 0x30) >> 4;       // bits 4-5
		uint8_t super_type_bits = (code & 0x0C) >> 2; // bits 2-3

		// Determine super kind name
		string super_kind_name;
		switch (super_kind_bits) {
		case 0:
			super_kind_name = "META_EXTERNAL";
			break;
		case 1:
			super_kind_name = "DATA_STRUCTURE";
			break;
		case 2:
			super_kind_name = "CONTROL_EFFECTS";
			break;
		case 3:
			super_kind_name = "COMPUTATION";
			break;
		}

		// Determine kind name based on super kind
		string kind_name;
		if (super_kind_bits == 0) { // META_EXTERNAL
			switch (kind_bits) {
			case 0:
				kind_name = "PARSER_SPECIFIC";
				break;
			case 1:
				kind_name = "RESERVED";
				break;
			case 2:
				kind_name = "METADATA";
				break;
			case 3:
				kind_name = "EXTERNAL";
				break;
			}
		} else if (super_kind_bits == 1) { // DATA_STRUCTURE
			switch (kind_bits) {
			case 0:
				kind_name = "LITERAL";
				break;
			case 1:
				kind_name = "NAME";
				break;
			case 2:
				kind_name = "PATTERN";
				break;
			case 3:
				kind_name = "TYPE";
				break;
			}
		} else if (super_kind_bits == 2) { // CONTROL_EFFECTS
			switch (kind_bits) {
			case 0:
				kind_name = "EXECUTION";
				break;
			case 1:
				kind_name = "FLOW_CONTROL";
				break;
			case 2:
				kind_name = "ERROR_HANDLING";
				break;
			case 3:
				kind_name = "ORGANIZATION";
				break;
			}
		} else if (super_kind_bits == 3) { // COMPUTATION
			switch (kind_bits) {
			case 0:
				kind_name = "OPERATOR";
				break;
			case 1:
				kind_name = "COMPUTATION_NODE";
				break;
			case 2:
				kind_name = "TRANSFORM";
				break;
			case 3:
				kind_name = "DEFINITION";
				break;
			}
		}

		// Determine super type name based on kind
		string super_type_name;
		if (super_kind_bits == 0 && kind_bits == 0) { // PARSER_SPECIFIC
			switch (super_type_bits) {
			case 0:
				super_type_name = "PARSER_CONSTRUCT";
				break;
			case 1:
				super_type_name = "PARSER_DELIMITER";
				break;
			case 2:
				super_type_name = "PARSER_PUNCTUATION";
				break;
			case 3:
				super_type_name = "PARSER_SYNTAX";
				break;
			}
		} else if (super_kind_bits == 0 && kind_bits == 2) { // METADATA
			switch (super_type_bits) {
			case 0:
				super_type_name = "METADATA_COMMENT";
				break;
			case 1:
				super_type_name = "METADATA_ANNOTATION";
				break;
			case 2:
				super_type_name = "METADATA_DIRECTIVE";
				break;
			case 3:
				super_type_name = "METADATA_DEBUG";
				break;
			}
		} else if (super_kind_bits == 0 && kind_bits == 3) { // EXTERNAL
			switch (super_type_bits) {
			case 0:
				super_type_name = "EXTERNAL_IMPORT";
				break;
			case 1:
				super_type_name = "EXTERNAL_EXPORT";
				break;
			case 2:
				super_type_name = "EXTERNAL_FOREIGN";
				break;
			case 3:
				super_type_name = "EXTERNAL_EMBED";
				break;
			}
		} else if (super_kind_bits == 1 && kind_bits == 0) { // LITERAL
			switch (super_type_bits) {
			case 0:
				super_type_name = "LITERAL_NUMBER";
				break;
			case 1:
				super_type_name = "LITERAL_STRING";
				break;
			case 2:
				super_type_name = "LITERAL_ATOMIC";
				break;
			case 3:
				super_type_name = "LITERAL_STRUCTURED";
				break;
			}
		} else if (super_kind_bits == 1 && kind_bits == 1) { // NAME
			switch (super_type_bits) {
			case 0:
				super_type_name = "NAME_KEYWORD";
				break;
			case 1:
				super_type_name = "NAME_IDENTIFIER";
				break;
			case 2:
				super_type_name = "NAME_QUALIFIED";
				break;
			case 3:
				super_type_name = "NAME_SCOPED";
				break;
			}
		} else if (super_kind_bits == 1 && kind_bits == 2) { // PATTERN
			switch (super_type_bits) {
			case 0:
				super_type_name = "PATTERN_DESTRUCTURE";
				break;
			case 1:
				super_type_name = "PATTERN_MATCH";
				break;
			case 2:
				super_type_name = "PATTERN_TEMPLATE";
				break;
			case 3:
				super_type_name = "PATTERN_GUARD";
				break;
			}
		} else if (super_kind_bits == 1 && kind_bits == 3) { // TYPE
			switch (super_type_bits) {
			case 0:
				super_type_name = "TYPE_PRIMITIVE";
				break;
			case 1:
				super_type_name = "TYPE_COMPOSITE";
				break;
			case 2:
				super_type_name = "TYPE_REFERENCE";
				break;
			case 3:
				super_type_name = "TYPE_GENERIC";
				break;
			}
		} else if (super_kind_bits == 3 && kind_bits == 0) { // OPERATOR
			switch (super_type_bits) {
			case 0:
				super_type_name = "OPERATOR_ARITHMETIC";
				break;
			case 1:
				super_type_name = "OPERATOR_LOGICAL";
				break;
			case 2:
				super_type_name = "OPERATOR_COMPARISON";
				break;
			case 3:
				super_type_name = "OPERATOR_ASSIGNMENT";
				break;
			}
		} else if (super_kind_bits == 3 && kind_bits == 1) { // COMPUTATION_NODE
			switch (super_type_bits) {
			case 0:
				super_type_name = "COMPUTATION_CALL";
				break;
			case 1:
				super_type_name = "COMPUTATION_ACCESS";
				break;
			case 2:
				super_type_name = "COMPUTATION_EXPRESSION";
				break;
			case 3:
				super_type_name = "COMPUTATION_CLOSURE";
				break;
			}
		} else if (super_kind_bits == 2 && kind_bits == 0) { // EXECUTION
			switch (super_type_bits) {
			case 0:
				super_type_name = "EXECUTION_STATEMENT";
				break;
			case 1:
				super_type_name = "EXECUTION_SIDE_EFFECT";
				break;
			case 2:
				super_type_name = "EXECUTION_MUTATION";
				break;
			case 3:
				super_type_name = "EXECUTION_IO";
				break;
			}
		} else if (super_kind_bits == 2 && kind_bits == 1) { // FLOW_CONTROL
			switch (super_type_bits) {
			case 0:
				super_type_name = "FLOW_CONDITIONAL";
				break;
			case 1:
				super_type_name = "FLOW_LOOP";
				break;
			case 2:
				super_type_name = "FLOW_JUMP";
				break;
			case 3:
				super_type_name = "FLOW_ASYNC";
				break;
			}
		} else {
			super_type_name = "UNKNOWN_" + to_string(super_type_bits);
		}

		// Get full name using existing function
		string full_name = SemanticTypes::GetSemanticTypeName(code);

		// Fill output vectors
		code_vec[output_count] = code;
		super_kind_vec[output_count] = StringVector::AddString(output.data[1], super_kind_name);
		kind_vec[output_count] = StringVector::AddString(output.data[2], kind_name);
		super_type_vec[output_count] = StringVector::AddString(output.data[3], super_type_name);
		full_name_vec[output_count] = StringVector::AddString(output.data[4], full_name);

		output_count++;
		global_state.current_code += 4; // Increment by 4 as requested
	}

	output.SetCardinality(output_count);
}

void RegisterSemanticTypeCodesFunction(ExtensionLoader &loader) {
	TableFunction semantic_type_codes("semantic_type_codes", {}, SemanticTypeCodesFunction, SemanticTypeCodesBind,
	                                  SemanticTypeCodesInit);
	semantic_type_codes.name = "semantic_type_codes";
	loader.RegisterFunction(semantic_type_codes);
}

} // namespace duckdb
