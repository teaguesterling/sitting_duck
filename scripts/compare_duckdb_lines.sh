#!/usr/bin/env bash
#
# compare_duckdb_lines.sh -- diff sitting_duck's DuckDB-SQL AST across two
# DuckDB lines (e.g. v1.5.6 "Variegata" vs v2.0 "Cyanoptera").
#
# WHY THIS EXISTS
# ---------------
# CI has a canary that BUILDS sitting_duck against the next DuckDB line with
# `skip_tests: true`. That proves "it compiles" and absolutely nothing about
# behaviour. This script closes that gap for the one adapter most exposed to
# upstream churn: the `duckdb` language, whose nodes come straight out of
# DuckDB's own parser and whose `peek` text comes straight out of DuckDB's own
# deparser (`SQLStatement::ToString()`).
#
# Those two surfaces drift independently, and they are not equally serious:
#
#   * STRUCTURAL drift (type, name, semantic_type, parentage, depth, sibling
#     order, child/descendant counts, line spans) means the tree sitting_duck
#     hands its users changed shape. That breaks queries, macros and tests.
#     Unexplained structural drift fails the run.
#
#   * KNOWN structural drift is upstream behaviour sitting_duck cannot undo,
#     recorded with a reason in the allowlist (default:
#     test/corpus/duckdb_sql/known_drift.tsv). It is reported and does NOT
#     fail, because a tool that is permanently red stops being read -- and the
#     value here is catching NEW drift, which a standing failure would bury.
#     `--strict` ignores the allowlist for a deliberate audit.
#
#   * PEEK drift means DuckDB re-rendered the same tree's source text
#     differently. Observed between 1.5.6 and 2.0: the doubled-semicolon fix,
#     fully parenthesised JOIN trees, cast type names losing their quotes AND
#     keeping the source's casing (1.5.6 normalises `CAST(x AS date)` to
#     `CAST(x AS "DATE")`; 2.0 leaves it as written), implicitly generated
#     function references losing their `main.` qualifier (an explicitly written
#     `main.upper(x)` is unchanged), boolean values rendering as `true` rather
#     than `CAST('t' AS BOOLEAN)`, COPY options renamed and REORDERED, and
#     whitespace normalisation. Least cosmetic of all: 1.5.6 decomposes one
#     `ALTER TABLE ... ADD COLUMN ... DEFAULT x` into three statements
#     (ADD COLUMN ... DEFAULT NULL; UPDATE ...; ALTER COLUMN ... SET DEFAULT)
#     where 2.0 renders the one statement that was written.
#     Peek drift is reported loudly but exits zero. "Not a bug" does NOT mean
#     "not worth reading": several of these change what the rendered SQL means,
#     so read the diff before updating any test that asserts on `peek`.
#
# Everything else in here is a guard against a specific way a previous run of
# this check lied.
#
set -euo pipefail

# ---------------------------------------------------------------- exit codes
# Distinct codes because "the code under test drifted", "the harness is broken"
# and "the allowlist is out of date" demand different responses, and a single
# `exit 1` cannot tell a caller (or a CI log) which one happened.
readonly EXIT_OK=0
readonly EXIT_USAGE=1
readonly EXIT_HARNESS=2          # a guard tripped; the comparison is void
readonly EXIT_STRUCTURAL=3       # unexplained drift in tree structure
readonly EXIT_CONTROL=4          # the diff could not detect a planted change
readonly EXIT_STALE_ALLOWLIST=5  # an allowlist entry no longer matches anything

# ---------------------------------------------------------------- defaults
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)

# Resolved from the script's own location, never from $PWD: this check is run
# from build dirs, CI steps and worktree roots, and a corpus that silently
# resolves to "whatever directory you happened to be in" is a corpus that
# silently compares nothing.
corpus_dir="${repo_root}/test/corpus/duckdb_sql"
allowlist=""                     # empty => <corpus_dir>/known_drift.tsv

baseline_bin="${repo_root}/build/release/duckdb"
candidate_bin=""
baseline_ext=""
candidate_ext=""
language="duckdb"
out_dir=""
keep_out_dir=0
strict=0
# Called the moment something in the dump dir becomes worth reading, so the
# EXIT trap below never deletes evidence the report has already cited.
retain_dumps() { keep_out_dir=1; }

readonly DEFAULT_ALLOWLIST_NAME="known_drift.tsv"
# read_csv() cannot sniff a zero-byte file, and "no allowlist" must not become a
# separate code path through the classifier -- strict mode and the negative
# control would then run through logic the default mode never exercises. So an
# empty allowlist is really a one-row file whose corpus_file can never match a
# real *.sql basename, which keeps exactly one classifier and one set of
# semantics.
readonly ALLOWLIST_SENTINEL_FILE="<no-such-corpus-file>"
write_allowlist_sentinel() {
    printf '%s\t\t\t\t\t\t\t%s\n' "${ALLOWLIST_SENTINEL_FILE}" "unmatchable sentinel row" >"$1"
}

