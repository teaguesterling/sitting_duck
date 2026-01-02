#include "duckdb.hpp"
#include "include/semantic_types.hpp"
#include "include/node_config.hpp"
#include "duckdb/common/types/vector.hpp"

namespace duckdb {

// Scalar function that converts semantic_type integer to human-readable string
static void SemanticTypeToStringFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, string_t>(semantic_type_vector, result, count, [&](uint8_t semantic_type) {
		// For backward compatibility, mask out refinement bits but only for refined types
		// Check if this could be a valid refined type (base type exists + has refinements)
		uint8_t refinement_bits = semantic_type & 0x03;
		uint8_t base_semantic_type = semantic_type & 0xFC;

		if (refinement_bits != 0) {
			// Has refinement bits - check if base type is valid
			string base_type_name = SemanticTypes::GetSemanticTypeName(base_semantic_type);
			if (base_type_name != "UNKNOWN_SEMANTIC_TYPE") {
				// Valid base type with refinements - return base type name for compatibility
				return StringVector::AddString(result, base_type_name);
			}
		}

		// No refinements or invalid base type - return as-is (might be UNKNOWN)
		string type_name = SemanticTypes::GetSemanticTypeName(semantic_type);
		return StringVector::AddString(result, type_name);
	});
}

// Function that gets the super kind name from semantic type
static void GetSuperKindFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, string_t>(semantic_type_vector, result, count, [&](uint8_t semantic_type) {
		uint8_t super_kind = SemanticTypes::GetSuperKind(semantic_type);
		string super_kind_name = SemanticTypes::GetSuperKindName(super_kind);
		return StringVector::AddString(result, super_kind_name);
	});
}

// Function that gets the kind name from semantic type
static void GetKindFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, string_t>(semantic_type_vector, result, count, [&](uint8_t semantic_type) {
		uint8_t kind = SemanticTypes::GetKind(semantic_type);
		string kind_name = SemanticTypes::GetKindName(kind);
		return StringVector::AddString(result, kind_name);
	});
}

// Function that checks if semantic type matches a specific pattern
static void IsSemanticTypeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 2);

	auto &semantic_type_vector = args.data[0];
	auto &pattern_vector = args.data[1];
	auto count = args.size();

	BinaryExecutor::Execute<uint8_t, string_t, bool>(
	    semantic_type_vector, pattern_vector, result, count, [&](uint8_t semantic_type, string_t pattern_str) {
		    string pattern = pattern_str.GetString();
		    uint8_t type = semantic_type;
		    uint8_t base_type = semantic_type & 0xFC; // Mask refinement bits for base comparisons

		    // Support common patterns
		    if (pattern == "FUNCTION") {
			    return base_type == SemanticTypes::DEFINITION_FUNCTION;
		    } else if (pattern == "CLASS") {
			    return base_type == SemanticTypes::DEFINITION_CLASS;
		    } else if (pattern == "VARIABLE") {
			    return base_type == SemanticTypes::DEFINITION_VARIABLE;
		    } else if (pattern == "IDENTIFIER") {
			    return base_type == SemanticTypes::NAME_IDENTIFIER;
		    } else if (pattern == "CALL") {
			    return base_type == SemanticTypes::COMPUTATION_CALL;
		    } else if (pattern == "LITERAL") {
			    return (base_type & 0xF0) == SemanticTypes::LITERAL;
		    } else if (pattern == "DEFINITION") {
			    return (base_type & 0xF0) == SemanticTypes::DEFINITION;
		    } else if (pattern == "COMPUTATION") {
			    return (base_type & 0xC0) == SemanticTypes::COMPUTATION;
		    }

		    // Default: exact string match with base semantic type name
		    return SemanticTypes::GetSemanticTypeName(base_type) == pattern;
	    });
}

