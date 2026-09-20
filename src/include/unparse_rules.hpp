#pragma once

#include "duckdb.hpp"
#include "duckdb/function/table_function.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace duckdb {

/**
 * @brief Formatting rule types for AST source reconstruction.
 */
enum class UnparseRuleKind : uint8_t {
	TIGHT_BEFORE = 0,         // Drop space before token: (TIGHT_BEFORE, ",", 0)
	TIGHT_AFTER = 1,          // Drop space after token:  (TIGHT_AFTER, "(", 0)
	SPACE_BEFORE = 2,         // Force space before
	SPACE_AFTER = 3,          // Force space after
	KEYWORD_PARENS_SPACE = 4, // Space between keyword & '(': (KEYWORD_PARENS_SPACE, "*", 1)
	INDENT_BLOCK = 5,         // Node type increments indent level: (INDENT_BLOCK, "block", 1)
	INDENT_STRING = 6,        // Indent character sequence: (INDENT_STRING, "*", 0, "    ")
	LINES_BEFORE = 7,         // N blank lines before node: (LINES_BEFORE, "class_definition", 2)
	LINES_AFTER = 8,          // N blank lines after node:  (LINES_AFTER, "import_statement", 1)
	BREAK_BEFORE = 9,         // Hard newline before node
	BREAK_AFTER = 10,         // Hard newline after node
	CUSTOM = 11               // Custom pluggable rule
};

inline const char *UnparseRuleKindToString(UnparseRuleKind kind) {
	switch (kind) {
	case UnparseRuleKind::TIGHT_BEFORE:
		return "TIGHT_BEFORE";
	case UnparseRuleKind::TIGHT_AFTER:
		return "TIGHT_AFTER";
	case UnparseRuleKind::SPACE_BEFORE:
		return "SPACE_BEFORE";
	case UnparseRuleKind::SPACE_AFTER:
		return "SPACE_AFTER";
	case UnparseRuleKind::KEYWORD_PARENS_SPACE:
		return "KEYWORD_PARENS_SPACE";
	case UnparseRuleKind::INDENT_BLOCK:
		return "INDENT_BLOCK";
	case UnparseRuleKind::INDENT_STRING:
		return "INDENT_STRING";
	case UnparseRuleKind::LINES_BEFORE:
		return "LINES_BEFORE";
	case UnparseRuleKind::LINES_AFTER:
		return "LINES_AFTER";
	case UnparseRuleKind::BREAK_BEFORE:
		return "BREAK_BEFORE";
	case UnparseRuleKind::BREAK_AFTER:
		return "BREAK_AFTER";
	case UnparseRuleKind::CUSTOM:
		return "CUSTOM";
	default:
		return "UNKNOWN";
	}
}

inline UnparseRuleKind StringToUnparseRuleKind(const std::string &str) {
	if (str == "TIGHT_BEFORE") return UnparseRuleKind::TIGHT_BEFORE;
	if (str == "TIGHT_AFTER") return UnparseRuleKind::TIGHT_AFTER;
	if (str == "SPACE_BEFORE") return UnparseRuleKind::SPACE_BEFORE;
	if (str == "SPACE_AFTER") return UnparseRuleKind::SPACE_AFTER;
	if (str == "KEYWORD_PARENS_SPACE") return UnparseRuleKind::KEYWORD_PARENS_SPACE;
	if (str == "INDENT_BLOCK") return UnparseRuleKind::INDENT_BLOCK;
	if (str == "INDENT_STRING") return UnparseRuleKind::INDENT_STRING;
	if (str == "LINES_BEFORE") return UnparseRuleKind::LINES_BEFORE;
	if (str == "LINES_AFTER") return UnparseRuleKind::LINES_AFTER;
	if (str == "BREAK_BEFORE") return UnparseRuleKind::BREAK_BEFORE;
	if (str == "BREAK_AFTER") return UnparseRuleKind::BREAK_AFTER;
	return UnparseRuleKind::CUSTOM;
}

/**
 * @brief An unparse rule definition.
 */
struct UnparseRule {
	UnparseRuleKind kind;
	std::string target;       // Node tag, token literal, or selector
	int64_t int_arg = 0;      // Line count, indent delta, etc.
	std::string str_arg = ""; // String argument (e.g. indent chars)

	UnparseRule(UnparseRuleKind k, std::string t, int64_t i = 0, std::string s = "")
	    : kind(k), target(std::move(t)), int_arg(i), str_arg(std::move(s)) {
	}
};

// Convenience macros for .def files
#define DEF_UNPARSE_INDENT(tag, level) \
	DEF_UNPARSE_RULE(UnparseRuleKind::INDENT_BLOCK, tag, level, "")

#define DEF_UNPARSE_LINES_BEFORE(tag, count) \
	DEF_UNPARSE_RULE(UnparseRuleKind::LINES_BEFORE, tag, count, "")

#define DEF_UNPARSE_LINES_AFTER(tag, count) \
	DEF_UNPARSE_RULE(UnparseRuleKind::LINES_AFTER, tag, count, "")

#define DEF_UNPARSE_TIGHT_BEFORE(tok) \
	DEF_UNPARSE_RULE(UnparseRuleKind::TIGHT_BEFORE, tok, 0, "")

#define DEF_UNPARSE_TIGHT_AFTER(tok) \
	DEF_UNPARSE_RULE(UnparseRuleKind::TIGHT_AFTER, tok, 0, "")

#define DEF_UNPARSE_BREAK_BEFORE(tag) \
	DEF_UNPARSE_RULE(UnparseRuleKind::BREAK_BEFORE, tag, 0, "")

#define DEF_UNPARSE_BREAK_AFTER(tag) \
	DEF_UNPARSE_RULE(UnparseRuleKind::BREAK_AFTER, tag, 0, "")

const std::vector<UnparseRule> &GetUniversalUnparseRules();
std::vector<UnparseRule> GetLanguageUnparseRules(const std::string &language);
std::vector<std::pair<std::string, UnparseRule>> GetAllUnparseRules();

void RegisterUnparseRulesFunction(ExtensionLoader &loader);

} // namespace duckdb
