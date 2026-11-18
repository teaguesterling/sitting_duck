// ============================================================================
// ast_source_bytes(path) -> BLOB — read a file's pristine bytes, per row
// ============================================================================
// The byte-exact unparse law (docs/planning/v2-architecture.md, "write_ast
// laws") is a SPLICE of the original bytes, so every byte-exact writer needs
// those bytes. No extraction configuration retains per-node source text (the
// v2 RFC's "substrate gap"), so they are re-read from the file.
//
// WHY A SCALAR FUNCTION AND NOT read_blob(). DuckDB table functions take only
// constant-foldable arguments: there is no per-row lateral path, so
// `read_blob(file_path)` over a node table is not expressible. That is exactly
// why `ast_unparse_exact_from(ast_table, files, ...)` has to ask the caller for
// a `files` literal covering the file it already names in every row. The COPY
// sink (tracker 045 / #174) cannot ask: it sees a query, not a path, and the
// path lives in the data. This function closes that gap — the blob relation the
// splice needs becomes
//
//     SELECT fp, ast_source_bytes(fp) AS cblob
//     FROM (SELECT DISTINCT file_path AS fp FROM <node table>)
//
// which is derived from the node table's own `file_path` column, so the join
// key cannot disagree with it about spelling.
//
// NULL, NOT AN ERROR, FOR AN UNREADABLE PATH. The callers that matter are the
// splice's validation guards, which already have precise messages for "the file
// is not readable as bytes" and for "this is in-memory parse_ast() output
// (file_path = '<inline>'), which has no file to re-read". Returning NULL lets
// those fire with their instructions intact instead of replacing them with a
// bare IO error. A path that exists but cannot be READ still throws: that is a
// real IO fault, not an absent file, and silently turning it into NULL would
// invite the "file not readable — pass the same path you used for read_ast"
// diagnosis onto a problem that advice does not fix.
//
// ACCESS CONTROL. File access goes through FileSystem::GetFileSystem(context),
// so `enable_external_access`, `allowed_directories` and any registered file
// system (httpfs, secrets) apply exactly as they do to read_blob(). This
// deliberately does not reach for a LocalFileSystem of its own.
// ============================================================================

#include "duckdb.hpp"
#include "duckdb_compat.hpp"
#include "function_doc_helper.hpp"
#include "include/ast_file_utils.hpp"
#include "duckdb/common/file_system.hpp"

namespace duckdb {

static void ASTSourceBytesFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	D_ASSERT(args.ColumnCount() == 1);

	auto &context = state.GetContext();
	auto &fs = FileSystem::GetFileSystem(context);

	CompatUnaryExecuteWithNulls<string_t, string_t>(
	    args.data[0], result, args.size(), [&](string_t path_str, ValidityMask &mask, idx_t idx) -> string_t {
		    if (!mask.RowIsValid(idx)) {
			    return string_t();
		    }
		    auto path = path_str.GetString();
		    // An empty path, a path that does not exist, and the '<inline>'
		    // pseudo-path parse_ast() reports all land here as NULL.
		    if (path.empty() || !fs.FileExists(path)) {
			    mask.SetInvalid(idx);
			    return string_t();
		    }
		    // Exists but unreadable -> the IOException propagates. ReadFileToString
		    // loops on short reads and fails rather than returning a NUL-padded
		    // tail, which for a byte-exact substrate is the only acceptable
		    // behaviour.
		    auto contents = ASTFileUtils::ReadFileToString(fs, path);
		    return StringVector::AddStringOrBlob(result, contents.c_str(), contents.size());
	    });
}

void RegisterASTSourceBytesFunction(ExtensionLoader &loader) {
	ScalarFunction source_bytes("ast_source_bytes", {LogicalType::VARCHAR}, LogicalType::BLOB, ASTSourceBytesFunction);
	RegisterDocumentedScalarFunction(
	    loader, source_bytes,
	    "Read a file's pristine bytes as a BLOB, one row at a time. NULL when the path does not exist (including "
	    "the '<inline>' pseudo-path of parse_ast()). This is the per-row substrate byte-exact unparse needs, which "
	    "read_blob() cannot provide because table functions take only constant arguments.",
	    {"path"},
	    {"SELECT ast_source_bytes('src/main.py')",
	     "SELECT fp, ast_source_bytes(fp) AS cblob FROM (SELECT DISTINCT file_path AS fp FROM nodes)"},
	    {"sitting_duck", "unparse"});
}

} // namespace duckdb
