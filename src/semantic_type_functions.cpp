#include "duckdb.hpp"
#include "duckdb_compat.hpp"
#include "function_doc_helper.hpp"
#include "include/semantic_types.hpp"
#include "include/node_config.hpp"
#include "include/ast_file_utils.hpp"
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

		    // ================================================================
		    // ALIAS TABLE: maps short selector names to semantic type checks.
		    // Ordered by measured call frequency from real workloads — the
		    // hottest super-types come first so a typical predicate hits its
		    // branch in 1-3 string comparisons rather than 30+. CALL is
		    // particularly hot via ::callers/::callees and was previously
		    // ~3x slower than FUNCTION purely due to its position in the
		    // cascade. Three levels remain available:
		    //   Super-type (mid):  .func, .class, .call, .cond, .loop, ...
		    //   Kind (broad):      .def, .flow, .literal, .external, ...
		    //   Existing names:    FUNCTION, CLASS, DEFINITION, LITERAL, ...
		    // ================================================================

		    // --- Tier 1: hottest super-types (fast path for selector engine) ---
		    if (pattern == "FUNCTION" || pattern == "FUNC" || pattern == "FN" || pattern == "METHOD") {
			    return base_type == SemanticTypes::DEFINITION_FUNCTION;
		    } else if (pattern == "CALL" || pattern == "INVOKE") {
			    return base_type == SemanticTypes::COMPUTATION_CALL;
		    } else if (pattern == "CLASS" || pattern == "CLS" || pattern == "STRUCT" || pattern == "TRAIT" ||
		               pattern == "INTERFACE") {
			    return base_type == SemanticTypes::DEFINITION_CLASS;
		    } else if (pattern == "IDENTIFIER" || pattern == "ID" || pattern == "IDENT") {
			    return base_type == SemanticTypes::NAME_IDENTIFIER;
		    } else if (pattern == "MODULE" || pattern == "MOD" || pattern == "PACKAGE" || pattern == "NAMESPACE" ||
		               pattern == "NS") {
			    // NAMESPACE/NS map to DEFINITION_MODULE because the current
			    // taxonomy's 4 DEFINITION super-type slots are full
			    // (FUNCTION/VARIABLE/CLASS/MODULE) and DEFINITION_MODULE is
			    // the conceptual home for any named module/namespace
			    // definition (Python module, C++/C# namespace, Rust mod,
			    // Java package decl). A future refinement could split
			    // namespace from module by repurposing the language-specific
			    // refinement bits — see tracker entry on namespace taxonomy.
			    return base_type == SemanticTypes::DEFINITION_MODULE;
		    } else if (pattern == "VARIABLE" || pattern == "VAR" || pattern == "LET" || pattern == "CONST") {
			    return base_type == SemanticTypes::DEFINITION_VARIABLE;

			    // --- Tier 2: common control-flow super-types ---
		    } else if (pattern == "CONDITIONAL" || pattern == "COND" || pattern == "IF") {
			    return base_type == SemanticTypes::FLOW_CONDITIONAL;
		    } else if (pattern == "LOOP" || pattern == "FOR" || pattern == "WHILE") {
			    return base_type == SemanticTypes::FLOW_LOOP;
		    } else if (pattern == "JUMP" || pattern == "RETURN" || pattern == "BREAK" || pattern == "CONTINUE" ||
		               pattern == "YIELD") {
			    return base_type == SemanticTypes::FLOW_JUMP;

			    // --- Tier 3: common kind-level patterns ---
		    } else if (pattern == "DEFINITION" || pattern == "DEF") {
			    return (base_type & 0xF0) == SemanticTypes::DEFINITION;
		    } else if (pattern == "LITERAL" || pattern == "LIT" || pattern == "VALUE") {
			    return (base_type & 0xF0) == SemanticTypes::LITERAL;
		    } else if (pattern == "NAME") {
			    return (base_type & 0xF0) == SemanticTypes::NAME;
		    } else if (pattern == "FLOW" || pattern == "CONTROL") {
			    return (base_type & 0xF0) == SemanticTypes::FLOW_CONTROL;
		    } else if (pattern == "EXTERNAL" || pattern == "EXT") {
			    return (base_type & 0xF0) == SemanticTypes::EXTERNAL;

			    // --- Tier 4: less-common super-types ---
		    } else if (pattern == "MEMBER" || pattern == "ATTR" || pattern == "FIELD" || pattern == "PROP") {
			    return base_type == SemanticTypes::COMPUTATION_ACCESS;
		    } else if (pattern == "IMPORT" || pattern == "REQUIRE" || pattern == "USE") {
			    return base_type == SemanticTypes::EXTERNAL_IMPORT;
		    } else if (pattern == "EXPORT" || pattern == "PUB") {
			    return base_type == SemanticTypes::EXTERNAL_EXPORT;

			    // --- Tier 5: error-handling super-types ---
		    } else if (pattern == "TRY") {
			    return base_type == SemanticTypes::ERROR_TRY;
		    } else if (pattern == "CATCH" || pattern == "EXCEPT" || pattern == "RESCUE") {
			    return base_type == SemanticTypes::ERROR_CATCH;
		    } else if (pattern == "THROW" || pattern == "RAISE") {
			    return base_type == SemanticTypes::ERROR_THROW;
		    } else if (pattern == "FINALLY" || pattern == "ENSURE" || pattern == "DEFER") {
			    return base_type == SemanticTypes::ERROR_FINALLY;

			    // --- Tier 6: literal super-types ---
		    } else if (pattern == "STR" || pattern == "STRING") {
			    return base_type == SemanticTypes::LITERAL_STRING;
		    } else if (pattern == "NUM" || pattern == "NUMBER") {
			    return base_type == SemanticTypes::LITERAL_NUMBER;
		    } else if (pattern == "BOOL" || pattern == "BOOLEAN") {
			    return base_type == SemanticTypes::LITERAL_ATOMIC;
		    } else if (pattern == "COLL" || pattern == "LIST" || pattern == "DICT" || pattern == "ARRAY" ||
		               pattern == "MAP" || pattern == "SET" || pattern == "TUPLE") {
			    return base_type == SemanticTypes::LITERAL_STRUCTURED;

			    // --- Tier 7: name / operator / transform super-types ---
		    } else if (pattern == "QUALIFIED" || pattern == "DOTTED") {
			    return base_type == SemanticTypes::NAME_QUALIFIED;
		    } else if (pattern == "SELF" || pattern == "THIS") {
			    return base_type == SemanticTypes::NAME_SCOPED;
		    } else if (pattern == "LABEL") {
			    return base_type == SemanticTypes::NAME_ATTRIBUTE;
		    } else if (pattern == "ARITH" || pattern == "MATH") {
			    return base_type == SemanticTypes::OPERATOR_ARITHMETIC;
		    } else if (pattern == "CMP" || pattern == "COMPARISON") {
			    return base_type == SemanticTypes::OPERATOR_COMPARISON;
		    } else if (pattern == "LOGIC" || pattern == "LOGICAL") {
			    return base_type == SemanticTypes::OPERATOR_LOGICAL;
		    } else if (pattern == "COMP" || pattern == "COMPREHENSION") {
			    return base_type == SemanticTypes::TRANSFORM_QUERY;

			    // --- Tier 8: rare kind-level patterns ---
		    } else if (pattern == "COMPUTATION") {
			    return (base_type & 0xC0) == SemanticTypes::COMPUTATION;
		    } else if (pattern == "ERROR" || pattern == "ERR") {
			    return (base_type & 0xF0) == SemanticTypes::ERROR_HANDLING;
		    } else if (pattern == "OPERATOR" || pattern == "OP") {
			    return (base_type & 0xF0) == SemanticTypes::OPERATOR;
		    } else if (pattern == "TYPEDEF" || pattern == "TYPE") {
			    return (base_type & 0xF0) == SemanticTypes::TYPE;
		    } else if (pattern == "PATTERN" || pattern == "PAT") {
			    return (base_type & 0xF0) == SemanticTypes::PATTERN;
		    } else if (pattern == "BLOCK") {
			    return (base_type & 0xF0) == SemanticTypes::ORGANIZATION;
		    } else if (pattern == "STATEMENT" || pattern == "STMT") {
			    return (base_type & 0xF0) == SemanticTypes::EXECUTION;
		    } else if (pattern == "SYNTAX" || pattern == "SYN") {
			    return (base_type & 0xF0) == SemanticTypes::PARSER_SPECIFIC;
		    } else if (pattern == "TRANSFORM" || pattern == "XFORM") {
			    return (base_type & 0xF0) == SemanticTypes::TRANSFORM;
		    } else if (pattern == "COMMENT") {
			    // Narrowed from the kind-level METADATA mask to the COMMENT super-type
			    // only (issue #134). METADATA also covers ANNOTATION (decorators/
			    // attributes), DIRECTIVE, and DEBUG, so the old `& 0xF0` made `.comment`
			    // match decorators — a false positive peers hit. Use METADATA/META
			    // (below) for the whole kind. Parallel to the .access fix (#135).
			    return base_type == SemanticTypes::METADATA_COMMENT;
		    } else if (pattern == "METADATA" || pattern == "META") {
			    // Kind-level umbrella: comments, annotations, directives, debug info.
			    return (base_type & 0xF0) == SemanticTypes::METADATA;
		    } else if (pattern == "ACCESS") {
			    // The COMPUTATION_NODE kind (0xD0): calls/access/expression/closure.
			    // Was `== COMPUTATION` (0xC0) — the top-level quadrant constant, whose
			    // `& 0xF0` lands on the OPERATOR kind, so `.access` matched arithmetic/
			    // logical/comparison/assignment operators and MISSED the actual
			    // call/access nodes. Kind-level alias, parallel to the other Tier-8
			    // `& 0xF0` entries. (COMPUTATION_ACCESS specifically stays .member/.attr.)
			    return (base_type & 0xF0) == SemanticTypes::COMPUTATION_NODE;
		    }

		    // Default: exact string match with full semantic type name
		    return SemanticTypes::GetSemanticTypeName(base_type) == pattern;
	    });
}