// Function that converts semantic type name to code
static void SemanticTypeCodeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &name_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::ExecuteWithNulls<string_t, uint8_t>(
	    name_vector, result, count, [&](string_t name_str, ValidityMask &mask, idx_t idx) {
		    string name = name_str.GetString();
		    uint8_t code = SemanticTypes::GetSemanticTypeCode(name);
		    if (code == 255) {
			    mask.SetInvalid(idx);
			    return uint8_t(0); // Return value doesn't matter when NULL
		    }
		    return code;
	    });
}

// Function that converts kind name to code
static void KindCodeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &name_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::ExecuteWithNulls<string_t, uint8_t>(
	    name_vector, result, count, [&](string_t name_str, ValidityMask &mask, idx_t idx) {
		    string name = name_str.GetString();
		    uint8_t code = SemanticTypes::GetKindCode(name);
		    if (code == 255) {
			    mask.SetInvalid(idx);
			    return uint8_t(0); // Return value doesn't matter when NULL
		    }
		    return code;
	    });
}

// Function that checks if semantic type belongs to a specific kind
static void IsKindFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 2);

	auto &semantic_type_vector = args.data[0];
	auto &kind_name_vector = args.data[1];
	auto count = args.size();

	BinaryExecutor::Execute<uint8_t, string_t, bool>(semantic_type_vector, kind_name_vector, result, count,
	                                                 [&](uint8_t semantic_type, string_t kind_str) {
		                                                 string kind_name = kind_str.GetString();
		                                                 uint8_t kind_code = SemanticTypes::GetKindCode(kind_name);
		                                                 if (kind_code == 255) {
			                                                 return false; // Invalid kind name
		                                                 }
		                                                 // Check if the semantic type's kind matches
		                                                 return (semantic_type & 0xF0) == kind_code;
	                                                 });
}

// Predicate functions for common type categories
static void IsDefinitionFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(semantic_type_vector, result, count, [&](uint8_t semantic_type) {
		return SemanticTypes::IsDefinition(semantic_type);
	});
}

static void IsCallFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(semantic_type_vector, result, count,
	                                      [&](uint8_t semantic_type) { return SemanticTypes::IsCall(semantic_type); });
}

static void IsControlFlowFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(semantic_type_vector, result, count, [&](uint8_t semantic_type) {
		return SemanticTypes::IsControlFlow(semantic_type);
	});
}

static void IsIdentifierFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(semantic_type_vector, result, count, [&](uint8_t semantic_type) {
		return SemanticTypes::IsIdentifier(semantic_type);
	});
}

static void IsParserSpecificFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(semantic_type_vector, result, count, [&](uint8_t semantic_type) {
		return SemanticTypes::IsParserSpecific(semantic_type);
	});
}

static void IsPunctuationFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &semantic_type_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(semantic_type_vector, result, count, [&](uint8_t semantic_type) {
		return SemanticTypes::IsPunctuation(semantic_type);
	});
}

// ============================================================================
// Flag Helper Functions
// ============================================================================

// Check if node is a syntax-only token (keyword, punctuation) vs semantic construct
static void IsSyntaxOnlyFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &flags_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count,
	                                      [&](uint8_t flags) { return (flags & ASTNodeFlags::IS_SYNTAX_ONLY) != 0; });
}

// Check if node is a semantic construct (NOT a syntax-only token)
// This is the inverse of is_syntax_only - returns true for meaningful code constructs
static void IsConstructFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &flags_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count, [&](uint8_t flags) {
		// A node is a construct if it's NOT syntax-only
		return (flags & ASTNodeFlags::IS_SYNTAX_ONLY) == 0;
	});
}

// Check if node is a declaration-only (no body/implementation)
static void IsDeclarationOnlyFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &flags_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(
	    flags_vector, result, count, [&](uint8_t flags) { return (flags & ASTNodeFlags::IS_DECLARATION_ONLY) != 0; });
}

// Check if node has body/implementation (default true, false only for forward declarations)
static void HasBodyFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &flags_vector = args.data[0];
	auto count = args.size();

	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count, [&](uint8_t flags) {
		// Has body unless explicitly marked as declaration-only
		return (flags & ASTNodeFlags::IS_DECLARATION_ONLY) == 0;
	});
}

