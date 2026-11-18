//============================================================================
//                         sitting_duck
//
// semantic_aliases.hpp
//
// The selector vocabulary, as DATA.
//============================================================================
//
// `.fn`, `.call`, `.def` and friends used to be a 44-arm if/else-if cascade inside
// IsSemanticTypeFunction. That shape had three consequences, all of them bugs
// rather than inefficiencies (sitting_duck#188):
//
//   1. Nothing could ENUMERATE the vocabulary, so a mistyped class could not be
//      distinguished from a valid class that happens to match nothing. `.frobnicate`
//      and `.annotation` both returned 0 rows in silence.
//   2. Nothing could DOCUMENT it either. 22 valid classes -- including `.name`, the
//      largest class in a typical Python file, and `.def` -- went unmentioned in the
//      reference because the only source of truth was unreadable C++ control flow.
//   3. The table and the docs could drift, with nothing to detect it.
//
// The cascade was ORDERED BY MEASURED CALL FREQUENCY so hot predicates exit in 1-3
// string comparisons. That ordering is preserved verbatim below -- a linear scan over
// this array in the same order has the same early-exit behaviour, so the fast path is
// unchanged. Do not sort this table alphabetically.
//
// mask semantics:
//   0xFF  exact base_type match      (was `base_type == CODE`)
//   0xF0  kind match                 (was `(base_type & 0xF0) == KIND`)
//   0xC0  quadrant match             (was `(base_type & 0xC0) == QUADRANT`)
//
// A pattern that is not in this table is still matched against the full semantic type
// name (e.g. `.DEFINITION_FUNCTION`), which is why GetSemanticTypeCode is consulted
// too when deciding whether a class is KNOWN.

#pragma once

#include "duckdb.hpp"
#include "semantic_types.hpp"

