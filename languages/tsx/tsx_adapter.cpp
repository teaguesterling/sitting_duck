#include "language_adapter.hpp"

extern "C" {
const TSLanguage *tree_sitter_tsx();
}

namespace duckdb {

// Everything but the grammar is TypeScript's: node configs, names and native extraction.
string TSXAdapter::GetLanguageName() const {
	return "tsx";
}

vector<string> TSXAdapter::GetAliases() const {
	return {"tsx"};
}

void TSXAdapter::InitializeParser() const {
	parser_wrapper_ = make_uniq<TSParserWrapper>();
	parser_wrapper_->SetLanguage(tree_sitter_tsx(), "TSX");
}

unique_ptr<TSParserWrapper> TSXAdapter::CreateFreshParser() const {
	auto fresh_parser = make_uniq<TSParserWrapper>();
	fresh_parser->SetLanguage(tree_sitter_tsx(), "TSX");
	return fresh_parser;
}

} // namespace duckdb
