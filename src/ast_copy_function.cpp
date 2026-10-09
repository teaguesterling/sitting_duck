// ============================================================================
// COPY (<query>) TO '<path>' (FORMAT ast) — the byte-exact unparse sink
// ============================================================================
// tracker/features/045-copy-format-ast-writer.md, issue #174 ("level 5").
// The settled form (Teague, 2026-10-07; docs/planning/v2-architecture.md,
// "write_ast laws"):
//
//     COPY (FROM read_ast(x, source := 'full')) TO 'x2' (FORMAT ast, LANGUAGE 'c')
//
// with LANGUAGE optional — inferred from the `language` column read_ast /
// parse_ast already emit, and an override / disambiguator when given.
//
// BYTE-EXACT OR REFUSE. There is deliberately NO normalising mode. 047's whole
// argument for sequencing this last is that "writing lossy output to disk is
// worse than returning it in a result set, because a file looks authoritative".
// A file that silently holds normalised text where the caller expected the
// original bytes is the trap; the only way to make that impossible — rather
// than merely loud — is to refuse. So FORMAT ast writes a byte-for-byte
// reproduction of the parsed file or it writes nothing at all, and the
// rules-based (whitespace-normalising) unparser is reachable only by naming it:
//
//     COPY (SELECT encode(source) FROM ast_unparse('x.py')) TO 'out.py' (FORMAT blob)
//
// which the refusal messages for PRESET / INDENT / LINE_ENDINGS /
// STRIP_COMMENTS point at. Those options are listed in 045's original spec;
// they belong to the normalising unparser and are rejected here rather than
// quietly honoured.
//
// HOW: ONE SPLICE, NOT TWO. The splice, its nine validation guards and its
// path-matching rules already exist, in SQL, as `ast_unparse_exact_splice`
// (src/sql_macros/ast_unparse.sql). Reimplementing them in C++ would put a
// second copy of the byte-exact law behind a file that looks authoritative —
// the two would drift and the file would still look right. So this copy
// function implements no splice. It uses DuckDB's `copy_to_plan` hook
// (CopyFunction::plan, consulted by Binder::BindCopyTo before anything else)
// to REWRITE the statement:
//
//     COPY (<query>) TO '<dest>' (FORMAT ast [, LANGUAGE ...] [, FILE_PATH ...])
//
// becomes, structurally — the user's query node is MOVED into the CTE, never
// re-serialised through ToString() —
//
//     COPY (
//       WITH __sd_ast_copy_nodes AS (<query>),
//            __sd_ast_copy_bytes AS (
//              SELECT * FROM (SELECT fp, ast_source_bytes(fp) AS cblob
//                             FROM (SELECT DISTINCT file_path AS fp FROM __sd_ast_copy_nodes))
//              WHERE cblob IS NOT NULL),
//            __sd_ast_copy_out AS MATERIALIZED (
//              SELECT * FROM ast_unparse_exact_splice('__sd_ast_copy_nodes', '__sd_ast_copy_bytes', ...)),
//            __sd_ast_copy_guard AS (SELECT CASE ... error(...) ... END AS ok FROM __sd_ast_copy_out)
//       SELECT encode(o.source) FROM __sd_ast_copy_guard g LEFT JOIN __sd_ast_copy_out o ON (g.ok)
//     ) TO '<dest>' (FORMAT blob, USE_TMP_FILE true)
//
// and re-binds it. `FORMAT blob` is DuckDB's own byte-verbatim writer
// (src/function/copy_blob.cpp): it writes a BLOB column's bytes and nothing
// else — no header, no quoting, no row terminator — so there is no writer code
// here either, and no trailing newline to accidentally append. Re-binding
// cannot recurse: the rewritten statement's format is "blob", which has no
// `plan`. (Binder::BindCopyFrom already re-enters Bind() on the same binder for
// a different statement, so the shape is not novel.)
//
// WHY A CTE AND NOT A SUBQUERY. `ast_unparse_exact_splice` takes the node
// relation BY NAME and resolves it with query_table(), because a table macro
// cannot be handed a relation. A CTE on the rewritten COPY's own select node is
// what gives that name something to resolve to — the same shape
// `ast_unparse_exact(path)` uses for its own `__sdx_src`.
//
// AS MATERIALIZED is load-bearing: __sd_ast_copy_out is referenced twice (once
// by the guard, once by the projection) and DuckDB inlines CTEs by default, so
// without it the whole splice would run twice.
//
// THE GUARD IS A LEFT JOIN, NOT A WHERE, for the empty case. A refusal has to
// fire when the query produced NO rows, and a `WHERE (SELECT ...)` over an
// empty relation may never evaluate its subquery. Joining FROM the single-row
// guard makes the aggregate's evaluation structural: it is the left side.
//
// THE JOIN-KEY QUESTION, ANSWERED. The PATH MATCHING block in ast_unparse.sql
// exists because three relations had to agree on one spelling of a path (the
// node table's `file_path`, the blob relation's `fp`, and a `file_path :=`
// argument) and DuckDB's globber returns './'-prefixed and, on Windows,
// backslash-separated paths while an exact path passes through verbatim. That
// hazard is STRUCTURALLY ABSENT here: the blob relation's `fp` is derived from
// the node table's own `file_path` column in the same statement, so the two
// sides of the join are the same string by construction and cannot disagree.
// The macro's normalisation still runs and is harmless (it maps both sides
// identically). Only an explicit `FILE_PATH '<path>'` override re-introduces a
// second spelling, and the macro already normalises that.
//
// WHAT IS NOT NEEDED: 4b part 2 (per-node templates for synthesized nodes).
// Nothing here is generated — every byte written comes from the original file,
// attributed to a leaf or to a gap — so there is no synthesized node to lay
// out. Templates become necessary when the sink must write a tree that no file
// can supply bytes for; that is a different feature, not half of this one.
// ============================================================================