namespace duckdb {

struct SemanticAliasEntry {
	const char *alias;   //! the uppercase selector class, without the leading dot
	uint8_t code;        //! the semantic type / kind / quadrant it resolves to
	uint8_t mask;        //! 0xFF exact, 0xF0 kind, 0xC0 quadrant
};

//! Frequency-ordered. See the header comment before reordering.
static const SemanticAliasEntry SEMANTIC_ALIASES[] = {
    {"FUNCTION",      SemanticTypes::DEFINITION_FUNCTION, 0xFF},
    {"FUNC",          SemanticTypes::DEFINITION_FUNCTION, 0xFF},
    {"FN",            SemanticTypes::DEFINITION_FUNCTION, 0xFF},
    {"METHOD",        SemanticTypes::DEFINITION_FUNCTION, 0xFF},
    {"CALL",          SemanticTypes::COMPUTATION_CALL, 0xFF},
    {"INVOKE",        SemanticTypes::COMPUTATION_CALL, 0xFF},
    {"CLASS",         SemanticTypes::DEFINITION_CLASS, 0xFF},
    {"CLS",           SemanticTypes::DEFINITION_CLASS, 0xFF},
    {"STRUCT",        SemanticTypes::DEFINITION_CLASS, 0xFF},
    {"TRAIT",         SemanticTypes::DEFINITION_CLASS, 0xFF},
    {"INTERFACE",     SemanticTypes::DEFINITION_CLASS, 0xFF},
    {"IDENTIFIER",    SemanticTypes::NAME_IDENTIFIER, 0xFF},
    {"ID",            SemanticTypes::NAME_IDENTIFIER, 0xFF},
    {"IDENT",         SemanticTypes::NAME_IDENTIFIER, 0xFF},
    {"MODULE",        SemanticTypes::DEFINITION_MODULE, 0xFF},
    {"MOD",           SemanticTypes::DEFINITION_MODULE, 0xFF},
    {"PACKAGE",       SemanticTypes::DEFINITION_MODULE, 0xFF},
    {"NAMESPACE",     SemanticTypes::DEFINITION_MODULE, 0xFF},
    {"NS",            SemanticTypes::DEFINITION_MODULE, 0xFF},
    {"VARIABLE",      SemanticTypes::DEFINITION_VARIABLE, 0xFF},
    {"VAR",           SemanticTypes::DEFINITION_VARIABLE, 0xFF},
    {"LET",           SemanticTypes::DEFINITION_VARIABLE, 0xFF},
    {"CONST",         SemanticTypes::DEFINITION_VARIABLE, 0xFF},
    {"CONDITIONAL",   SemanticTypes::FLOW_CONDITIONAL, 0xFF},
    {"COND",          SemanticTypes::FLOW_CONDITIONAL, 0xFF},
    {"IF",            SemanticTypes::FLOW_CONDITIONAL, 0xFF},
    {"LOOP",          SemanticTypes::FLOW_LOOP, 0xFF},
    {"FOR",           SemanticTypes::FLOW_LOOP, 0xFF},
    {"WHILE",         SemanticTypes::FLOW_LOOP, 0xFF},
    {"JUMP",          SemanticTypes::FLOW_JUMP, 0xFF},
    {"RETURN",        SemanticTypes::FLOW_JUMP, 0xFF},
    {"BREAK",         SemanticTypes::FLOW_JUMP, 0xFF},
    {"CONTINUE",      SemanticTypes::FLOW_JUMP, 0xFF},
    {"YIELD",         SemanticTypes::FLOW_JUMP, 0xFF},
    {"DEFINITION",    SemanticTypes::DEFINITION, 0xF0},
    {"DEF",           SemanticTypes::DEFINITION, 0xF0},
    {"LITERAL",       SemanticTypes::LITERAL, 0xF0},
    {"LIT",           SemanticTypes::LITERAL, 0xF0},
    {"VALUE",         SemanticTypes::LITERAL, 0xF0},
    {"NAME",          SemanticTypes::NAME, 0xF0},
    {"FLOW",          SemanticTypes::FLOW_CONTROL, 0xF0},
    {"CONTROL",       SemanticTypes::FLOW_CONTROL, 0xF0},
    {"EXTERNAL",      SemanticTypes::EXTERNAL, 0xF0},
    {"EXT",           SemanticTypes::EXTERNAL, 0xF0},
    {"MEMBER",        SemanticTypes::COMPUTATION_ACCESS, 0xFF},
    {"ATTR",          SemanticTypes::COMPUTATION_ACCESS, 0xFF},
    {"FIELD",         SemanticTypes::COMPUTATION_ACCESS, 0xFF},
    {"PROP",          SemanticTypes::COMPUTATION_ACCESS, 0xFF},
    {"IMPORT",        SemanticTypes::EXTERNAL_IMPORT, 0xFF},
    {"REQUIRE",       SemanticTypes::EXTERNAL_IMPORT, 0xFF},
    {"USE",           SemanticTypes::EXTERNAL_IMPORT, 0xFF},
    {"EXPORT",        SemanticTypes::EXTERNAL_EXPORT, 0xFF},
    {"PUB",           SemanticTypes::EXTERNAL_EXPORT, 0xFF},
    {"TRY",           SemanticTypes::ERROR_TRY, 0xFF},
    {"CATCH",         SemanticTypes::ERROR_CATCH, 0xFF},
    {"EXCEPT",        SemanticTypes::ERROR_CATCH, 0xFF},
    {"RESCUE",        SemanticTypes::ERROR_CATCH, 0xFF},
    {"THROW",         SemanticTypes::ERROR_THROW, 0xFF},
    {"RAISE",         SemanticTypes::ERROR_THROW, 0xFF},
    {"FINALLY",       SemanticTypes::ERROR_FINALLY, 0xFF},
    {"ENSURE",        SemanticTypes::ERROR_FINALLY, 0xFF},
    {"DEFER",         SemanticTypes::ERROR_FINALLY, 0xFF},
    {"STR",           SemanticTypes::LITERAL_STRING, 0xFF},
    {"STRING",        SemanticTypes::LITERAL_STRING, 0xFF},
    {"NUM",           SemanticTypes::LITERAL_NUMBER, 0xFF},
    {"NUMBER",        SemanticTypes::LITERAL_NUMBER, 0xFF},
    {"BOOL",          SemanticTypes::LITERAL_ATOMIC, 0xFF},
    {"BOOLEAN",       SemanticTypes::LITERAL_ATOMIC, 0xFF},
    {"COLL",          SemanticTypes::LITERAL_STRUCTURED, 0xFF},
    {"LIST",          SemanticTypes::LITERAL_STRUCTURED, 0xFF},
    {"DICT",          SemanticTypes::LITERAL_STRUCTURED, 0xFF},
    {"ARRAY",         SemanticTypes::LITERAL_STRUCTURED, 0xFF},
    {"MAP",           SemanticTypes::LITERAL_STRUCTURED, 0xFF},
    {"SET",           SemanticTypes::LITERAL_STRUCTURED, 0xFF},
    {"TUPLE",         SemanticTypes::LITERAL_STRUCTURED, 0xFF},
    {"QUALIFIED",     SemanticTypes::NAME_QUALIFIED, 0xFF},
    {"DOTTED",        SemanticTypes::NAME_QUALIFIED, 0xFF},
    {"SELF",          SemanticTypes::NAME_SCOPED, 0xFF},
    {"THIS",          SemanticTypes::NAME_SCOPED, 0xFF},
    {"LABEL",         SemanticTypes::NAME_ATTRIBUTE, 0xFF},
    {"ARITH",         SemanticTypes::OPERATOR_ARITHMETIC, 0xFF},
    {"MATH",          SemanticTypes::OPERATOR_ARITHMETIC, 0xFF},
    {"CMP",           SemanticTypes::OPERATOR_COMPARISON, 0xFF},
    {"COMPARISON",    SemanticTypes::OPERATOR_COMPARISON, 0xFF},
    {"LOGIC",         SemanticTypes::OPERATOR_LOGICAL, 0xFF},
    {"LOGICAL",       SemanticTypes::OPERATOR_LOGICAL, 0xFF},
    {"COMP",          SemanticTypes::TRANSFORM_QUERY, 0xFF},
    {"COMPREHENSION",  SemanticTypes::TRANSFORM_QUERY, 0xFF},
    {"COMPUTATION",   SemanticTypes::COMPUTATION, 0xC0},
    {"ERROR",         SemanticTypes::ERROR_HANDLING, 0xF0},
    {"ERR",           SemanticTypes::ERROR_HANDLING, 0xF0},
    {"OPERATOR",      SemanticTypes::OPERATOR, 0xF0},
    {"OP",            SemanticTypes::OPERATOR, 0xF0},
    {"TYPEDEF",       SemanticTypes::TYPE, 0xF0},
    {"TYPE",          SemanticTypes::TYPE, 0xF0},
    {"PATTERN",       SemanticTypes::PATTERN, 0xF0},
    {"PAT",           SemanticTypes::PATTERN, 0xF0},
    {"BLOCK",         SemanticTypes::ORGANIZATION, 0xF0},
    {"STATEMENT",     SemanticTypes::EXECUTION, 0xF0},
    {"STMT",          SemanticTypes::EXECUTION, 0xF0},
    {"SYNTAX",        SemanticTypes::PARSER_SPECIFIC, 0xF0},
    {"SYN",           SemanticTypes::PARSER_SPECIFIC, 0xF0},
    {"TRANSFORM",     SemanticTypes::TRANSFORM, 0xF0},
    {"XFORM",         SemanticTypes::TRANSFORM, 0xF0},
    {"COMMENT",       SemanticTypes::METADATA_COMMENT, 0xFF},
    {"METADATA",      SemanticTypes::METADATA, 0xF0},
    {"META",          SemanticTypes::METADATA, 0xF0},
    {"ACCESS",        SemanticTypes::COMPUTATION_NODE, 0xF0},
};

static constexpr idx_t SEMANTIC_ALIAS_COUNT = sizeof(SEMANTIC_ALIASES) / sizeof(SEMANTIC_ALIASES[0]);

//! Resolve a selector class against the table. Sets `known` so callers can tell a
//! mistyped class from a valid one that matched nothing -- the distinction #188 is about.
inline bool MatchSemanticAlias(uint8_t base_type, const string &pattern, bool &known) {
	for (idx_t i = 0; i < SEMANTIC_ALIAS_COUNT; i++) {
		if (pattern == SEMANTIC_ALIASES[i].alias) {
			known = true;
			return (base_type & SEMANTIC_ALIASES[i].mask) == SEMANTIC_ALIASES[i].code;
		}
	}
	// Not an alias: fall back to the full semantic type name, as the cascade did.
	const auto full_name = SemanticTypes::GetSemanticTypeName(base_type);
	known = false; // the caller decides via GetSemanticTypeCode whether the name is valid
	return full_name == pattern;
}

} // namespace duckdb