# Structural columns, in a fixed order. `file_path` is deliberately absent:
# the negative control compares a perturbed COPY at a different path, and a
# path column would make every control run trivially (and uselessly) "differ".
# start_line/end_line are included for completeness and for other languages,
# but they carry no signal for `duckdb`: DuckDBAdapter::CreateASTNode hardcodes
# both to 1 (src/language_adapters/duckdb_adapter.cpp), so every row reads 1,1.
# Do not read a clean comparison of those two columns as evidence of anything.
readonly STRUCT_COLUMNS="node_id, type, name, semantic_type, parent_id, depth, sibling_index, children_count, descendant_count, start_line, end_line"
# Same names, as a read_csv() column spec, so the classifier below reads the
# dumps back with DuckDB's own CSV parser instead of splitting on commas --
# `name` can contain commas and quotes (string literals, LIKE patterns, map
# and list literals), which shifts the fields of any hand-rolled split.
readonly STRUCT_CSV_SPEC="{'node_id': 'VARCHAR', 'type': 'VARCHAR', 'name': 'VARCHAR', 'semantic_type': 'VARCHAR', 'parent_id': 'VARCHAR', 'depth': 'VARCHAR', 'sibling_index': 'VARCHAR', 'children_count': 'VARCHAR', 'descendant_count': 'VARCHAR', 'start_line': 'VARCHAR', 'end_line': 'VARCHAR'}"
readonly ALLOWLIST_CSV_SPEC="{'corpus_file': 'VARCHAR', 'from_type': 'VARCHAR', 'from_name': 'VARCHAR', 'from_semantic_type': 'VARCHAR', 'to_type': 'VARCHAR', 'to_name': 'VARCHAR', 'to_semantic_type': 'VARCHAR', 'reason': 'VARCHAR'}"

