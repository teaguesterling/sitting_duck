#pragma once
//===----------------------------------------------------------------------===//
// duckdb_parser_compat.hpp — shims for the DuckDB PARSER objects, shared
//===----------------------------------------------------------------------===//
//
// See docs/development/duckdb-version-compatibility.md for the two-line
// situation, the four-rung rule for choosing a mechanism, and the break-family
// index. Everything here is rung 3: a SFINAE probe plus tag dispatch, because
// in each case BOTH lines express the same idea with a different spelling —
// which is exactly what a probe absorbs — and nothing here names a TYPE that is
// absent on either line (that would force an `#if`).
//
// WHY A SEPARATE HEADER FROM duckdb_compat.hpp. That header is included by
// fourteen translation units, and the parser-object shims need parser headers.
// The heavyweight ones (ten `parser/parsed_data` includes, for CreateInfo and
// friends) stay local to src/language_adapters/duckdb_adapter.cpp, which is
// their only consumer. What lives HERE is the subset with more than one
// consumer, so that there is exactly one copy of each probe: a second copy of a
// compat probe is how these drift, and they drift silently, because the partial
// backports mean "is this v2.0?" is a question per accessor rather than once.
//
// Consumers: src/language_adapters/duckdb_adapter.cpp, src/ast_copy_function.cpp.

#include "duckdb/common/helper.hpp"
#include "duckdb/common/string.hpp"
#include "duckdb/common/unique_ptr.hpp"
#include "duckdb/parser/common_table_expression_info.hpp"
#include "duckdb/parser/parser_options.hpp"
#include "duckdb/parser/query_node.hpp"
#include "duckdb/parser/statement/select_statement.hpp"

#include <type_traits>
#include <utility>

namespace duckdb {

//! Family A — the string-vs-Identifier spelling.
//!
//! v2.0 turned a long list of plain `string` fields and getters into
//! `Identifier` (case-insensitive comparison, explicit conversion back to
//! string). v1.5.6 backported *some* of them, so the shape differs per accessor
//! rather than per line. Exact-match overload resolution prefers the
//! non-template for a `string`, so each call site compiles against whichever
//! shape the pinned DuckDB hands it, with no probe and no branch:
//!
//!     IdentString(bound.names[i])     // vector<string> on v1.5, vector<Identifier> on v2.0
//!     IdentString(option.first)       // case_insensitive_map_t vs identifier_map_t key
//!
//! `Identifier`'s conversion to `const string &` is explicit *by design*
//! upstream ("it discards the case-insensitive semantics, so callers must opt
//! in"), which is why passing one to anything taking `const string &` is a hard
//! error rather than a silent conversion. This is the opt-in.
inline string IdentString(const string &name) {
	return name;
}
template <class IDENTIFIER>
string IdentString(const IDENTIFIER &identifier) {
	return identifier.GetIdentifierName();
}

//! Family F — constructing a Parser with no ClientContext.
//!
//! v1.5 declares `explicit Parser(ParserOptions options = ParserOptions())`, so
//! a bare `Parser parser;` works. v2.0 made ParserOptions' default constructor
//! PRIVATE (friending only ClientContext) and added ParserOptions::Builtin() as
//! the explicit "parse without a context" configuration, leaving Parser with no
//! zero-argument constructor at all.
//!
//! This is not a cosmetic rename. Builtin() installs
//! CompiledGrammar::DefaultGrammar(), and v2.0's Parser::GetGrammar() throws
//! InternalException("ParserOptions requires a compiled grammar") when the
//! options carry none. Had upstream left the default constructor public we would
//! have built cleanly and failed at PARSE time; the private constructor is what
//! turns that into a build error instead.
//!
//! A probe rather than `#if`, because ParserOptions exists on both lines and
//! only the spelling of "the default options" differs.
//!
//! NOTE for callers: `Parser parser(DefaultParserOptions());` is the most vexing
//! parse — it declares a function. Bind the options to a named local first, or
//! use `make_uniq<Parser>(DefaultParserOptions())`.
template <class T, class = void>
struct HasBuiltinParserOptions : std::false_type {};
template <class T>
struct HasBuiltinParserOptions<T, decltype(void(T::Builtin()))> : std::true_type {};

typedef HasBuiltinParserOptions<ParserOptions> BuiltinParserOptionsTag;

// Both overloads are templates on purpose: a non-template is type-checked
// whether it is called or not, and each body names something the other line
// lacks (Builtin() on v1.5, a public default ctor on v2.0).
template <class OPTIONS>
OPTIONS DefaultParserOptionsImpl(std::true_type) {
	return OPTIONS::Builtin();
}
template <class OPTIONS>
OPTIONS DefaultParserOptionsImpl(std::false_type) {
	return OPTIONS();
}

//! The options to construct a Parser with when there is no ClientContext to take
//! them from. Both lines accept `Parser(const ParserOptions &)`, so this is the
//! only part of constructing a context-free parser that differs between them.
inline ParserOptions DefaultParserOptions() {
	return DefaultParserOptionsImpl<ParserOptions>(BuiltinParserOptionsTag());
}

//! Family H (new, 2026-10-09) — where a CTE keeps its query.
//!
//!     v1.5.6   unique_ptr<SelectStatement> query;    // a statement wrapping a node
//!     v2.0     unique_ptr<QueryNode> query_node;     // the node, directly
//!
//! v2.0 deleted `query` outright. It keeps a `CommonTableExpressionInfo(
//! unique_ptr<SelectStatement>, unique_ptr<QueryNode>)` constructor and a
//! `GetQueryForSerialization()` for deserialization compatibility, so a grep for
//! "SelectStatement" in that header still hits — but the MEMBER is gone, and
//! assigning it is a hard error.
//!
//! A probe, not an `#if`: both `SelectStatement` and `QueryNode` exist on both
//! lines (so no absent type is named at template-definition time), and only the
//! spelling of "the CTE's root query" differs. The probe aims at the v2.0-only
//! member, never at the one being replaced — a probe aimed at the old field can
//! be true on both lines.
template <class T, class = void>
struct HasCTEQueryNode : std::false_type {};
template <class T>
struct HasCTEQueryNode<T, decltype(void(std::declval<T &>().query_node))> : std::true_type {};

typedef HasCTEQueryNode<CommonTableExpressionInfo> CTEQueryNodeTag;

// Both overloads are templates, for the same reason as above: each body names a
// member the other line does not have.
template <class CTE_INFO>
void SetCTEQueryImpl(CTE_INFO &cte, unique_ptr<QueryNode> node, std::true_type) {
	cte.query_node = std::move(node);
}
template <class CTE_INFO>
void SetCTEQueryImpl(CTE_INFO &cte, unique_ptr<QueryNode> node, std::false_type) {
	// make_uniq<SelectStatement>() is NON-dependent and therefore checked when
	// this template is defined, not when it is instantiated — so it has to be
	// valid on both lines. It is: v2.0's SelectStatement is still
	// default-constructible with a public `node`, it is just no longer what a
	// CTE holds. (`cte.query` IS dependent, so that half is only checked on the
	// line that has it.)
	cte.query = make_uniq<SelectStatement>();
	cte.query->node = std::move(node);
}

//! Install `node` as the given CTE's query, whichever way this DuckDB spells it.
inline void SetCTEQuery(CommonTableExpressionInfo &cte, unique_ptr<QueryNode> node) {
	SetCTEQueryImpl(cte, std::move(node), CTEQueryNodeTag());
}

} // namespace duckdb
