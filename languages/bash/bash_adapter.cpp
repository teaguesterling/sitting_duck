#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_bash();
}

namespace duckdb {

namespace {

// Does this subtree hold a command substitution? `$(...)` and the backtick form
// both parse as `command_substitution`, and either one may sit inside a string
// or a concatenation in command position: `"$(which ls)" -l`. A substitution in
// command position has no single callee (#91), so the whole name position is
// unnamed when one is present at any depth.
bool HoldsCommandSubstitution(TSNode node) {
	if (ts_node_is_null(node)) {
		return false;
	}
	if (strcmp(ts_node_type(node), "command_substitution") == 0) {
		return true;
	}
	uint32_t child_count = ts_node_child_count(node);
	for (uint32_t i = 0; i < child_count; i++) {
		if (HoldsCommandSubstitution(ts_node_child(node, i))) {
			return true;
		}
	}
	return false;
}

// Strip one matched pair of surrounding quotes. The quote characters are shell
// syntax rather than part of the command name, so `"$cmd"` and `$cmd` give the
// same name and join to each other, and `'ls' -l` is the command `ls`. A
// concatenation that merely BEGINS with a quote (`"$a"b`) keeps its text,
// because its last character is not the closing quote.
string StripOuterQuotes(const string &text) {
	if (text.size() < 2) {
		return text;
	}
	const char first = text.front();
	if ((first == '"' || first == '\'') && text.back() == first) {
		return text.substr(1, text.size() - 2);
	}
	return text;
}

} // namespace

//==============================================================================
// Bash Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> BashAdapter::node_configs = {
#include "../bash/bash_types.def"
};

#undef DEF_TYPE

string BashAdapter::GetLanguageName() const {
	return "bash";
}

vector<string> BashAdapter::GetAliases() const {
	return {"bash", "shell", "sh"};
}

void BashAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_bash();
	parser_wrapper_->SetLanguage(ts_language, "Bash");
}

unique_ptr<TSParserWrapper> BashAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	auto ts_language = tree_sitter_bash();
	fresh_parser->SetLanguage(ts_language, "Bash");
	return fresh_parser;
}

string BashAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string BashAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		if (config->name_strategy == ExtractionStrategy::CUSTOM) {
			string node_type = string(node_type_str);
			if (node_type == "command") {
				// The command name lives in the grammar's REQUIRED `name` field, which holds a
				// `command_name` wrapper -- it is not a direct child of `command`. Argument words
				// ARE direct children, so FIND_IDENTIFIER's `word` pass named the first ARGUMENT:
				// `date +%s` came out as `+%s` (#140). Commit 28c61fd left this UNNAMED rather
				// than wrong, and tracked the correct binding here.
				TSNode name_node = ts_node_child_by_field_name(node, "name", 4);
				if (ts_node_is_null(name_node)) {
					return "";
				}
				// THE NAME IS THE TEXT AS WRITTEN. Teague ruled it twice on
				// 2026-11-18: first that a VARIABLE in command position is named as
				// written (`$verbose`, `${CMD}`), and then, when this code still
				// excluded strings, that "that command's name is the whole string:
				// $prefix/bin/$tool".
				//
				// THIS IS NOT A DIVERGENCE FROM PYTHON AND JAVASCRIPT, though it was
				// argued here as one. This code used to return "" for `$cmd`, citing
				// python `fns[k]()` and javascript `obj[k]()`, which are both unnamed
				// (measured, while `foo()` is named in each). The citation was the
				// mistake. The house rule is that a callee with NO NAME IN THE SOURCE
				// gets an empty name, and `$cmd` has one. Four ways the two cases
				// differ:
				//
				//   1. `$cmd` names ONE variable, written right there. `fns[k]` is a
				//      compound expression -- two variables and a subscript -- and no
				//      identifier in it names the callee. There is nothing to write down.
				//   2. Bash has no first-class functions and no function references, so
				//      expansion in command position is THE way to parameterize a
				//      command. In python `fns[k]()` is one dispatch idiom among many and
				//      the subscript form is not privileged.
				//   3. The written form IS the interface. `$CC`, `$MAKE` and `$PYTHON` in
				//      a build script are the meaningful unit, and more useful to a reader
				//      than whatever they resolved to on one machine. `fns[k]` as a name
				//      would be noise.
				//   4. `$cmd` often resolves: an assignment is frequently visible in the
				//      same file (`cmd=/usr/bin/foo`, `: ${CC:=gcc}`). `fns[k]`
				//      essentially never does.
				//
				// LOOKING FOR ONE IDENTIFIER WAS THE SECOND MISTAKE. This code used to
				// take the `word` child, then a `simple_expansion` or `expansion` child,
				// and leave everything else unnamed -- which read `"$prefix/bin/$tool"`
				// as "no single identifier names the callee, so no name". The whole
				// written form is the name, so `command_name`'s own text answers every
				// one of those shapes: a word, either expansion spelling, a quoted or
				// unquoted compound, and a raw string.
				//
				// "The name as written" is also already the convention for kotlin's
				// parent types and jsx's `svg:circle`.
				//
				// WHAT THE NAME MEANS HERE: it is an INDICATION OF AN UNEVALUATED NAME
				// (Teague, 2026-11-18). The command that runs is the value, which is not
				// knowable at parse time, so the name records the expression that
				// produces it. The leading `$` is what lets a consumer tell the two
				// apart, which is why it is kept rather than stripped with the quotes.
				//
				// THE ANSWER IS THEREFORE IMPERFECT ON PURPOSE. `$'ansi'` keeps its
				// `$'...'` spelling, and `"$a"b` keeps its interior quotes, because only
				// a matched outer pair comes off. These are bashisms, and the place to
				// refine them is the semantic type refinements -- not a larger string
				// rule here, which would trade one rough edge for several.
				//
				// THE ONE EXCEPTION IS A COMMAND SUBSTITUTION (#91): `$(which ls) -l`
				// runs whatever `which ls` printed, so the outer command has no callee
				// to name, and the inner command already carries `which` on its own row.
				// test/sql/languages/bash_variable_named_command.test section 3 is the
				// negative control, bare and inside a string.
				if (HoldsCommandSubstitution(name_node)) {
					return "";
				}
				return StripOuterQuotes(ExtractNodeText(name_node, content));
			}
			if (node_type == "declaration_command") {
				// declaration_command nests the variable name 2 levels deep:
				// declaration_command -> variable_assignment -> variable_name
				TSNode var_assign = FindChildByTypeNode(node, "variable_assignment");
				if (!ts_node_is_null(var_assign)) {
					return FindChildByType(var_assign, content, "variable_name");
				}
				// Fallback: direct variable_name child (e.g., `local var`)
				string result = FindChildByType(node, content, "variable_name");
				if (!result.empty()) {
					return result;
				}
				// Last resort: find first word child that isn't a flag
				// e.g., `declare -a FILES_PROCESSED` has word children: "-a", "FILES_PROCESSED"
				uint32_t child_count = ts_node_child_count(node);
				for (uint32_t i = 0; i < child_count; i++) {
					TSNode child = ts_node_child(node, i);
					if (string(ts_node_type(child)) == "word") {
						string text = ExtractNodeText(child, content);
						if (!text.empty() && text[0] != '-') {
							return text;
						}
					}
				}
				return "";
			}
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	return "";
}

string BashAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool BashAdapter::IsPublicNode(TSNode node, const string &content) const {
	// Bash doesn't have visibility concepts - all nodes are "public"
	return true;
}

const unordered_map<string, NodeConfig> &BashAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction BashAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const BashAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
