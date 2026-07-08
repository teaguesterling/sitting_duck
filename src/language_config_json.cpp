#include "language_config_json.hpp"

#include "semantic_types.hpp"
#include "yyjson.h"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"

#include <memory>

namespace duckdb {

namespace {

// RAII wrapper so yyjson_doc_free runs on every return/throw path.
struct YYJSONDocDeleter {
	void operator()(yyjson_doc *doc) const {
		if (doc) {
			yyjson_doc_free(doc);
		}
	}
};
using YYJSONDocPtr = std::unique_ptr<yyjson_doc, YYJSONDocDeleter>;

ExtractionStrategy ParseNameStrategy(const string &name, const string &node_type) {
	static const unordered_map<string, ExtractionStrategy> name_strategies = {
	    {"NONE", ExtractionStrategy::NONE},
	    {"NODE_TEXT", ExtractionStrategy::NODE_TEXT},
	    {"FIRST_CHILD", ExtractionStrategy::FIRST_CHILD},
	    {"FIND_IDENTIFIER", ExtractionStrategy::FIND_IDENTIFIER},
	    {"FIND_PROPERTY", ExtractionStrategy::FIND_PROPERTY},
	    {"FIND_ASSIGNMENT_TARGET", ExtractionStrategy::FIND_ASSIGNMENT_TARGET},
	    {"FIND_QUALIFIED_IDENTIFIER", ExtractionStrategy::FIND_QUALIFIED_IDENTIFIER},
	    {"FIND_IN_DECLARATOR", ExtractionStrategy::FIND_IN_DECLARATOR},
	    {"FIND_CALL_TARGET", ExtractionStrategy::FIND_CALL_TARGET},
	};
	// Case-insensitive: ast_type_map() displays these names lowercased, and a
	// config author copying from its output should not be rejected.
	const string upper_name = StringUtil::Upper(name);
	if (upper_name == "CUSTOM") {
		throw InvalidInputException(
		    "name_strategy \"CUSTOM\" is not allowed in a runtime language config (node type \"%s\"): "
		    "CUSTOM requires native C++ logic that cannot be loaded at runtime",
		    node_type);
	}
	auto it = name_strategies.find(upper_name);
	if (it == name_strategies.end()) {
		throw InvalidInputException("Unknown name_strategy \"%s\" for node type \"%s\"", name, node_type);
	}
	return it->second;
}

uint8_t ParseFlagName(const string &name, const string &node_type) {
	static const unordered_map<string, uint8_t> flag_names = {
	    {"IS_SYNTAX_ONLY", ASTNodeFlags::IS_SYNTAX_ONLY},
	    {"NAME_REFERENCE", ASTNodeFlags::NAME_REFERENCE},
	    {"NAME_DECLARATION", ASTNodeFlags::NAME_DECLARATION},
	    {"NAME_DEFINITION", ASTNodeFlags::NAME_DEFINITION},
	    {"IS_SCOPE", ASTNodeFlags::IS_SCOPE},
	    {"IS_EXPORTED", ASTNodeFlags::IS_EXPORTED},
	};
	auto it = flag_names.find(name);
	if (it == flag_names.end()) {
		throw InvalidInputException("Unknown flag \"%s\" for node type \"%s\"", name, node_type);
	}
	return it->second;
}

string RequireString(yyjson_val *val, const string &key, const string &node_type) {
	if (!yyjson_is_str(val)) {
		throw InvalidInputException("Field \"%s\" must be a string for node type \"%s\"", key, node_type);
	}
	return string(yyjson_get_str(val), yyjson_get_len(val));
}

NodeConfig ParseNodeEntry(yyjson_val *entry, const string &node_type) {
	if (!yyjson_is_obj(entry)) {
		throw InvalidInputException("Node type \"%s\" must map to an object", node_type);
	}

	bool has_semantic_type = false;
	uint8_t semantic_code = 0;
	uint8_t refinement = 0;
	ExtractionStrategy name_strategy = ExtractionStrategy::NONE;
	uint8_t flags = 0;

	yyjson_obj_iter iter;
	yyjson_obj_iter_init(entry, &iter);
	yyjson_val *key;
	while ((key = yyjson_obj_iter_next(&iter))) {
		const string field(yyjson_get_str(key), yyjson_get_len(key));
		yyjson_val *val = yyjson_obj_iter_get_val(key);

		if (field == "semantic_type") {
			const string type_name = RequireString(val, field, node_type);
			semantic_code = SemanticTypes::GetSemanticTypeCode(type_name);
			if (semantic_code == 255) {
				throw InvalidInputException("Unknown semantic_type \"%s\" for node type \"%s\"", type_name, node_type);
			}
			has_semantic_type = true;
		} else if (field == "refinement") {
			if (!yyjson_is_int(val)) {
				throw InvalidInputException("Field \"refinement\" must be an integer for node type \"%s\"", node_type);
			}
			int64_t value = yyjson_get_sint(val);
			if (value < 0 || value > 3) {
				throw InvalidInputException(
				    "Field \"refinement\" must be between 0 and 3 for node type \"%s\" (got %lld)", node_type,
				    (long long)value);
			}
			refinement = (uint8_t)value;
		} else if (field == "name_strategy") {
			name_strategy = ParseNameStrategy(RequireString(val, field, node_type), node_type);
		} else if (field == "native_strategy") {
			// Rejected rather than silently ignored: accepting the key would let a
			// config author believe native context extraction is active when it
			// never runs for runtime-registered languages.
			throw InvalidInputException(
			    "\"native_strategy\" is not supported in runtime language configs (node type \"%s\"): "
			    "native context extraction requires compiled-in language support",
			    node_type);
		} else if (field == "flags") {
			if (!yyjson_is_arr(val)) {
				throw InvalidInputException("Field \"flags\" must be an array for node type \"%s\"", node_type);
			}
			yyjson_val *flag_val;
			yyjson_arr_iter flag_iter = yyjson_arr_iter_with(val);
			while ((flag_val = yyjson_arr_iter_next(&flag_iter))) {
				if (!yyjson_is_str(flag_val)) {
					throw InvalidInputException("Each entry in \"flags\" must be a string for node type \"%s\"",
					                            node_type);
				}
				flags |= ParseFlagName(string(yyjson_get_str(flag_val), yyjson_get_len(flag_val)), node_type);
			}
		} else {
			throw InvalidInputException("Unknown key \"%s\" in node type \"%s\"", field, node_type);
		}
	}

	if (!has_semantic_type) {
		throw InvalidInputException("Node type \"%s\" is missing the required \"semantic_type\" field", node_type);
	}

	// Native extraction is out of scope for runtime configs (v1): always NONE.
	return NodeConfig((uint8_t)(semantic_code | refinement), name_strategy, NativeExtractionStrategy::NONE, flags);
}

} // namespace

unordered_map<string, NodeConfig> ParseLanguageConfigJSON(const string &json_text, const string &language_name) {
	yyjson_read_err read_err;
	YYJSONDocPtr doc(yyjson_read_opts(const_cast<char *>(json_text.c_str()), json_text.size(), 0, nullptr, &read_err));
	if (!doc) {
		throw InvalidInputException("Failed to parse language config JSON: %s (at byte %llu)", read_err.msg,
		                            (unsigned long long)read_err.pos);
	}

	yyjson_val *root = yyjson_doc_get_root(doc.get());
	if (!yyjson_is_obj(root)) {
		throw InvalidInputException("Language config must be a JSON object at the top level");
	}

	yyjson_val *node_types = nullptr;

	yyjson_obj_iter iter;
	yyjson_obj_iter_init(root, &iter);
	yyjson_val *key;
	while ((key = yyjson_obj_iter_next(&iter))) {
		const string field(yyjson_get_str(key), yyjson_get_len(key));
		yyjson_val *val = yyjson_obj_iter_get_val(key);

		if (field == "language") {
			if (!yyjson_is_str(val)) {
				throw InvalidInputException("Top-level \"language\" field must be a string");
			}
			const string declared(yyjson_get_str(val), yyjson_get_len(val));
			if (declared != language_name) {
				throw InvalidInputException(
				    "Language config declares language \"%s\" but is being registered as \"%s\"", declared,
				    language_name);
			}
		} else if (field == "node_types") {
			if (!yyjson_is_obj(val)) {
				throw InvalidInputException("Top-level \"node_types\" must be an object");
			}
			node_types = val;
		} else {
			throw InvalidInputException("Unknown top-level key \"%s\" in language config", field);
		}
	}

	if (!node_types) {
		throw InvalidInputException("Language config is missing the required \"node_types\" object");
	}

	unordered_map<string, NodeConfig> configs;
	yyjson_obj_iter node_iter;
	yyjson_obj_iter_init(node_types, &node_iter);
	yyjson_val *node_key;
	while ((node_key = yyjson_obj_iter_next(&node_iter))) {
		const string node_type(yyjson_get_str(node_key), yyjson_get_len(node_key));
		yyjson_val *entry = yyjson_obj_iter_get_val(node_key);
		configs[node_type] = ParseNodeEntry(entry, node_type);
	}

	return configs;
}

} // namespace duckdb