// Alias for has_body - backward compatibility
static void IsEmbodiedFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	HasBodyFunction(args, state, result);
}

// ============================================================================
// String Utility Functions
// ============================================================================

// Check if a string contains any of the patterns in a list
// string_contains_any(str, ['pattern1', 'pattern2', ...]) -> BOOLEAN
// Case-sensitive by default
static void StringContainsAnyFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 2);

	auto &str_vector = args.data[0];
	auto &patterns_vector = args.data[1];
	auto count = args.size();

	// Get unified format for both vectors
	UnifiedVectorFormat str_data;
	str_vector.ToUnifiedFormat(count, str_data);
	auto str_entries = UnifiedVectorFormat::GetData<string_t>(str_data);

	UnifiedVectorFormat patterns_data;
	patterns_vector.ToUnifiedFormat(count, patterns_data);
	auto patterns_entries = UnifiedVectorFormat::GetData<list_entry_t>(patterns_data);

	// Get the child vector containing the actual pattern strings
	auto &patterns_child = ListVector::GetEntry(patterns_vector);
	UnifiedVectorFormat patterns_child_data;
	patterns_child.ToUnifiedFormat(ListVector::GetListSize(patterns_vector), patterns_child_data);
	auto patterns_child_entries = UnifiedVectorFormat::GetData<string_t>(patterns_child_data);

	auto result_data = FlatVector::GetData<bool>(result);
	auto &result_validity = FlatVector::Validity(result);

	for (idx_t i = 0; i < count; i++) {
		auto str_idx = str_data.sel->get_index(i);
		auto patterns_idx = patterns_data.sel->get_index(i);

		// Handle NULL inputs
		if (!str_data.validity.RowIsValid(str_idx) || !patterns_data.validity.RowIsValid(patterns_idx)) {
			result_validity.SetInvalid(i);
			continue;
		}

		auto str = str_entries[str_idx].GetString();
		auto &list_entry = patterns_entries[patterns_idx];

		bool found = false;
		for (idx_t j = 0; j < list_entry.length; j++) {
			auto pattern_idx = patterns_child_data.sel->get_index(list_entry.offset + j);
			if (!patterns_child_data.validity.RowIsValid(pattern_idx)) {
				continue; // Skip NULL patterns
			}
			auto pattern = patterns_child_entries[pattern_idx].GetString();
			if (str.find(pattern) != string::npos) {
				found = true;
				break;
			}
		}
		result_data[i] = found;
	}
}

// Case-insensitive version
// string_contains_any_i(str, ['pattern1', 'pattern2', ...]) -> BOOLEAN
static void StringContainsAnyIFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 2);

	auto &str_vector = args.data[0];
	auto &patterns_vector = args.data[1];
	auto count = args.size();

	// Get unified format for both vectors
	UnifiedVectorFormat str_data;
	str_vector.ToUnifiedFormat(count, str_data);
	auto str_entries = UnifiedVectorFormat::GetData<string_t>(str_data);

	UnifiedVectorFormat patterns_data;
	patterns_vector.ToUnifiedFormat(count, patterns_data);
	auto patterns_entries = UnifiedVectorFormat::GetData<list_entry_t>(patterns_data);

	// Get the child vector containing the actual pattern strings
	auto &patterns_child = ListVector::GetEntry(patterns_vector);
	UnifiedVectorFormat patterns_child_data;
	patterns_child.ToUnifiedFormat(ListVector::GetListSize(patterns_vector), patterns_child_data);
	auto patterns_child_entries = UnifiedVectorFormat::GetData<string_t>(patterns_child_data);

	auto result_data = FlatVector::GetData<bool>(result);
	auto &result_validity = FlatVector::Validity(result);

	// Helper to convert string to lowercase
	auto to_lower = [](const string &s) {
		string result;
		result.reserve(s.size());
		for (char c : s) {
			result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return result;
	};

	for (idx_t i = 0; i < count; i++) {
		auto str_idx = str_data.sel->get_index(i);
		auto patterns_idx = patterns_data.sel->get_index(i);

		// Handle NULL inputs
		if (!str_data.validity.RowIsValid(str_idx) || !patterns_data.validity.RowIsValid(patterns_idx)) {
			result_validity.SetInvalid(i);
			continue;
		}

		auto str = to_lower(str_entries[str_idx].GetString());
		auto &list_entry = patterns_entries[patterns_idx];

		bool found = false;
		for (idx_t j = 0; j < list_entry.length; j++) {
			auto pattern_idx = patterns_child_data.sel->get_index(list_entry.offset + j);
			if (!patterns_child_data.validity.RowIsValid(pattern_idx)) {
				continue; // Skip NULL patterns
			}
			auto pattern = to_lower(patterns_child_entries[pattern_idx].GetString());
			if (str.find(pattern) != string::npos) {
				found = true;
				break;
			}
		}
		result_data[i] = found;
	}
}