// Function that converts semantic type name to code
static void SemanticTypeCodeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &name_vector = args.data[0];
	auto count = args.size();

	CompatUnaryExecuteWithNulls<string_t, uint8_t>(name_vector, result, count,
	                                               [&](string_t name_str, ValidityMask &mask, idx_t idx) {
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

	CompatUnaryExecuteWithNulls<string_t, uint8_t>(name_vector, result, count,
	                                               [&](string_t name_str, ValidityMask &mask, idx_t idx) {
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

// ============================================================================
// NAME_ROLE Flag Functions (bits 1-2 of flags byte)
// ============================================================================

// Check if node is a definition by flags (introduces name with implementation)
static void IsDefinitionFlagFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);
	auto &flags_vector = args.data[0];
	auto count = args.size();
	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count, [&](uint8_t flags) {
		return (flags & ASTNodeFlags::NAME_ROLE_MASK) == ASTNodeFlags::NAME_DEFINITION;
	});
}

// Check if node is a declaration by flags (introduces name without implementation)
static void IsDeclarationFlagFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);
	auto &flags_vector = args.data[0];
	auto count = args.size();
	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count, [&](uint8_t flags) {
		return (flags & ASTNodeFlags::NAME_ROLE_MASK) == ASTNodeFlags::NAME_DECLARATION;
	});
}

