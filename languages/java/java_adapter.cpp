#include "language_adapter.hpp"
#include "unified_ast_backend_impl.hpp"
#include "semantic_types.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/common/helper.hpp"
#include <cstring>

// Tree-sitter language declarations
extern "C" {
const TSLanguage *tree_sitter_java();
}

namespace duckdb {

//==============================================================================
// Java Adapter implementation
//==============================================================================

#define DEF_TYPE(raw_type, semantic_type, name_strat, native_strat, flags)                                             \
	{raw_type, NodeConfig(SemanticTypes::semantic_type, ExtractionStrategy::name_strat,                                \
	                      NativeExtractionStrategy::native_strat, flags)},

const unordered_map<string, NodeConfig> JavaAdapter::node_configs = {
#include "../java/java_types.def"
};

#undef DEF_TYPE

string JavaAdapter::GetLanguageName() const {
	return "java";
}

vector<string> JavaAdapter::GetAliases() const {
	return {"java"};
}

void JavaAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	parser_wrapper_->SetLanguage(tree_sitter_java(), "Java");
}

unique_ptr<TSParserWrapper> JavaAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	fresh_parser->SetLanguage(tree_sitter_java(), "Java");
	return fresh_parser;
}

string JavaAdapter::GetNormalizedType(const string &node_type) const {
	const NodeConfig *config = GetNodeConfig(node_type);
	if (config) {
		return SemanticTypes::GetSemanticTypeName(config->semantic_type);
	}
	return node_type; // Fallback to raw type
}

string JavaAdapter::ExtractNodeName(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		if (config->name_strategy == ExtractionStrategy::CUSTOM) {
			string node_type = string(node_type_str);
			if (node_type == "import_declaration") {
				// #140c. The name of an import is its PATH, not the statement. NODE_TEXT
				// returned `import java.util.List;`, keyword and semicolon included, so a
				// prefix query over a package could not be written at all.
				//
				// The grammar gives this node NO named fields
				// (grammars/tree-sitter-java/src/node-types.json), so there is no field to
				// read the way bash's `command` does in #140a. Its rule is
				//     'import' ('static')? _name ('.' asterisk)? ';'
				// and its only NAMED children are _name, which is an `identifier` or a
				// `scoped_identifier`, and the optional `asterisk`. `import`, `static`,
				// `.` and `;` are anonymous, so scanning the NAMED children is exact.
				string path;
				bool wildcard = false;
				uint32_t named_count = ts_node_named_child_count(node);
				for (uint32_t i = 0; i < named_count; i++) {
					TSNode child = ts_node_named_child(node, i);
					const char *child_type = ts_node_type(child);
					if (strcmp(child_type, "asterisk") == 0) {
						wildcard = true;
					} else if (path.empty() && (strcmp(child_type, "scoped_identifier") == 0 ||
					                            strcmp(child_type, "identifier") == 0)) {
						// Both arms of _name are taken. A single-segment import has a bare
						// `identifier` child and no scoped_identifier at all.
						path = ExtractNodeText(child, content);
					}
				}
				if (path.empty()) {
					// No _name child: nothing statically knowable to bind. An empty name is
					// the house answer, as it is for a dynamic callee in bash and python.
					return "";
				}
				// The star is kept. `java.io` alone cannot be told apart from a single-type
				// import of a class named `io`, and no other column records the difference.
				// `static` is dropped: `name` is a path in every other language, and the
				// keyword stays in the node's own text.
				return wildcard ? path + ".*" : path;
			}
		}
		return ExtractByStrategy(node, content, config->name_strategy);
	}

	// Java-specific fallbacks
	string node_type = string(node_type_str);
	if (node_type.find("declaration") != string::npos) {
		return FindChildByType(node, content, "identifier");
	}

	return "";
}

string JavaAdapter::ExtractNodeValue(TSNode node, const string &content) const {
	const char *node_type_str = ts_node_type(node);
	const NodeConfig *config = GetNodeConfig(node_type_str);

	if (config) {
		// Note: value_strategy is now repurposed as native_strategy for pattern-based extraction
		// For backward compatibility, we'll return empty string since most nodes don't need legacy value extraction
		return "";
	}

	return "";
}

bool JavaAdapter::IsPublicNode(TSNode node, const string &content) const {
	// Check child modifier nodes for access keywords.
	// Default (no modifier) = package-private = not exported.
	uint32_t child_count = ts_node_child_count(node);
	for (uint32_t i = 0; i < child_count; i++) {
		TSNode child = ts_node_child(node, i);
		const char *child_type = ts_node_type(child);
		if (strcmp(child_type, "modifiers") == 0) {
			uint32_t mod_count = ts_node_named_child_count(child);
			for (uint32_t j = 0; j < mod_count; j++) {
				TSNode mod = ts_node_named_child(child, j);
				string mod_text = ExtractNodeText(mod, content);
				if (mod_text == "public") {
					return true;
				}
			}
			return false;
		}
	}
	return false;
}

const unordered_map<string, NodeConfig> &JavaAdapter::GetNodeConfigs() const {
	return node_configs;
}

ParsingFunction JavaAdapter::GetParsingFunction() const {
	// Return a lambda that captures the templated parsing function
	return [](const void *adapter, const string &content, const string &language, const string &file_path,
	          int32_t peek_size, const string &peek_mode) -> ASTResult {
		auto typed_adapter = static_cast<const JavaAdapter *>(adapter);
		return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size,
		                                                    peek_mode);
	};
}

} // namespace duckdb