// Function that returns list of searchable semantic types
static void GetSearchableTypesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 0);

	auto searchable_types = SemanticTypes::GetSearchableTypes();
	auto count = args.size();

	// Create a list value for each row
	auto &list_vector = result;
	auto list_entries = FlatVector::GetData<list_entry_t>(list_vector);

	// Create child vector to hold the semantic type values
	auto list_size = searchable_types.size();
	ListVector::Reserve(list_vector, list_size * count);
	auto &child_vector = ListVector::GetEntry(list_vector);
	auto child_data = FlatVector::GetData<uint8_t>(child_vector);

	idx_t offset = 0;
	for (idx_t i = 0; i < count; i++) {
		list_entries[i].offset = offset;
		list_entries[i].length = list_size;

		// Copy semantic types to child vector
		for (idx_t j = 0; j < list_size; j++) {
			child_data[offset + j] = searchable_types[j];
		}
		offset += list_size;
	}

	ListVector::SetListSize(list_vector, offset);
}

void RegisterSemanticTypeFunctions(ExtensionLoader &loader) {

	// Register semantic_type_to_string(semantic_type) -> VARCHAR
	ScalarFunction semantic_type_to_string_func("semantic_type_to_string", {LogicalType::UTINYINT},
	                                            LogicalType::VARCHAR, SemanticTypeToStringFunction);
	loader.RegisterFunction(semantic_type_to_string_func);

	// Register get_super_kind(semantic_type) -> VARCHAR
	ScalarFunction get_super_kind_func("get_super_kind", {LogicalType::UTINYINT}, LogicalType::VARCHAR,
	                                   GetSuperKindFunction);
	loader.RegisterFunction(get_super_kind_func);

	// Register get_kind(semantic_type) -> VARCHAR
	ScalarFunction get_kind_func("get_kind", {LogicalType::UTINYINT}, LogicalType::VARCHAR, GetKindFunction);
	loader.RegisterFunction(get_kind_func);

	// Register is_semantic_type(semantic_type, pattern) -> BOOLEAN
	ScalarFunction is_semantic_type_func("is_semantic_type", {LogicalType::UTINYINT, LogicalType::VARCHAR},
	                                     LogicalType::BOOLEAN, IsSemanticTypeFunction);
	loader.RegisterFunction(is_semantic_type_func);

	// Register semantic_type_code(name) -> UTINYINT
	ScalarFunction semantic_type_code_func("semantic_type_code", {LogicalType::VARCHAR}, LogicalType::UTINYINT,
	                                       SemanticTypeCodeFunction);
	loader.RegisterFunction(semantic_type_code_func);

	// Register predicate functions
	ScalarFunction is_definition_func("is_definition", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                  IsDefinitionFunction);
	loader.RegisterFunction(is_definition_func);

	ScalarFunction is_call_func("is_call", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsCallFunction);
	loader.RegisterFunction(is_call_func);

	ScalarFunction is_control_flow_func("is_control_flow", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                    IsControlFlowFunction);
	loader.RegisterFunction(is_control_flow_func);

	ScalarFunction is_identifier_func("is_identifier", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                  IsIdentifierFunction);
	loader.RegisterFunction(is_identifier_func);

	ScalarFunction is_parser_specific_func("is_parser_specific", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                       IsParserSpecificFunction);
	loader.RegisterFunction(is_parser_specific_func);

	ScalarFunction is_punctuation_func("is_punctuation", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                   IsPunctuationFunction);
	loader.RegisterFunction(is_punctuation_func);

	// Register get_searchable_types() -> LIST<UTINYINT>
	ScalarFunction get_searchable_types_func("get_searchable_types", {}, LogicalType::LIST(LogicalType::UTINYINT),
	                                         GetSearchableTypesFunction);
	loader.RegisterFunction(get_searchable_types_func);

	// Register kind_code(name) -> UTINYINT
	ScalarFunction kind_code_func("kind_code", {LogicalType::VARCHAR}, LogicalType::UTINYINT, KindCodeFunction);
	loader.RegisterFunction(kind_code_func);

	// Register is_kind(semantic_type, kind_name) -> BOOLEAN
	ScalarFunction is_kind_func("is_kind", {LogicalType::UTINYINT, LogicalType::VARCHAR}, LogicalType::BOOLEAN,
	                            IsKindFunction);
	loader.RegisterFunction(is_kind_func);

	// ========================================================================
	// Flag Helper Functions
	// ========================================================================

	// Register is_syntax_only(flags) -> BOOLEAN
	// Returns true if node is a pure syntax token (keyword, punctuation)
	ScalarFunction is_syntax_only_func("is_syntax_only", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                   IsSyntaxOnlyFunction);
	loader.RegisterFunction(is_syntax_only_func);

	// Register is_construct(flags) -> BOOLEAN
	// Returns true if node is a semantic construct (NOT syntax-only)
	ScalarFunction is_construct_func("is_construct", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                 IsConstructFunction);
	loader.RegisterFunction(is_construct_func);

	// Register is_declaration_only(flags) -> BOOLEAN
	// Returns true if node is a forward declaration (no body)
	ScalarFunction is_declaration_only_func("is_declaration_only", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                        IsDeclarationOnlyFunction);
	loader.RegisterFunction(is_declaration_only_func);

	// Register has_body(flags) -> BOOLEAN
	// Returns true unless IS_DECLARATION_ONLY is set (default: has body)
	ScalarFunction has_body_func("has_body", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, HasBodyFunction);
	loader.RegisterFunction(has_body_func);

	// Register is_embodied(flags) -> BOOLEAN
	// Alias for has_body - backward compatibility
	ScalarFunction is_embodied_func("is_embodied", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsEmbodiedFunction);
	loader.RegisterFunction(is_embodied_func);

	// ========================================================================
	// String Utility Functions
	// ========================================================================

	// Register string_contains_any(str, patterns) -> BOOLEAN
	// Case-sensitive substring search for any pattern in the list
	ScalarFunction string_contains_any_func("string_contains_any",
	                                        {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR)},
	                                        LogicalType::BOOLEAN, StringContainsAnyFunction);
	loader.RegisterFunction(string_contains_any_func);

	// Register string_contains_any_i(str, patterns) -> BOOLEAN
	// Case-insensitive version
	ScalarFunction string_contains_any_i_func("string_contains_any_i",
	                                          {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR)},
	                                          LogicalType::BOOLEAN, StringContainsAnyIFunction);
	loader.RegisterFunction(string_contains_any_i_func);

	// Register ast_peek_contains_any as alias for string_contains_any
	// For use in AST queries where you're searching peek columns
	ScalarFunction peek_contains_any_func("ast_peek_contains_any",
	                                      {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR)},
	                                      LogicalType::BOOLEAN, StringContainsAnyFunction);
	loader.RegisterFunction(peek_contains_any_func);
}

} // namespace duckdb