// Check if node is a reference by flags (uses a name)
static void IsReferenceFlagFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);
	auto &flags_vector = args.data[0];
	auto count = args.size();
	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count, [&](uint8_t flags) {
		return (flags & ASTNodeFlags::NAME_ROLE_MASK) == ASTNodeFlags::NAME_REFERENCE;
	});
}

// Check if node binds a name (definition OR declaration — bit 2 set)
static void BindsNameFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);
	auto &flags_vector = args.data[0];
	auto count = args.size();
	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count,
	                                      [&](uint8_t flags) { return (flags & ASTNodeFlags::BINDS_NAME) != 0; });
}

// Check if node creates a scope boundary
static void IsScopeFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);
	auto &flags_vector = args.data[0];
	auto count = args.size();
	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count,
	                                      [&](uint8_t flags) { return (flags & ASTNodeFlags::IS_SCOPE) != 0; });
}

// Check if node is exported (visible outside file/module)
static void IsExportedFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);
	auto &flags_vector = args.data[0];
	auto count = args.size();
	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count,
	                                      [&](uint8_t flags) { return (flags & ASTNodeFlags::IS_EXPORTED) != 0; });
}

// Check if node is a constituent — a meaningful sub-part of a larger construct
// that already represents the whole (string content, a function declarator, an
// import specifier). Distinct from is_syntax_only: a constituent carries payload
// but is subordinate; class selectors skip it in favour of the enclosing construct.
static void IsConstituentFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);
	auto &flags_vector = args.data[0];
	auto count = args.size();
	UnaryExecutor::Execute<uint8_t, bool>(flags_vector, result, count,
	                                      [&](uint8_t flags) { return (flags & ASTNodeFlags::IS_CONSTITUENT) != 0; });
}