usage() {
    cat <<EOF
Usage: $(basename "$0") --candidate <duckdb> [options]

Compares the AST that sitting_duck produces for a corpus of DuckDB SQL between
two duckdb binaries, and separates unexplained STRUCTURAL drift (a regression)
from allowlisted known drift and from \`peek\`-only deparser noise.

Required:
  --candidate PATH            duckdb binary for the line under test

Options:
  --baseline PATH             duckdb binary for the known-good line
                              (default: ${baseline_bin})
  --baseline-extension PATH   sitting_duck extension to LOAD into --baseline
  --candidate-extension PATH  sitting_duck extension to LOAD into --candidate
                              (omit either when that build links it statically)
  --corpus DIR                directory of *.sql files to compare
                              (default: ${corpus_dir})
  --allowlist PATH            known-drift file
                              (default: <corpus>/${DEFAULT_ALLOWLIST_NAME}, if present)
  --strict                    ignore the allowlist; fail on ANY structural drift
  --language NAME             read_ast language (default: ${language})
  --out DIR                   keep the raw dumps here instead of a temp dir
  -h, --help                  this text

Dumps, per corpus file and per binary, ordered by node_id as headerless CSV:
  structure: ${STRUCT_COLUMNS}
  peek:      node_id, md5(peek)

Exit codes:
  ${EXIT_OK}  identical, peek-only drift, or structural drift that is all allowlisted
  ${EXIT_USAGE}  usage error
  ${EXIT_HARNESS}  harness guard tripped -- the comparison proved nothing
  ${EXIT_STRUCTURAL}  unexplained structural drift -- a real regression
  ${EXIT_CONTROL}  the negative control failed -- the diff cannot detect change
  ${EXIT_STALE_ALLOWLIST}  an allowlist entry matched nothing -- upstream changed again

Example:
  $(basename "$0") \\
    --baseline  /path/to/v1.5.6/build/release/duckdb \\
    --candidate /path/to/v2.0/build/release/duckdb \\
    --candidate-extension /path/to/v2.0/build/release/extension/sitting_duck/sitting_duck.duckdb_extension
EOF
}

die_usage() {
    printf 'error: %s\n\n' "$1" >&2
    usage >&2
    exit "${EXIT_USAGE}"
}

while (($# > 0)); do
    case "$1" in
        --baseline)             baseline_bin="${2:?--baseline needs a path}"; shift 2 ;;
        --candidate)            candidate_bin="${2:?--candidate needs a path}"; shift 2 ;;
        --baseline-extension)   baseline_ext="${2:?--baseline-extension needs a path}"; shift 2 ;;
        --candidate-extension)  candidate_ext="${2:?--candidate-extension needs a path}"; shift 2 ;;
        --corpus)               corpus_dir="${2:?--corpus needs a path}"; shift 2 ;;
        --allowlist)            allowlist="${2:?--allowlist needs a path}"; shift 2 ;;
        --strict)               strict=1; shift ;;
        --language)             language="${2:?--language needs a name}"; shift 2 ;;
        --out)                  out_dir="${2:?--out needs a path}"; keep_out_dir=1; shift 2 ;;
        -h|--help)              usage; exit "${EXIT_OK}" ;;
        *)                      die_usage "unknown argument: $1" ;;
    esac
done

[[ -n ${candidate_bin} ]] || die_usage "--candidate is required"

# ---------------------------------------------------------------- guards
# A prior session declared "all tests passed" against a duckdb binary that a
# failed build had left stale, and against an extension path that did not
# exist. Every input is therefore checked before any SQL runs, and the checks
# fail the run rather than degrading it.
harness_failed=0
harness_fail() {
    printf '  HARNESS FAILURE: %s\n' "$1" >&2
    harness_failed=1
    retain_dumps
}

# Paths reach DuckDB inside single-quoted SQL string literals, so a quote in
# one would silently change the statement instead of failing loudly.
require_quotable_path() {
    local label="$1" path="$2"
    [[ ${path} == *"'"* ]] && die_usage "${label}: path contains a single quote: ${path}"
    return 0
}

require_executable() {
    local label="$1" path="$2"
    [[ -n ${path} ]] || die_usage "${label}: empty path"
    [[ -e ${path} ]] || die_usage "${label}: no such file: ${path}"
    [[ -x ${path} ]] || die_usage "${label}: not executable: ${path}"
}

require_readable_extension() {
    local label="$1" path="$2"
    [[ -z ${path} ]] && return 0
    [[ -f ${path} ]] || die_usage "${label}: no such extension file: ${path}"
    [[ -r ${path} ]] || die_usage "${label}: extension not readable: ${path}"
    require_quotable_path "${label}" "${path}"
}

require_executable "--baseline" "${baseline_bin}"
require_executable "--candidate" "${candidate_bin}"

# The default --baseline is ${repo_root}/build/release/duckdb, which in a
# worktree checked out to the NEXT DuckDB line is the candidate itself. Running
# `--candidate build/release/duckdb` from there would compare a binary with
# itself and print "identical, control passed, guards clean, exit 0" -- the
# canonical false green, and the exact failure mode the guards exist for.
if [[ $(realpath -- "${baseline_bin}") == "$(realpath -- "${candidate_bin}")" ]]; then
    die_usage "--baseline and --candidate resolve to the same binary ($(realpath -- "${baseline_bin}")); pass --baseline explicitly"
fi
require_readable_extension "--baseline-extension" "${baseline_ext}"
require_readable_extension "--candidate-extension" "${candidate_ext}"

[[ -d ${corpus_dir} ]] || die_usage "--corpus: no such directory: ${corpus_dir}"
require_quotable_path "--corpus" "${corpus_dir}"

# LC_ALL=C so the file order (and therefore the report) is identical on every
# machine and locale. The allowlist lives in the corpus dir, so it is excluded
# by the *.sql filter already -- but say so, because a corpus file that is not
# SQL would collapse to a parse_error node and trip a guard.
mapfile -t corpus_files < <(find "${corpus_dir}" -maxdepth 1 -type f -name '*.sql' -print | LC_ALL=C sort)
((${#corpus_files[@]} > 0)) || die_usage "--corpus: no *.sql files in ${corpus_dir}"

if [[ -z ${allowlist} ]]; then
    # A missing default allowlist is normal (a fresh corpus has no known drift
    # yet); a missing EXPLICIT one is a typo and must not be ignored.
    [[ -f "${corpus_dir}/${DEFAULT_ALLOWLIST_NAME}" ]] && allowlist="${corpus_dir}/${DEFAULT_ALLOWLIST_NAME}"
else
    [[ -f ${allowlist} ]] || die_usage "--allowlist: no such file: ${allowlist}"
fi
[[ -n ${allowlist} ]] && require_quotable_path "--allowlist" "${allowlist}"

if [[ -n ${out_dir} ]]; then
    mkdir -p "${out_dir}"
else
    out_dir=$(mktemp -d -t compare_duckdb_lines.XXXXXX)
fi
require_quotable_path "--out" "${out_dir}"
# The report prints paths to dumps and classifications. Deleting them on the
# way out would leave every such line pointing at a file that no longer
# exists, so the dir is retained the moment anything in it becomes worth
# reading (see `retain_dumps`), and only discarded on a fully clean run.
# shellcheck disable=SC2317  # invoked indirectly, via the EXIT trap below
cleanup() { ((keep_out_dir)) || rm -rf "${out_dir}"; }
trap cleanup EXIT

# The classifier reads the allowlist with read_csv(), which has no portable
# comment syntax across both DuckDB lines, so comments and blank lines are
# stripped here instead. --strict is implemented by handing the classifier an
# EMPTY allowlist rather than by a second code path: one classifier, one set of
# semantics, and strict mode cannot drift away from the default mode.
effective_allowlist="${out_dir}/allowlist.effective.tsv"
write_allowlist_sentinel "${effective_allowlist}"
allowlist_entries=0
if ((!strict)) && [[ -n ${allowlist} ]]; then
    grep -vE '^[[:space:]]*(#|$)' "${allowlist}" >>"${effective_allowlist}" || true
    allowlist_entries=$(($(wc -l <"${effective_allowlist}") - 1))
fi

# ---------------------------------------------------------------- invocation
# DuckDB v2.0 added an "agent mode" that switches itself on when AI_AGENT or
# CLAUDECODE is set and stdout is not a tty -- i.e. exactly how this script is
# run from an agent session. It prints a banner to stderr and renders markdown
# tables and JSON errors, so leaving it on would both corrupt the CSV dumps and
# make the result depend on WHO ran the script.
#
# The fix is to unset the triggers, not to pass `-no-agent`: that flag does not
# exist on v1.5.6, which rejects unknown options outright, so a flag-based fix
# would need per-binary probing to be portable. Unsetting works identically on
# both lines and needs no probe.
#
# DUCKDB_AGENT_MODE is in the list because it is a hard override that turns
# agent mode on even when an output mode IS given (measured: with it set,
# `duckdb -csv` still prints the banner), so scrubbing only the other two is
# not enough.
readonly -a AGENT_MODE_TRIGGERS=(AI_AGENT CLAUDECODE DUCKDB_AGENT_MODE)
duckdb_env=(env)
for trigger in "${AGENT_MODE_TRIGGERS[@]}"; do duckdb_env+=(-u "${trigger}"); done
readonly duckdb_env

# `-unsigned` is only needed to LOAD a locally built extension, and asking for
# it when we are not loading one would needlessly relax the binary's policy.
baseline_flags=()
candidate_flags=()
[[ -n ${baseline_ext} ]] && baseline_flags+=(-unsigned)
[[ -n ${candidate_ext} ]] && candidate_flags+=(-unsigned)

# Runs `query`, stdout to out_file, stderr to err_file, and returns duckdb's
# own exit status. Never pipes duckdb's output anywhere: a pipeline's $? is the
# LAST command's status, which is how a failing duckdb run gets mistaken for a
# successful one.
run_duckdb() {
    local bin="$1" ext="$2" mode="$3" query="$4" out_file="$5" err_file="$6"
    shift 6
    local -a flags=("$@")
    local sql="${query}"
    [[ -n ${ext} ]] && sql="LOAD '${ext}'; ${query}"

    set +e
    "${duckdb_env[@]}" "${bin}" "${flags[@]}" "${mode}" -noheader -c "${sql}" \
        >"${out_file}" 2>"${err_file}"
    local rc=$?
    set -e
    return "${rc}"
}

# Shared post-conditions for any duckdb invocation. `allow_empty` exists
# because a dump of zero rows is a bug while a classification of zero rows just
# means "no differences".
check_duckdb_result() {
    local what="$1" rc="$2" out_file="$3" err_file="$4" allow_empty="$5"

    if ((rc != 0)); then
        harness_fail "${what} exited ${rc}"
        sed 's/^/    | /' "${err_file}" >&2
        return 1
    fi

    # Zero rows is the signature of a dump that ran but measured nothing --
    # and two empty dumps compare equal, which is a silent false pass.
    if ((!allow_empty)) && [[ ! -s ${out_file} ]]; then
        harness_fail "${what} produced 0 rows"
        return 1
    fi

    # duckdb can exit 0 while still having complained (a failed LOAD inside a
    # multi-statement -c, a warning about a deprecated option). Surface it, and
    # treat anything error-shaped as fatal.
    if [[ -s ${err_file} ]]; then
        printf '  stderr from %s:\n' "${what}" >&2
        sed 's/^/    | /' "${err_file}" >&2
        if grep -qiE '(^|[^[:alnum:]])error([^[:alnum:]]|$)' "${err_file}"; then
            harness_fail "${what} wrote an error to stderr"
            return 1
        fi
    fi
    return 0
}

struct_query() { printf "SELECT %s FROM read_ast('%s', '%s') ORDER BY node_id" "${STRUCT_COLUMNS}" "$1" "${language}"; }
peek_query()   { printf "SELECT node_id, coalesce(md5(peek), '<null>') AS peek_md5 FROM read_ast('%s', '%s') ORDER BY node_id" "$1" "${language}"; }

# Dumps one corpus file with one binary and refuses to return a dump it cannot
# vouch for. Each check corresponds to a way an earlier ad-hoc run of this
# comparison reported success it had not earned.
dump() {
    local label="$1" bin="$2" ext="$3" sql_file="$4" kind="$5" out_file="$6"
    local err_file="${out_file}.stderr"
    local query
    case "${kind}" in
        struct) query=$(struct_query "${sql_file}") ;;
        peek)   query=$(peek_query "${sql_file}") ;;
        *)      printf 'internal error: unknown dump kind %s\n' "${kind}" >&2; exit "${EXIT_HARNESS}" ;;
    esac

    local -a flags
    if [[ ${label} == baseline ]]; then flags=("${baseline_flags[@]+"${baseline_flags[@]}"}"); else flags=("${candidate_flags[@]+"${candidate_flags[@]}"}"); fi

    local what
    what="${label} ${kind} dump of $(basename "${sql_file}")"
    local rc=0
    run_duckdb "${bin}" "${ext}" -csv "${query}" "${out_file}" "${err_file}" "${flags[@]+"${flags[@]}"}" || rc=$?
    check_duckdb_result "${what}" "${rc}" "${out_file}" "${err_file}" 0 || return 1

    # read_ast does NOT fail on SQL it cannot parse: it collapses the entire
    # file to one `parse_error` node. Two such dumps are byte-identical, so a
    # corpus file that no longer parses would otherwise read as a clean pass.
    # `type` is CSV field 2 and never needs quoting, so this anchor is exact.
    if [[ ${kind} == struct ]] && grep -qE '^[0-9]+,parse_error,' "${out_file}"; then
        harness_fail "$(basename "${sql_file}") did not parse under ${label} (read_ast returned a parse_error node)"
        return 1
    fi
    return 0
}

# ------------------------------------------------- structural classification
# THE one structural comparison primitive. Both the real check and the negative
# control call this, never a private copy -- a control that exercises a
# different code path than the real check is how a harness defect masquerades
# as five source-level "breaks".
#
# It writes one tab-separated line per finding, tagged UNEXPECTED, KNOWN or
# STALE, and the caller counts tags. The analysis runs in DuckDB rather than in
# awk/cut so that the dumps are read back by the same CSV parser that wrote
# them; `name` can contain commas and quotes, and a hand-rolled split would
# misalign exactly on the rows that matter most.
#
# An allowlist entry matches a changed node only when the baseline triple and
# the candidate triple both match AND nothing outside the triple moved: a node
# that also changed parent, depth or descendant count is a different and
# unexplained event, so it stays UNEXPECTED even if its triple is allowlisted.
classify_struct() {
    local base_csv="$1" cand_csv="$2" corpus_name="$3" allow_file="$4" out_file="$5"
    local err_file="${out_file}.stderr"
    local query
    query=$(cat <<SQL
WITH baseline AS (
    SELECT * FROM read_csv('${base_csv}', header = false, all_varchar = true, columns = ${STRUCT_CSV_SPEC})
), candidate AS (
    SELECT * FROM read_csv('${cand_csv}', header = false, all_varchar = true, columns = ${STRUCT_CSV_SPEC})
), paired AS (
    SELECT
        coalesce(b.node_id, c.node_id) AS node_id,
        b.node_id IS NULL AS added,
        c.node_id IS NULL AS removed,
        b.type AS b_type, b.name AS b_name, b.semantic_type AS b_semantic_type,
        c.type AS c_type, c.name AS c_name, c.semantic_type AS c_semantic_type,
        (b.type IS DISTINCT FROM c.type
         OR b.name IS DISTINCT FROM c.name
         OR b.semantic_type IS DISTINCT FROM c.semantic_type) AS triple_moved,
        (b.parent_id IS DISTINCT FROM c.parent_id
         OR b.depth IS DISTINCT FROM c.depth
         OR b.sibling_index IS DISTINCT FROM c.sibling_index
         OR b.children_count IS DISTINCT FROM c.children_count
         OR b.descendant_count IS DISTINCT FROM c.descendant_count
         OR b.start_line IS DISTINCT FROM c.start_line
         OR b.end_line IS DISTINCT FROM c.end_line) AS other_moved
    FROM baseline AS b FULL OUTER JOIN candidate AS c ON b.node_id = c.node_id
), changed AS (
    SELECT * FROM paired WHERE added OR removed OR triple_moved OR other_moved
), entries AS (
    SELECT * FROM read_csv('${allow_file}', delim = '\t', header = false, all_varchar = true,
                           columns = ${ALLOWLIST_CSV_SPEC})
    WHERE corpus_file = '${corpus_name}'
), matched AS (
    SELECT ch.node_id, e.reason,
           e.from_type, e.from_name, e.from_semantic_type,
           e.to_type, e.to_name, e.to_semantic_type
    FROM changed AS ch
    JOIN entries AS e
      ON NOT ch.added AND NOT ch.removed AND NOT ch.other_moved
     AND ch.b_type IS NOT DISTINCT FROM e.from_type
     AND ch.b_name IS NOT DISTINCT FROM e.from_name
     AND ch.b_semantic_type IS NOT DISTINCT FROM e.from_semantic_type
     AND ch.c_type IS NOT DISTINCT FROM e.to_type
     AND ch.c_name IS NOT DISTINCT FROM e.to_name
     AND ch.c_semantic_type IS NOT DISTINCT FROM e.to_semantic_type
)
SELECT line FROM (
    SELECT
        1 AS tag_order,
        CAST(ch.node_id AS BIGINT) AS sort_key,
        -- chr(9), not '\\t': DuckDB's printf() does not interpret backslash
        -- escapes in its format string, so '\\t' would emit two literal
        -- characters and the tag-counting greps below would never match.
        concat_ws(chr(9),
               CASE WHEN m.node_id IS NULL THEN 'UNEXPECTED' ELSE 'KNOWN' END,
               'node ' || ch.node_id,
               CASE WHEN ch.added THEN '<absent in baseline>'
                    ELSE concat_ws('/', ch.b_type, coalesce(ch.b_name, ''), ch.b_semantic_type) END,
               '->',
               CASE WHEN ch.removed THEN '<absent in candidate>'
                    ELSE concat_ws('/', ch.c_type, coalesce(ch.c_name, ''), ch.c_semantic_type) END,
               coalesce(m.reason, CASE WHEN ch.other_moved THEN 'also moved parent/depth/counts/lines'
                                       ELSE 'not in allowlist' END)) AS line
    FROM changed AS ch
    LEFT JOIN (SELECT node_id, min(reason) AS reason FROM matched GROUP BY node_id) AS m
           ON m.node_id = ch.node_id
    UNION ALL
    SELECT
        2, 0,
        concat_ws(chr(9),
               'STALE', 'entry',
               concat_ws('/', e.from_type, coalesce(e.from_name, ''), e.from_semantic_type),
               '->',
               concat_ws('/', e.to_type, coalesce(e.to_name, ''), e.to_semantic_type),
               e.reason)
    FROM entries AS e
    WHERE NOT EXISTS (
        SELECT 1 FROM matched AS m
        WHERE m.from_type IS NOT DISTINCT FROM e.from_type
          AND m.from_name IS NOT DISTINCT FROM e.from_name
          AND m.from_semantic_type IS NOT DISTINCT FROM e.from_semantic_type
          AND m.to_type IS NOT DISTINCT FROM e.to_type
          AND m.to_name IS NOT DISTINCT FROM e.to_name
          AND m.to_semantic_type IS NOT DISTINCT FROM e.to_semantic_type)
) ORDER BY tag_order, sort_key
SQL
)
    # Run on the baseline binary: this query is pure read_csv() and needs no
    # sitting_duck extension, so no LOAD and no -unsigned.
    local rc=0
    run_duckdb "${baseline_bin}" "" -list "${query}" "${out_file}" "${err_file}" || rc=$?
    # allow_empty=1: no findings is the good outcome here, not a broken run.
    check_duckdb_result "structural classification of ${corpus_name}" "${rc}" "${out_file}" "${err_file}" 1
}

count_tag() { grep -c "^$1"$'\t' "$2" || true; }

# ---------------------------------------------------------------- report
version_of() {
    local bin="$1"; shift
    "${duckdb_env[@]}" "${bin}" "$@" -version 2>/dev/null | tail -1
}

printf '== comparing DuckDB lines ==\n'
printf 'baseline   : %s\n' "${baseline_bin}"
printf '             %s\n' "$(version_of "${baseline_bin}" "${baseline_flags[@]+"${baseline_flags[@]}"}")"
[[ -n ${baseline_ext} ]] && printf '  extension: %s\n' "${baseline_ext}"
printf 'candidate  : %s\n' "${candidate_bin}"
printf '             %s\n' "$(version_of "${candidate_bin}" "${candidate_flags[@]+"${candidate_flags[@]}"}")"
[[ -n ${candidate_ext} ]] && printf '  extension: %s\n' "${candidate_ext}"
printf 'corpus     : %s (%d file(s), language=%s)\n' "${corpus_dir}" "${#corpus_files[@]}" "${language}"
if ((strict)); then
    printf 'allowlist  : IGNORED (--strict): any structural drift fails\n'
elif ((allowlist_entries > 0)); then
    printf 'allowlist  : %s (%d entries)\n' "${allowlist}" "${allowlist_entries}"
else
    printf 'allowlist  : none\n'
fi
printf '\n'

unexpected_files=()
known_files=()
stale_files=()
peek_files=()
identical_files=()
# Files whose four dumps all passed the guards. The negative control reuses
# their baseline dumps as its unperturbed side, so it costs two extra dumps per
# file instead of four -- which is what makes running it on EVERY file cheap.
dumped_files=()

for sql_file in "${corpus_files[@]}"; do
    name=$(basename "${sql_file}")
    stem="${out_dir}/${name%.sql}"
    printf -- '-- %s\n' "${name}"

    file_ok=1
    dump baseline  "${baseline_bin}"  "${baseline_ext}"  "${sql_file}" struct "${stem}.baseline.struct.csv"  || file_ok=0
    dump candidate "${candidate_bin}" "${candidate_ext}" "${sql_file}" struct "${stem}.candidate.struct.csv" || file_ok=0
    dump baseline  "${baseline_bin}"  "${baseline_ext}"  "${sql_file}" peek   "${stem}.baseline.peek.csv"    || file_ok=0
    dump candidate "${candidate_bin}" "${candidate_ext}" "${sql_file}" peek   "${stem}.candidate.peek.csv"   || file_ok=0
    if ((!file_ok)); then
        printf '  SKIPPED (see harness failure above)\n\n'
        continue
    fi
    dumped_files+=("${name}")

    findings="${stem}.struct.findings.tsv"
    if ! classify_struct "${stem}.baseline.struct.csv" "${stem}.candidate.struct.csv" \
                         "${name}" "${effective_allowlist}" "${findings}"; then
        printf '  SKIPPED (see harness failure above)\n\n'
        continue
    fi

    nodes=$(wc -l <"${stem}.baseline.struct.csv")
    n_unexpected=$(count_tag UNEXPECTED "${findings}")
    n_known=$(count_tag KNOWN "${findings}")
    n_stale=$(count_tag STALE "${findings}")

    peek_differs=0
    diff -u "${stem}.baseline.peek.csv" "${stem}.candidate.peek.csv" >"${stem}.diff.peek" || peek_differs=1

    if ((n_unexpected > 0)); then
        unexpected_files+=("${name}"); retain_dumps
        printf '  UNEXPECTED STRUCTURAL DRIFT: %d of %d node(s) -- THIS IS A REGRESSION\n' "${n_unexpected}" "${nodes}"
        grep "^UNEXPECTED"$'\t' "${findings}" | head -10 | sed 's/^/    | /'
        printf '    all findings: %s\n' "${findings}"
    fi
    if ((n_known > 0)); then
        known_files+=("${name}"); retain_dumps
        printf '  KNOWN DRIFT: %d of %d node(s) match the allowlist (not a failure)\n' "${n_known}" "${nodes}"
        grep "^KNOWN"$'\t' "${findings}" | head -10 | sed 's/^/    | /'
    fi
    if ((n_stale > 0)); then
        stale_files+=("${name}"); retain_dumps
        printf '  STALE ALLOWLIST ENTR%s: %d entr%s matched nothing -- upstream changed again\n' \
            "$( ((n_stale == 1)) && echo Y || echo IES )" "${n_stale}" "$( ((n_stale == 1)) && echo 'y' || echo 'ies' )"
        grep "^STALE"$'\t' "${findings}" | sed 's/^/    | /'
    fi
    if ((peek_differs)); then
        peek_files+=("${name}"); retain_dumps
        drifted=$(grep -c '^-[0-9]' "${stem}.diff.peek" || true)
        printf '  PEEK DRIFT: %d of %d node(s) re-render differently\n' "${drifted}" "${nodes}"
        printf '    (expected across DuckDB lines -- peek comes from the upstream deparser)\n'
        printf '    diff: %s\n' "${stem}.diff.peek"
        # The dumps hold md5s, not text, so the diff names WHICH nodes moved
        # but not HOW. Hand over the query that shows the actual rendering.
        printf "    to see the text: SELECT node_id, type, peek FROM read_ast('%s', '%s') WHERE node_id IN (...);\n" \
            "${sql_file}" "${language}"
    fi
    if ((n_unexpected == 0 && n_known == 0 && n_stale == 0 && !peek_differs)); then
        identical_files+=("${name}")
        printf '  identical (%d nodes, structure and peek)\n' "${nodes}"
    fi
    printf '\n'
done

# ---------------------------------------------------------------- negative control
# A comparison that cannot fail is worthless. Before trusting a green result we
# plant a change and require the SAME classify_struct() to call it UNEXPECTED.
#
# Two deliberate choices:
#  * Both sides are dumped with the BASELINE binary, so the perturbation is the
#    only variable.
#  * The control always runs with an EMPTY allowlist, so an allowlist entry can
#    never absorb the planted change and quietly turn the control green. That is
#    the failure mode the allowlist introduces, so it is designed out rather
#    than tested for.
#
# The perturbation appends one trivial statement -- the smallest edit that is
# guaranteed to (a) still parse on every DuckDB line and (b) change the tree:
# it adds nodes and bumps the root's child and descendant counts, so it can
# never be confused with a triple-only (allowlistable) change.
printf -- '-- negative control\n'
control_allowlist="${out_dir}/allowlist.control.tsv"
write_allowlist_sentinel "${control_allowlist}"

control_passed=0
if ((${#dumped_files[@]} == 0)); then
    printf '  FAILED: no corpus file produced trustworthy dumps to perturb\n\n'
else
    # Every file is controlled, not just the first. Controlling only
    # corpus_files[0] would quietly re-target itself the day someone adds a file
    # that sorts earlier, and would then be testing a file nobody chose.
    control_passed=1
    for name in "${dumped_files[@]}"; do
        src="${corpus_dir}/${name}"
        stem="${out_dir}/${name%.sql}"
        copy="${out_dir}/${name%.sql}.perturbed.sql"
        cp -- "${src}" "${copy}"
        printf '\nSELECT 1 AS negative_control_probe;\n' >>"${copy}"

        # Only the perturbed side needs dumping: the unperturbed side is the
        # baseline dump already taken above, so both sides come from the SAME
        # binary.
        control_findings="${stem}.control.findings.tsv"
        pair_ok=1
        dump baseline "${baseline_bin}" "${baseline_ext}" "${copy}" struct "${stem}.perturbed.struct.csv" || pair_ok=0
        if ((pair_ok)); then
            classify_struct "${stem}.baseline.struct.csv" "${stem}.perturbed.struct.csv" \
                            "${name}" "${control_allowlist}" "${control_findings}" || pair_ok=0
        fi

        if ((!pair_ok)); then
            control_passed=0
            printf '  FAILED (%s): could not dump or classify the perturbed copy (see harness failure above)\n' "${name}"
            continue
        fi

        n_control=$(count_tag UNEXPECTED "${control_findings}")
        if ((n_control > 0)); then
            printf '  passed (%s): a one-statement perturbation produced %d UNEXPECTED finding(s)\n' "${name}" "${n_control}"
        else
            control_passed=0
            retain_dumps
            printf '  FAILED (%s): a one-statement perturbation produced NO unexpected finding.\n' "${name}"
            printf '           The classifier cannot detect change, so every green result above is meaningless.\n'
            printf '           findings: %s\n' "${control_findings}"
        fi
    done
    printf '\n'
fi

# ---------------------------------------------------------------- summary
join_names() { local IFS=' '; [[ $# -eq 0 ]] && return 0; printf ' (%s)' "$*"; }

printf '== summary ==\n'
printf 'identical            : %d\n' "${#identical_files[@]}"
printf 'peek drift           : %d%s\n' "${#peek_files[@]}" "$(join_names "${peek_files[@]+"${peek_files[@]}"}")"
printf 'known structural     : %d%s\n' "${#known_files[@]}" "$(join_names "${known_files[@]+"${known_files[@]}"}")"
printf 'unexpected structural: %d%s\n' "${#unexpected_files[@]}" "$(join_names "${unexpected_files[@]+"${unexpected_files[@]}"}")"
printf 'stale allowlist      : %d%s\n' "${#stale_files[@]}" "$(join_names "${stale_files[@]+"${stale_files[@]}"}")"
printf 'negative control     : %s\n' "$( ((control_passed)) && echo passed || echo FAILED )"
printf 'harness guards       : %s\n' "$( ((harness_failed)) && echo TRIPPED || echo clean )"
if ((keep_out_dir)); then
    printf 'dumps and findings   : %s\n' "${out_dir}"
else
    printf 'dumps and findings   : discarded (nothing to inspect; pass --out to keep them)\n'
fi

# Order matters: a tripped guard or a dead control invalidates every verdict, so
# they are checked before -- and preferred over -- the drift results themselves.
if ((harness_failed)); then
    printf '\nRESULT: INCONCLUSIVE -- a harness guard tripped; this run proved nothing.\n'
    exit "${EXIT_HARNESS}"
fi
if ((!control_passed)); then
    printf '\nRESULT: INCONCLUSIVE -- the negative control failed.\n'
    exit "${EXIT_CONTROL}"
fi
if ((${#unexpected_files[@]} > 0)); then
    printf '\nRESULT: FAIL -- unexplained structural drift in %d file(s).\n' "${#unexpected_files[@]}"
    printf '        Either sitting_duck regressed, or this is new upstream behaviour that\n'
    printf '        belongs in the allowlist WITH a reason -- decide, do not just add it.\n'
    exit "${EXIT_STRUCTURAL}"
fi
if ((${#stale_files[@]} > 0)); then
    printf '\nRESULT: FAIL -- %d file(s) have allowlist entries that matched nothing.\n' "${#stale_files[@]}"
    printf '        The allowlist is describing a difference that no longer exists, so it is\n'
    printf '        lying about what these binaries do. Remove or update the entry.\n'
    exit "${EXIT_STALE_ALLOWLIST}"
fi
if ((${#known_files[@]} > 0)); then
    printf '\nRESULT: PASS with known drift -- no unexplained structural difference.\n'
    cat <<'EOF'
        Allowlisted structural drift and `peek` re-rendering are upstream
        behaviour, not sitting_duck regressions -- but read the findings before
        updating any test that asserts on node types or on `peek` text.
EOF
    exit "${EXIT_OK}"
fi
if ((${#peek_files[@]} > 0)); then
    printf '\nRESULT: PASS with peek drift -- structure matches on every file; %d file(s)\n' "${#peek_files[@]}"
    cat <<'EOF'
        re-render `peek` differently. That is upstream deparser output, not a
        sitting_duck regression; tests that assert on `peek` text need updating.
EOF
    exit "${EXIT_OK}"
fi
printf '\nRESULT: PASS -- structure and peek identical on every corpus file.\n'
exit "${EXIT_OK}"