#include "duckdb.hpp"
#include "duckdb/common/file_system.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/copy_function.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/parser/query_node.hpp"
#include "duckdb/parser/statement/copy_statement.hpp"
#include "duckdb/parser/statement/select_statement.hpp"
#include "duckdb/planner/binder.hpp"

namespace duckdb {

namespace {

//! The node columns ast_unparse_exact_splice reads. Checked up front so a query
//! that is not read_ast(..., source := 'full') output is named precisely rather
//! than surfacing as a bare "Referenced column not found".
const char *const REQUIRED_NODE_COLUMNS[] = {"file_path",        "language",   "node_id",  "depth",
                                             "descendant_count", "start_byte", "end_byte", nullptr};

//! A SQL string literal, properly escaped (Value::ToSQLString doubles quotes).
string SqlLiteral(const string &text) {
	return Value(text).ToSQLString();
}

//! Read an option the sink owns. COPY options arrive as a vector<Value> because
//! an option may be a list; ours take exactly one string.
string SingleStringOption(const string &name, const vector<Value> &values) {
	if (values.size() != 1 || values[0].IsNull()) {
		throw BinderException("COPY ... TO ... (FORMAT ast): option %s expects exactly one string value, e.g. %s '%s'",
		                      StringUtil::Upper(name), StringUtil::Upper(name),
		                      name == "language" ? "python" : "src/main.py");
	}
	return values[0].ToString();
}

//! Reject an option rather than silently ignoring it, with a message that says
//! what FORMAT ast is for the options 045's original spec advertised.
void RejectOption(const string &option) {
	auto lower = StringUtil::Lower(option);
	if (lower == "preset" || lower == "indent" || lower == "line_endings" || lower == "strip_comments") {
		throw BinderException(
		    "COPY ... TO ... (FORMAT ast): option %s is not supported, and is not an oversight. FORMAT ast writes a "
		    "BYTE-EXACT reproduction of the parsed file: every byte comes from the original, so there is no layout "
		    "left to choose. %s belongs to the rules-based, whitespace-NORMALISING unparser, whose output is not the "
		    "file it came from. If that is what you want, say so explicitly:\n"
		    "  COPY (SELECT encode(source) FROM ast_unparse('x.py', preset := 'black')) TO 'out.py' (FORMAT blob)",
		    StringUtil::Upper(option), StringUtil::Upper(option));
	}
	if (lower == "partition_by" || lower == "per_thread_output" || lower == "file_size_bytes" ||
	    lower == "filename_pattern" || lower == "file_extension") {
		throw BinderException(
		    "COPY ... TO ... (FORMAT ast): option %s is not supported. FORMAT ast writes exactly ONE file from "
		    "exactly ONE parse — a node table carrying more than one file_path has no single textual answer and is "
		    "refused (#89: never guess which one was meant). Write one COPY statement per file.",
		    StringUtil::Upper(option));
	}
	if (lower == "use_tmp_file") {
		throw BinderException(
		    "COPY ... TO ... (FORMAT ast): option USE_TMP_FILE is not supported because FORMAT ast decides it. Every "
		    "validation guard fires after the destination would have been opened, so a file destination is always "
		    "written through a temporary file — that is what keeps a refusal from leaving a truncated or empty file "
		    "where source code was asked for. '/dev/stdout' is the one exception: it is a stream with nothing to "
		    "truncate, so no temporary file is used there.");
	}
	throw BinderException("COPY ... TO ... (FORMAT ast): unrecognized option %s. Supported options are LANGUAGE "
	                      "('<language>', an override for the inferred `language` column) and FILE_PATH ('<path>', a "
	                      "disambiguator for a multi-file node table).",
	                      StringUtil::Upper(option));
}

//! Bind a throwaway copy of the user's query just to learn its column names, so
//! a missing node column can be reported as such. Any failure here is NOT
//! reported: it is the user's own query failing to bind, and the real bind
//! below will report it authentically.
void CheckNodeColumns(Binder &binder, const QueryNode &user_query) {
	vector<string> names;
	try {
		auto node_copy = user_query.Copy();
		auto inspector = Binder::CreateBinder(binder.context, binder);
		names = inspector->Bind(*node_copy).names;
	} catch (...) {
		return;
	}
	case_insensitive_set_t present;
	for (auto &name : names) {
		present.insert(name);
	}
	vector<string> missing;
	for (idx_t i = 0; REQUIRED_NODE_COLUMNS[i]; i++) {
		if (present.find(REQUIRED_NODE_COLUMNS[i]) == present.end()) {
			missing.push_back(REQUIRED_NODE_COLUMNS[i]);
		}
	}
	if (missing.empty()) {
		return;
	}
	throw BinderException(
	    "COPY ... TO ... (FORMAT ast): the query is missing the column(s) %s, which a byte-exact write needs. "
	    "FORMAT ast splices the original bytes at [start_byte, end_byte) over the whole leaf frontier, so it needs "
	    "UNFILTERED read_ast(<path>, source := 'full') output — byte offsets exist only at that retention level. "
	    "Got: %s.",
	    StringUtil::Join(missing, ", "), names.empty() ? "no columns" : StringUtil::Join(names, ", "));
}

//! Build the rewritten COPY statement's select node: the splice, its guards and
//! the byte projection, with a placeholder CTE for the user's query.
//!
//! `override_note` describes the LANGUAGE / FILE_PATH arguments in prose, so
//! that "nothing matched" can say what did the filtering. A query whose rows
//! all fall to an override is a very different mistake from a query with no
//! rows, and one message for both would misdiagnose whichever it was.
string BuildRewriteSQL(const string &language_arg, const string &file_path_arg, const string &override_note) {
	// Messages the generated SQL raises. Built here so they are escaped once.
	auto no_rows = SqlLiteral(
	    "COPY ... TO ... (FORMAT ast): the query produced no rows, so there is no byte-exact answer to write and "
	    "nothing was written. A file with no content is not a faithful reproduction of anything — pass unfiltered "
	    "read_ast(<path>, source := 'full') output. Note that a genuinely EMPTY source file is a different case: it "
	    "parses to one root node and is reproduced as a 0-byte file.");
	auto filtered_out = SqlLiteral(
	    "COPY ... TO ... (FORMAT ast): the query produced rows, but none of them survived to become a textual "
	    "answer, so nothing was written. " +
	    override_note);
	auto many_rows = SqlLiteral(
	    "COPY ... TO ... (FORMAT ast): the writer produced more than one textual answer, so there is no single file "
	    "to write. Scope the query to one file, or pass FILE_PATH '<path>'.");
	auto null_source = SqlLiteral("COPY ... TO ... (FORMAT ast): the writer produced a NULL source, which cannot be "
	                              "a faithful reproduction of a file. Nothing was written.");

	return StringUtil::Format(
	    "COPY ("
	    // MATERIALIZED: the user's query is referenced three times below (the
	    // byte relation, the splice, and the row-count guard) and DuckDB inlines
	    // CTEs by default, so without this a COPY would parse its input file
	    // once per reference.
	    "  WITH __sd_ast_copy_nodes AS MATERIALIZED (SELECT NULL AS __sd_ast_copy_placeholder),"
	    "       __sd_ast_copy_bytes AS ("
	    "         SELECT * FROM ("
	    "           SELECT fp, ast_source_bytes(fp) AS cblob"
	    "           FROM (SELECT DISTINCT file_path AS fp FROM __sd_ast_copy_nodes)"
	    "         ) WHERE cblob IS NOT NULL"
	    "       ),"
	    "       __sd_ast_copy_out AS MATERIALIZED ("
	    "         SELECT * FROM ast_unparse_exact_splice('__sd_ast_copy_nodes', '__sd_ast_copy_bytes',"
	    "                                                language := %s, file_path := %s)"
	    "       ),"
	    // Every probe is an aggregate, evaluated in the FROM rather than inside
	    // the CASE: a scalar subquery in a CASE arm is materialized whether or
	    // not its branch is taken, so a multi-row probe would raise DuckDB's own
	    // "more than one row returned by a subquery" before the branch that
	    // explains the problem could run.
	    "       __sd_ast_copy_guard AS ("
	    "         SELECT CASE WHEN n_out = 0 AND n_in = 0 THEN error(%s)"
	    "                     WHEN n_out = 0 THEN error(%s)"
	    "                     WHEN n_out > 1 THEN error(%s)"
	    "                     WHEN n_null > 0 THEN error(%s)"
	    "                     ELSE true END AS ok"
	    "         FROM (SELECT (SELECT count(*) FROM __sd_ast_copy_nodes) AS n_in,"
	    "                      (SELECT count(*) FROM __sd_ast_copy_out) AS n_out,"
	    "                      (SELECT count(*) - count(source) FROM __sd_ast_copy_out) AS n_null)"
	    "       )"
	    "  SELECT encode(o.source) AS ast_source"
	    "  FROM __sd_ast_copy_guard g LEFT JOIN __sd_ast_copy_out o ON (g.ok)"
	    ") TO 'placeholder' (FORMAT blob)",
	    language_arg, file_path_arg, no_rows, filtered_out, many_rows, null_source);
}

BoundStatement AstCopyPlan(Binder &binder, CopyStatement &stmt) {
	auto &info = *stmt.info;

	// 1. Options. Only the two the law defines are accepted; everything else is
	//    refused with a reason (see RejectOption).
	string language_arg = "NULL";
	string file_path_arg = "NULL";
	vector<string> overrides;
	for (auto &option : info.options) {
		auto lower = StringUtil::Lower(option.first);
		if (lower == "language") {
			auto value = SingleStringOption("language", option.second);
			language_arg = SqlLiteral(value);
			overrides.push_back("LANGUAGE '" + value + "'");
		} else if (lower == "file_path") {
			auto value = SingleStringOption("file_path", option.second);
			file_path_arg = SqlLiteral(value);
			overrides.push_back("FILE_PATH '" + value + "'");
		} else {
			RejectOption(option.first);
		}
	}
	auto override_note =
	    overrides.empty()
	        ? string("No LANGUAGE or FILE_PATH override was given, so this should not be reachable — the splice's own "
	                 "guards cover every other way of producing no answer. Please report it.")
	        : StringUtil::Format("The %s override matched none of them; it is a disambiguator for a table that "
	                             "carries several, not a selector that may come up empty.",
	                             StringUtil::Join(overrides, " and "));

	// A remote destination cannot be protected, so it is refused rather than
	// quietly downgraded. DuckDB's own COPY binder discards use_tmp_file
	// UNCONDITIONALLY for a remote path (bind_copy.cpp: `if (is_remote_file)
	// { use_tmp_file = false; }`), which means the object would be created
	// before any guard runs and a refusal would leave a 0-byte object behind.
	// A 0-byte S3 object looks exactly as authoritative as a 0-byte file, which
	// is the thing this whole feature exists to avoid.
	if (FileSystem::IsRemoteFile(info.file_path)) {
		throw BinderException(
		    "COPY ... TO ... (FORMAT ast): '%s' is a remote destination, which this writer refuses. Every "
		    "byte-exactness guard fires after the destination would have been opened, so FORMAT ast relies on "
		    "writing through a temporary file and renaming — and DuckDB's COPY disables temporary files for remote "
		    "paths unconditionally, so a refusal would leave a 0-byte object where source code was asked for. Write "
		    "locally and upload, or accept that risk explicitly:\n"
		    "  COPY (SELECT encode(source) FROM ast_unparse_exact('<path>')) TO '%s' (FORMAT blob)",
		    info.file_path, info.file_path);
	}

	if (!info.select_statement) {
		// Binder::Bind(CopyStatement &) synthesizes SELECT * FROM <table> before
		// reaching here, so this is defensive only.
		throw BinderException("COPY ... TO ... (FORMAT ast): nothing to write — expected a query or a table.");
	}

	// 2. Name the schema problem before DuckDB names a column problem.
	CheckNodeColumns(binder, *info.select_statement);

	// 3. Parse the rewrite template and move the user's query into its CTE. The
	//    user's query node is never serialised back to SQL text: a ToString()
	//    round trip would be a second, lossy parser of its own.
	Parser parser;
	parser.ParseQuery(BuildRewriteSQL(language_arg, file_path_arg, override_note));
	D_ASSERT(parser.statements.size() == 1);
	auto &rewritten = parser.statements[0]->Cast<CopyStatement>();
	auto &cte_map = rewritten.info->select_statement->cte_map.map;
	auto cte_entry = cte_map.find("__sd_ast_copy_nodes");
	if (cte_entry == cte_map.end()) {
		throw InternalException("COPY (FORMAT ast): rewrite template lost its node CTE");
	}
	cte_entry->second->query = make_uniq<SelectStatement>();
	cte_entry->second->query->node = std::move(info.select_statement);

	// 4. Graft the rewritten select onto the ORIGINAL statement, which the
	//    caller owns and keeps alive for the whole query, and hand it to the
	//    blob writer. `parser` dies at the end of this function; only the
	//    QueryNode moved out of it survives.
	info.select_statement = std::move(rewritten.info->select_statement);
	info.format = "blob";
	info.is_format_auto_detected = false;
	info.options.clear();
	// Write through a temporary file: every guard above fires after the
	// destination would have been opened, and a refusal must not leave a
	// truncated or 0-byte file where source code was asked for. This is also
	// what makes writing back over the file that was parsed safe — the splice
	// has read every byte it needs before the rename happens.
	//
	// '/dev/stdout' is excluded, matching the same special case DuckDB's COPY
	// binder makes: it is a stream, there is nothing to truncate, and the
	// temporary path would be the nonsensical '/dev/tmp_stdout'.
	if (info.file_path != "/dev/stdout") {
		info.options["use_tmp_file"] = {Value::BOOLEAN(true)};
	}

	return binder.Bind(stmt.Cast<SQLStatement>());
}

} // namespace

void RegisterASTCopyFunction(ExtensionLoader &loader) {
	CopyFunction ast_copy("ast");
	// `plan` only: the rewrite below replaces this statement with a FORMAT blob
	// COPY, so there is no bind/sink/finalize of our own. A CopyFunction with no
	// copy_to_bind that reached BindCopyTo's normal path would raise
	// "COPY TO is not supported for FORMAT ast"; it never does, because `plan` is
	// consulted first and always rewrites.
	ast_copy.plan = AstCopyPlan;
	loader.RegisterFunction(ast_copy);
}

} // namespace duckdb