// Get the name role as an integer (0=none, 1=reference, 2=declaration, 3=definition)
static void NameRoleFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);
	auto &flags_vector = args.data[0];
	auto count = args.size();
	UnaryExecutor::Execute<uint8_t, uint8_t>(flags_vector, result, count, [&](uint8_t flags) {
		return (uint8_t)((flags & ASTNodeFlags::NAME_ROLE_MASK) >> 1);
	});
}

// DEPRECATED: backward compatibility wrappers
static void IsDeclarationOnlyFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	IsDeclarationFlagFunction(args, state, result);
}

static void HasBodyFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	IsDefinitionFlagFunction(args, state, result);
}

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

	auto result_data = CompatFlatDataMutable<bool>(result);
	auto &result_validity = CompatFlatValidityMutable<>(result);

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

	auto result_data = CompatFlatDataMutable<bool>(result);
	auto &result_validity = CompatFlatValidityMutable<>(result);

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
	auto list_entries = CompatFlatDataMutable<list_entry_t>(list_vector);

	// Create child vector to hold the semantic type values
	auto list_size = searchable_types.size();
	ListVector::Reserve(list_vector, list_size * count);
	auto &child_vector = ListVector::GetEntry(list_vector);
	auto child_data = CompatFlatDataMutable<uint8_t>(child_vector);

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

// ============================================================================
// Language Detection Functions
// ============================================================================

// detect_language(file_path) -> VARCHAR or NULL
// Exposes the internal filename-to-language detection used by read_ast
static void DetectLanguageFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &path_vector = args.data[0];
	auto count = args.size();

	CompatUnaryExecuteWithNulls<string_t, string_t>(
	    path_vector, result, count, [&](string_t path_str, ValidityMask &mask, idx_t idx) {
		    string file_path = path_str.GetString();
		    string language = ASTFileUtils::DetectLanguageFromPath(file_path);
		    if (language == "auto") {
			    // "auto" means unrecognized - return NULL
			    mask.SetInvalid(idx);
			    return string_t();
		    }
		    return StringVector::AddString(result, language);
	    });
}

