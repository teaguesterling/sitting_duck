#include "ast_parser.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"

// Tree-sitter headers
extern "C" {
#include <tree_sitter/api.h>
}

namespace duckdb {

TSParser* ASTParser::CreateParser(const string &language) {
	const LanguageHandler* handler = GetLanguageHandler(language);
	if (!handler) {
		auto& registry = LanguageHandlerRegistry::GetInstance();
		auto supported = registry.GetSupportedLanguages();
		string supported_str = StringUtil::Join(supported, ", ");
		throw InvalidInputException("Unsupported language: %s. Supported languages: %s", language, supported_str);
	}
	
	return handler->CreateParser();
}

TSTree* ASTParser::ParseString(const string &content, TSParser *parser) {
	TSTree *tree = ts_parser_parse_string(parser, nullptr, content.c_str(), content.length());
	if (!tree) {
		throw InvalidInputException("Failed to parse content");
	}
	return tree;
}

const LanguageHandler* ASTParser::GetLanguageHandler(const string &language) {
	auto& registry = LanguageHandlerRegistry::GetInstance();
	return registry.GetHandler(language);
}

} // namespace duckdb