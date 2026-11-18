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
				// A literal `word` is a statically knowable command name, and is the
				// common case.
				string word = FindChildByType(name_node, content, "word");
				if (!word.empty()) {
					return word;
				}
				// A VARIABLE in command position is named AS WRITTEN, `$` included:
				// `$verbose && ...` is named `$verbose`, `${CMD} arg` is named `${CMD}`.
				//
				// THIS DIVERGES FROM PYTHON AND JAVASCRIPT ON PURPOSE. Both return "" for
				// a dynamic callee -- measured: python `fns[k]()` and javascript
				// `obj[k]()` are both unnamed, while `foo()` is named in each -- and this
				// code previously returned "" for the same reason, arguing that naming it
				// records the VARIABLE rather than the command. Teague ruled otherwise on
				// 2026-11-18, having been shown that reasoning and that measurement:
				// `$verbose` tells a reader more than "" does, the leading `$` is itself
				// the signal that the callee is dynamic, and "the name as written" is
				// already the convention for kotlin's parent types and jsx's
				// `svg:circle`. The divergence is bash-only and deliberate.
				//
				// `$VAR` is a simple_expansion; `${VAR}` is an expansion. Both are checked
				// because the two spellings mean the same thing to a reader.
				TSNode expansion = FindChildByTypeNode(name_node, "simple_expansion");
				if (ts_node_is_null(expansion)) {
					expansion = FindChildByTypeNode(name_node, "expansion");
				}
				if (!ts_node_is_null(expansion)) {
					return ExtractNodeText(expansion, content);
				}
				// EVERYTHING ELSE STAYS UNNAMED, and the boundary is the point.
				// `$(which ls) -l` is a command_substitution, not a variable, and its
				// inner command already carries the name `which` on its own row -- naming
				// the outer one would duplicate that fact. `"$cmd" arg` is a string, whose
				// contents are not knowable. test/sql/languages/bash_variable_named_command.test
				// section 3 is the negative control for both.
				return "";
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