void RegisterSemanticTypeFunctions(ExtensionLoader &loader) {
	// Register semantic_type_to_string(semantic_type) -> VARCHAR
	ScalarFunction semantic_type_to_string_func("semantic_type_to_string", {LogicalType::UTINYINT},
	                                            LogicalType::VARCHAR, SemanticTypeToStringFunction);
	RegisterDocumentedScalarFunction(loader, semantic_type_to_string_func,
	                                 "Convert a numeric semantic type bitmask to its human-readable name string.",
	                                 {"semantic_type"}, {"semantic_type_to_string(1::UTINYINT)"},
	                                 {"sitting_duck", "taxonomy"});

	// Register get_super_kind(semantic_type) -> VARCHAR
	ScalarFunction get_super_kind_func("get_super_kind", {LogicalType::UTINYINT}, LogicalType::VARCHAR,
	                                   GetSuperKindFunction);
	RegisterDocumentedScalarFunction(
	    loader, get_super_kind_func,
	    "Get the super-kind category name (DEFINITION, EXECUTION, STRUCTURE, LITERAL, etc.) for a semantic type.",
	    {"semantic_type"}, {"get_super_kind(semantic_type)"}, {"sitting_duck", "taxonomy"});

	// Register get_kind(semantic_type) -> VARCHAR
	ScalarFunction get_kind_func("get_kind", {LogicalType::UTINYINT}, LogicalType::VARCHAR, GetKindFunction);
	RegisterDocumentedScalarFunction(loader, get_kind_func,
	                                 "Get the primary kind name (FUNCTION, CLASS, VARIABLE, etc.) for a semantic type.",
	                                 {"semantic_type"}, {"get_kind(semantic_type)"}, {"sitting_duck", "taxonomy"});

	// Register is_semantic_type(semantic_type, pattern) -> BOOLEAN
	ScalarFunction is_semantic_type_func("is_semantic_type", {LogicalType::UTINYINT, LogicalType::VARCHAR},
	                                     LogicalType::BOOLEAN, IsSemanticTypeFunction);
	RegisterDocumentedScalarFunction(loader, is_semantic_type_func,
	                                 "Check if a semantic type bitmask matches a taxonomy pattern string.",
	                                 {"semantic_type", "pattern"}, {"is_semantic_type(semantic_type, 'DEFINITION_%')"},
	                                 {"sitting_duck", "taxonomy"});

	// Register semantic_type_code(name) -> UTINYINT
	ScalarFunction semantic_type_code_func("semantic_type_code", {LogicalType::VARCHAR}, LogicalType::UTINYINT,
	                                       SemanticTypeCodeFunction);
	RegisterDocumentedScalarFunction(loader, semantic_type_code_func,
	                                 "Convert a semantic type name string to its numeric bitmask code.", {"name"},
	                                 {"semantic_type_code('DEFINITION_FUNCTION')"}, {"sitting_duck", "taxonomy"});

	// Register predicate functions
	ScalarFunction is_definition_func("is_definition", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                  IsDefinitionFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_definition_func,
	    "Check if a semantic type represents a definition (function, class, variable, etc.).", {"semantic_type"},
	    {"is_definition(semantic_type)"}, {"sitting_duck", "taxonomy"});

	ScalarFunction is_call_func("is_call", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsCallFunction);
	RegisterDocumentedScalarFunction(loader, is_call_func,
	                                 "Check if a semantic type represents a function/method call expression.",
	                                 {"semantic_type"}, {"is_call(semantic_type)"}, {"sitting_duck", "taxonomy"});

	ScalarFunction is_control_flow_func("is_control_flow", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                    IsControlFlowFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_control_flow_func,
	    "Check if a semantic type represents a control flow construct (if, loop, switch, return, etc.).",
	    {"semantic_type"}, {"is_control_flow(semantic_type)"}, {"sitting_duck", "taxonomy"});

	ScalarFunction is_identifier_func("is_identifier", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                  IsIdentifierFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_identifier_func,
	    "Check if a semantic type represents an identifier (variable name, function name, etc.).", {"semantic_type"},
	    {"is_identifier(semantic_type)"}, {"sitting_duck", "taxonomy"});

	ScalarFunction is_parser_specific_func("is_parser_specific", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                       IsParserSpecificFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_parser_specific_func,
	    "Check if a semantic type is parser-specific/unclassified in the universal taxonomy.", {"semantic_type"},
	    {"is_parser_specific(semantic_type)"}, {"sitting_duck", "taxonomy"});

	ScalarFunction is_punctuation_func("is_punctuation", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                   IsPunctuationFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_punctuation_func, "Check if a semantic type represents punctuation or syntax delimiter.",
	    {"semantic_type"}, {"is_punctuation(semantic_type)"}, {"sitting_duck", "taxonomy"});

	// Register get_searchable_types() -> LIST<UTINYINT>
	ScalarFunction get_searchable_types_func("get_searchable_types", {}, LogicalType::LIST(LogicalType::UTINYINT),
	                                         GetSearchableTypesFunction);
	RegisterDocumentedScalarFunction(loader, get_searchable_types_func,
	                                 "Get list of all searchable semantic type codes.", {}, {"get_searchable_types()"},
	                                 {"sitting_duck", "taxonomy"});

	// Register kind_code(name) -> UTINYINT
	ScalarFunction kind_code_func("kind_code", {LogicalType::VARCHAR}, LogicalType::UTINYINT, KindCodeFunction);
	RegisterDocumentedScalarFunction(loader, kind_code_func, "Get the numeric code for a taxonomy kind name.", {"name"},
	                                 {"kind_code('FUNCTION')"}, {"sitting_duck", "taxonomy"});

	// Register is_kind(semantic_type, kind_name) -> BOOLEAN
	ScalarFunction is_kind_func("is_kind", {LogicalType::UTINYINT, LogicalType::VARCHAR}, LogicalType::BOOLEAN,
	                            IsKindFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_kind_func, "Check if a semantic type code belongs to the specified kind.",
	    {"semantic_type", "kind_name"}, {"is_kind(semantic_type, 'FUNCTION')"}, {"sitting_duck", "taxonomy"});

	// ========================================================================
	// Flag Helper Functions
	// ========================================================================

	// Register is_syntax_only(flags) -> BOOLEAN
	ScalarFunction is_syntax_only_func("is_syntax_only", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                   IsSyntaxOnlyFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_syntax_only_func,
	    "Returns true if node is a pure syntax token (keyword, punctuation) with no semantic payload.", {"flags"},
	    {"is_syntax_only(flags)"}, {"sitting_duck", "flags"});

	// Register is_construct(flags) -> BOOLEAN
	ScalarFunction is_construct_func("is_construct", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                 IsConstructFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_construct_func,
	    "Returns true if node is a meaningful semantic construct (inverse of is_syntax_only).", {"flags"},
	    {"is_construct(flags)"}, {"sitting_duck", "flags"});

	// NAME_ROLE flag functions (bits 1-2 of flags byte)
	ScalarFunction is_name_definition_func("is_name_definition", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                       IsDefinitionFlagFunction);
	RegisterDocumentedScalarFunction(loader, is_name_definition_func,
	                                 "Returns true if node represents a name definition.", {"flags"},
	                                 {"is_name_definition(flags)"}, {"sitting_duck", "flags"});

	ScalarFunction is_name_declaration_func("is_name_declaration", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                        IsDeclarationFlagFunction);
	RegisterDocumentedScalarFunction(loader, is_name_declaration_func,
	                                 "Returns true if node represents a name declaration.", {"flags"},
	                                 {"is_name_declaration(flags)"}, {"sitting_duck", "flags"});

	ScalarFunction is_name_reference_func("is_name_reference", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                      IsReferenceFlagFunction);
	RegisterDocumentedScalarFunction(loader, is_name_reference_func,
	                                 "Returns true if node represents a name reference.", {"flags"},
	                                 {"is_name_reference(flags)"}, {"sitting_duck", "flags"});

	ScalarFunction binds_name_func("binds_name", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, BindsNameFunction);
	RegisterDocumentedScalarFunction(loader, binds_name_func,
	                                 "Returns true if node binds a name in lexical scope (definition or declaration).",
	                                 {"flags"}, {"binds_name(flags)"}, {"sitting_duck", "flags"});

	ScalarFunction name_role_func("name_role", {LogicalType::UTINYINT}, LogicalType::UTINYINT, NameRoleFunction);
	RegisterDocumentedScalarFunction(
	    loader, name_role_func,
	    "Get the numeric NameRole code (0=NONE, 1=DECLARATION, 2=DEFINITION, 3=REFERENCE) from flags.", {"flags"},
	    {"name_role(flags)"}, {"sitting_duck", "flags"});

	// IS_SCOPE flag function (bit 3)
	ScalarFunction is_scope_func("is_scope", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsScopeFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_scope_func,
	    "Returns true if node establishes a lexical scope boundary (function, class, block, module).", {"flags"},
	    {"is_scope(flags)"}, {"sitting_duck", "flags"});

	// IS_EXPORTED flag function (bit 4)
	ScalarFunction is_exported_func("is_exported", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsExportedFunction);
	RegisterDocumentedScalarFunction(loader, is_exported_func,
	                                 "Returns true if node is exported from its containing module/package.", {"flags"},
	                                 {"is_exported(flags)"}, {"sitting_duck", "flags"});

	// IS_CONSTITUENT flag function (bit 5)
	ScalarFunction is_constituent_func("is_constituent", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                   IsConstituentFunction);
	RegisterDocumentedScalarFunction(loader, is_constituent_func,
	                                 "Returns true if node is an internal constituent part of a compound construct.",
	                                 {"flags"}, {"is_constituent(flags)"}, {"sitting_duck", "flags"});

	// DEPRECATED: backward compatibility wrappers
	ScalarFunction is_declaration_only_func("is_declaration_only", {LogicalType::UTINYINT}, LogicalType::BOOLEAN,
	                                        IsDeclarationOnlyFunction);
	RegisterDocumentedScalarFunction(
	    loader, is_declaration_only_func,
	    "Returns true if node is a declaration without an accompanying implementation body.", {"flags"},
	    {"is_declaration_only(flags)"}, {"sitting_duck", "flags"});

	ScalarFunction has_body_func("has_body", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, HasBodyFunction);
	RegisterDocumentedScalarFunction(loader, has_body_func, "Returns true if node has an embodied implementation body.",
	                                 {"flags"}, {"has_body(flags)"}, {"sitting_duck", "flags"});

	ScalarFunction is_embodied_func("is_embodied", {LogicalType::UTINYINT}, LogicalType::BOOLEAN, IsEmbodiedFunction);
	RegisterDocumentedScalarFunction(loader, is_embodied_func,
	                                 "Returns true if node is an embodied definition (has implementation body).",
	                                 {"flags"}, {"is_embodied(flags)"}, {"sitting_duck", "flags"});

	// ========================================================================
	// String Utility Functions
	// ========================================================================

	// Register string_contains_any(str, patterns) -> BOOLEAN
	ScalarFunction string_contains_any_func("string_contains_any",
	                                        {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR)},
	                                        LogicalType::BOOLEAN, StringContainsAnyFunction);
	RegisterDocumentedScalarFunction(
	    loader, string_contains_any_func, "Case-sensitive search checking if string contains any pattern in the list.",
	    {"str", "patterns"}, {"string_contains_any('hello world', ['world', 'foo'])"}, {"sitting_duck", "utility"});

	// Register string_contains_any_i(str, patterns) -> BOOLEAN
	ScalarFunction string_contains_any_i_func("string_contains_any_i",
	                                          {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR)},
	                                          LogicalType::BOOLEAN, StringContainsAnyIFunction);
	RegisterDocumentedScalarFunction(loader, string_contains_any_i_func,
	                                 "Case-insensitive search checking if string contains any pattern in the list.",
	                                 {"str", "patterns"}, {"string_contains_any_i('Hello World', ['world', 'foo'])"},
	                                 {"sitting_duck", "utility"});

	// Register ast_peek_contains_any as alias for string_contains_any
	ScalarFunction peek_contains_any_func("ast_peek_contains_any",
	                                      {LogicalType::VARCHAR, LogicalType::LIST(LogicalType::VARCHAR)},
	                                      LogicalType::BOOLEAN, StringContainsAnyFunction);
	RegisterDocumentedScalarFunction(loader, peek_contains_any_func,
	                                 "Case-sensitive search checking if peek string contains any pattern in the list "
	                                 "(alias of string_contains_any).",
	                                 {"peek_str", "patterns"}, {"ast_peek_contains_any(peek, ['import', 'require'])"},
	                                 {"sitting_duck", "utility"});

	// ========================================================================
	// Language Detection Functions
	// ========================================================================

	// Register detect_language(file_path) -> VARCHAR
	ScalarFunction detect_language_func("detect_language", {LogicalType::VARCHAR}, LogicalType::VARCHAR,
	                                    DetectLanguageFunction);
	RegisterDocumentedScalarFunction(loader, detect_language_func,
	                                 "Detect tree-sitter language name from file extension, or return NULL if unknown.",
	                                 {"file_path"}, {"detect_language('src/main.rs')"}, {"sitting_duck", "metadata"});
}

} // namespace duckdb
