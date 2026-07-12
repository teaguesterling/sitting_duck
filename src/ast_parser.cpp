#include "ast_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include <limits>

// Tree-sitter headers
extern "C" {
#include <tree_sitter/api.h>
}

namespace duckdb {

TSParser *ASTParser::CreateParser(const string &language) {
	const LanguageHandler *handler = GetLanguageHandler(language);
	if (!handler) {
		auto &registry = LanguageHandlerRegistry::GetInstance();
		auto supported = registry.GetSupportedLanguages();
		string supported_str = StringUtil::Join(supported, ", ");
		throw InvalidInputException("Unsupported language: %s. Supported languages: %s", language, supported_str);
	}

	return handler->GetParser();
}

TSTree *ASTParser::ParseString(const string &content, TSParser *parser) {
	// tree-sitter takes the input length as uint32_t; larger inputs would be
	// silently truncated to a partial parse, so reject them explicitly.
	if (content.length() > std::numeric_limits<uint32_t>::max()) {
		throw InvalidInputException("Cannot parse input of " + std::to_string(content.length()) +
		                            " bytes: exceeds tree-sitter's 4 GiB input limit");
	}
	TSTree *tree = ts_parser_parse_string(parser, nullptr, content.c_str(), static_cast<uint32_t>(content.length()));
	if (!tree) {
		throw InvalidInputException("Failed to parse content");
	}
	return tree;
}

const LanguageHandler *ASTParser::GetLanguageHandler(const string &language) {
	auto &registry = LanguageHandlerRegistry::GetInstance();
	return registry.GetHandler(language);
}

} // namespace duckdb
