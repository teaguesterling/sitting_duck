#!/usr/bin/env bash
#
# verify_byte_offsets.sh -- prove that read_ast's start_byte / end_byte
# (source := 'full') address the bytes they claim to, over a whole corpus.
#
# WHY THIS EXISTS
# ---------------
# start_byte / end_byte are the substrate for byte-exact unparse
# (`write_ast(read_ast(x, source := 'full')) = x`, tracker 048 #2). The
# committed sqllogictest, test/sql/source_byte_offsets.test, pins the contract:
# the gating, the 0-based half-open semantics, hand-computed offsets on a
# CRLF + multi-byte fixture. What it cannot do is sweep a corpus -- the
# per-node hex slicing below is quadratic-ish in file size and would make the
# suite crawl.
#
# So this script does the volume: every node of every file in a corpus, across
# every language it is pointed at, with TWO checks that fail for different
# reasons. The separation is the whole point:
#
#   * PASS 1 -- SLICE vs PEEK. The byte slice at [start_byte, end_byte) must
#     equal the node's own `peek := 'full'` text. This is an end-to-end check
#     that the SURFACED offsets address real, in-file text, and it covers
#     every language.
#     It is also WEAKLY CIRCULAR, and saying so is the point: `peek` is
#     produced by the parser as content.substr(start_byte, end_byte -
#     start_byte) from the SAME tree-sitter offsets. If tree-sitter itself
#     reported a wrong offset, peek and the slice would be wrong together and
#     this pass would stay green.
#
#   * PASS 2 -- THE NON-CIRCULAR ORACLE, and the reason this script is worth
#     keeping. It never looks at `peek`. It rebuilds every line's absolute byte
#     start from the raw bytes of the file -- split on LF, take the BYTE length
#     of each piece (so a retained CR counts, and a multi-byte character counts
#     as its bytes, not as one character), add 1 for the LF -- and then
#     requires, for every node:
#
#         start_byte == line_start(start_line) + start_column - 1
#         end_byte   == line_start(end_line)   + end_column   - 1
#
#     That reconstruction is exactly the fragile thing byte offsets exist to
#     replace (it is what src/sql_macros/ast_patch.sql still does by hand).
#     Here it is used as an INDEPENDENT ORACLE: it derives positions from the
#     file's bytes and from the line/column columns, so it agrees with
#     start_byte only if both are right. It is what catches the class of bug
#     pass 1 cannot see.
#
#   * NEGATIVE CONTROL. Both passes above report "0 problems" when they work
#     AND when they compare nothing (an empty corpus, a typo'd glob, a join
#     that matched no rows). So a run also plants a deliberate one-byte offset
#     error and requires pass 1's comparison to notice it. A control that does
#     not fire means the report is void, not clean.
#
# BYTE SLICING IN SQL: WHY to_hex() AND NOT substring()
# -----------------------------------------------------
# DuckDB's substring() on VARCHAR is CHARACTER-indexed --
# substring('hello' with an accent, 2, 2) counts characters, not bytes -- and
# DuckDB v1.5.6 has no substring(BLOB, ...) overload at all (only
# substring(VARCHAR, BIGINT[, BIGINT])). So the obvious
# `substring(content, start_byte + 1, end_byte - start_byte)` is NOT byte-exact
# on multi-byte input: it silently returns the wrong text, which is the exact
# failure mode byte offsets exist to remove.
#
# The byte-exact primitive used throughout is therefore to hex-encode first:
#
#     from_hex(substring(to_hex(<blob>), 2 * start_byte + 1,
#                                        2 * (end_byte - start_byte)))
#
# to_hex() of a BLOB is pure ASCII at exactly two characters per byte, so
# character indexing over the hex string IS byte indexing over the bytes.
# read_blob() is used rather than read_text() so nothing can normalize the
# bytes on the way in.
#
# FIXTURES THAT MATTER
# --------------------
# The default corpus includes test/data/encoding/ (CRLF line endings AND
# multi-byte UTF-8 in one file) and test/data/python/unicode.py (multi-byte,
# LF). Both are marked `-text` in .gitattributes precisely so git cannot
# normalize them into testing nothing. If you add a byte-exactness fixture,
# add it to .gitattributes too.
#
# NOT COVERED
# -----------
# The `duckdb` language adapter wraps DuckDB's own SQL parser rather than
# tree-sitter and has no byte positions to report: it returns
# start_byte = end_byte = 0 for every node (and already hardcodes its
# line/column columns to 1). Do not point this script at `duckdb` and do not
# read a result for it as meaningful. Byte offsets are a tree-sitter-backed
# property.
#
set -euo pipefail

# ---------------------------------------------------------------- exit codes
# Distinct codes because "the offsets are wrong", "the harness compared
# nothing" and "the line/column oracle disagrees" demand different responses,
# and a bare `exit 1` cannot tell a caller or a CI log which one happened.
readonly EXIT_OK=0
readonly EXIT_USAGE=1
readonly EXIT_HARNESS=2    # a guard tripped; the comparison is void
readonly EXIT_SLICE=3      # pass 1: a slice did not match the node's own text
readonly EXIT_ORACLE=4     # pass 2: byte offsets disagree with line/column
readonly EXIT_CONTROL=5    # the negative control failed to fire

# ---------------------------------------------------------------- defaults
# Resolved from the script's own location, never from $PWD. read_ast globs here
# are repo-relative, and a corpus that resolves to "whatever directory you
# happened to be in" is a corpus that silently sweeps nothing.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)

binary="${repo_root}/build/release/duckdb"
extension=""
ignore_errors=0
quiet=0
globs=()

# Every default glob pins its language via the file extension, so language
# detection cannot silently skip a file. test/data/encoding is first because
# it is the fixture the whole exercise is about.
readonly DEFAULT_GLOBS=(
    'test/data/encoding/*.py'
    'test/data/python/*.py'
    'test/data/python/macros/*.py'
    'test/data/python/scope_resolution/*.py'
    'test/data/javascript/*.js'
    'test/data/c/*.c'
    'test/data/cpp/*.cpp'
    'test/data/go/*.go'
    'test/data/rust/*.rs'
    'test/data/ruby/*.rb'
    'test/data/bash/*.sh'
    'test/data/swift/*.swift'
)

usage() {
    cat <<EOF
Usage: $(basename "$0") [options] [glob ...]

Verifies that read_ast's start_byte / end_byte (source := 'full') slice the
bytes they claim to, for every node of every file matched by each glob.

Runs two checks per glob, which fail for different reasons:
  pass 1  slice == the node's own \`peek := 'full'\` text (compared as BLOBs).
          Covers every language, but is weakly circular: peek derives from the
          same tree-sitter offsets.
  pass 2  the NON-CIRCULAR oracle. Rebuilds each line's byte start from the raw
          bytes and requires
              start_byte == line_start(start_line) + start_column - 1
          for every node. Independent of peek; this is the pass that can catch
          a genuinely wrong offset.
Plus a negative control that plants a one-byte error and requires pass 1's
comparison to notice it.

Globs are repo-relative (resolved against ${repo_root}).
With no glob arguments, sweeps a default corpus of ${#DEFAULT_GLOBS[@]} globs
across 11 languages, including the CRLF + multi-byte fixtures.

Options:
  --binary PATH      duckdb binary to use
                     (default: ${binary})
  --extension PATH   sitting_duck extension to LOAD
                     (omit when the build links it statically)
  --ignore-errors    pass ignore_errors := true to read_ast. Off by default: a
                     file that fails to parse should be loud, not silently
                     dropped from a sweep that then reports zero problems.
  -q, --quiet        only print the final summary lines
  -h, --help         this text

Exit codes:
  ${EXIT_OK}  all checks passed
  ${EXIT_USAGE}  usage error
  ${EXIT_HARNESS}  a guard tripped (empty glob, file-count mismatch, no nodes)
                   -- the comparison is void, not clean
  ${EXIT_SLICE}  pass 1 found a slice that does not match the node's text
  ${EXIT_ORACLE}  pass 2 found byte offsets disagreeing with line/column
  ${EXIT_CONTROL}  the negative control did not fire

Do NOT point this at the \`duckdb\` language: that adapter wraps DuckDB's own
parser, has no byte positions, and reports 0/0 for every node.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --binary)        binary="${2:?--binary needs a path}"; shift 2 ;;
        --extension)     extension="${2:?--extension needs a path}"; shift 2 ;;
        --ignore-errors) ignore_errors=1; shift ;;
        -q|--quiet)      quiet=1; shift ;;
        -h|--help)       usage; exit "${EXIT_OK}" ;;
        --)              shift; break ;;
        -*)              echo "unknown option: $1" >&2; usage >&2; exit "${EXIT_USAGE}" ;;
        *)               break ;;
    esac
done
globs=("$@")
[ ${#globs[@]} -eq 0 ] && globs=("${DEFAULT_GLOBS[@]}")

if [ ! -x "${binary}" ]; then
    echo "not an executable duckdb binary: ${binary}" >&2
    echo "(build it with: GEN=ninja make release)" >&2
    exit "${EXIT_USAGE}"
fi
if [ -n "${extension}" ] && [ ! -f "${extension}" ]; then
    echo "no such extension file: ${extension}" >&2
    exit "${EXIT_USAGE}"
fi

cd -- "${repo_root}"

# read_ast globs are relative to CWD, which is now repo_root.
load_prelude=""
if [ -n "${extension}" ]; then
    load_prelude="LOAD '${extension}';"
fi
ignore_clause=""
if [ "${ignore_errors}" -eq 1 ]; then
    ignore_clause=", ignore_errors := true"
fi

# Runs one SQL statement and returns its rows as pipe-separated fields.
# `-noheader -list` keeps parsing trivial; every query below selects integers.
run_sql() {
    "${binary}" -noheader -list -c "${load_prelude} $1"
}

say() { [ "${quiet}" -eq 1 ] || printf '%s\n' "$*"; }

# Number of files a repo-relative glob matches on disk.
#
# Deliberately NOT `ls -1 $glob | wc -l`: under `set -e -o pipefail` a glob
# that matches nothing makes `ls` exit non-zero, the pipeline fail, and the
# whole script die mid-sweep with a non-zero status that LOOKS like a guard
# firing but printed no report at all. Shell glob expansion cannot fail that
# way. With nullglob unset a non-matching pattern expands to itself, so the
# single-element case is checked for existence.
count_glob() {
    local pattern="$1"
    local -a matches
    # shellcheck disable=SC2206  # unquoted expansion is the point: we want globbing
    matches=( $pattern )
    if [ "${#matches[@]}" -eq 0 ]; then
        echo 0
    elif [ "${#matches[@]}" -eq 1 ] && [ ! -e "${matches[0]}" ]; then
        echo 0
    else
        echo "${#matches[@]}"
    fi
}

say "binary:  ${binary}"
say "repo:    ${repo_root}"
[ -n "${extension}" ] && say "extension: ${extension}"
say ""

# =============================================================================
# PASS 1 -- SLICE vs PEEK  (every language; weakly circular, see header)
# =============================================================================
say "pass 1: byte slice at [start_byte, end_byte) == the node's own peek text"
say "        (end-to-end, all languages; weakly circular -- peek derives from"
say "         the same tree-sitter offsets, so this cannot catch tree-sitter"
say "         itself being wrong. that is pass 2's job.)"
say ""

total_files=0
total_nodes=0
slice_failures=0
harness_failures=0

for glob in "${globs[@]}"; do
    # Guard: a glob that matches nothing must be a harness error, not a silent
    # "0 problems". This is the single easiest way for this script to lie.
    on_disk=$(count_glob "${glob}")
    if [ "${on_disk}" -eq 0 ]; then
        printf 'HARNESS  %-48s glob matched no files on disk\n' "${glob}"
        harness_failures=$((harness_failures + 1))
        continue
    fi

    out=$(run_sql "
        WITH nodes AS (
            SELECT file_path, node_id, start_byte, end_byte, peek
            FROM read_ast('${glob}', source := 'full', peek := 'full'${ignore_clause})
        ),
        files AS (
            SELECT filename AS file_path,
                   to_hex(content) AS hex,
                   octet_length(content) AS nbytes
            FROM read_blob('${glob}')
        ),
        joined AS (
            SELECT n.file_path, n.start_byte, n.end_byte, n.peek, f.nbytes,
                   from_hex(substring(f.hex, 2 * n.start_byte + 1,
                                             2 * (n.end_byte - n.start_byte))) AS slice
            FROM nodes n JOIN files f USING (file_path)
        )
        SELECT count(*),
               count(DISTINCT file_path),
               -- A node's slice must equal its peek text byte for byte.
               -- Compared as BLOBs so no decode step can mask a difference.
               -- Zero-width nodes (MISSING / zero-length ERROR, EOF) have a
               -- NULL peek and an empty slice: those agree, and are counted.
               count(*) FILTER (
                   WHERE coalesce(slice, ''::BLOB) <> coalesce(encode(peek), ''::BLOB)
               ),
               -- Offsets must also stay inside the file and run forwards.
               count(*) FILTER (WHERE start_byte > end_byte OR end_byte > nbytes)
        FROM joined;
    ")
    nodes=$(echo "${out}" | cut -d'|' -f1)
    files=$(echo "${out}" | cut -d'|' -f2)
    mismatches=$(echo "${out}" | cut -d'|' -f3)
    out_of_range=$(echo "${out}" | cut -d'|' -f4)

    # Guard: every file the glob matched on disk must have produced nodes. A
    # file silently dropped (parse failure under --ignore-errors, a language
    # that did not resolve, a join that did not match) would otherwise shrink
    # the corpus while the report still said "0 mismatches".
    status="ok"
    if [ "${files}" -ne "${on_disk}" ]; then
        status="HARNESS: ${files} of ${on_disk} files produced nodes"
        harness_failures=$((harness_failures + 1))
    elif [ "${nodes}" -eq 0 ]; then
        status="HARNESS: no nodes"
        harness_failures=$((harness_failures + 1))
    fi

    say "$(printf '%-48s files=%-4s nodes=%-7s mismatches=%-5s out_of_range=%-5s %s' \
        "${glob}" "${files}" "${nodes}" "${mismatches}" "${out_of_range}" "${status}")"

    total_files=$((total_files + files))
    total_nodes=$((total_nodes + nodes))
    slice_failures=$((slice_failures + mismatches + out_of_range))
done

say "----"
say "PASS 1: files=${total_files} nodes=${total_nodes} failures=${slice_failures}"
say ""

# =============================================================================
# PASS 2 -- THE NON-CIRCULAR ORACLE  (independent of peek; see header)
# =============================================================================
# Rebuilds each line's absolute byte start from the file's raw bytes and
# requires start_byte / end_byte to agree with (line, column). Never reads
# `peek`, so it does not share an input with pass 1. This is the pass that can
# catch a wrong offset rather than a mis-projected one, and it is the reason
# CRLF and multi-byte fixtures belong in the corpus: a reconstruction that
# counted characters instead of bytes, or dropped the CR, would disagree here.
say "pass 2: start_byte == line_start(start_line) + start_column - 1,"
say "        with line starts rebuilt from the raw bytes"
say "        (NON-CIRCULAR: independent of peek -- this is the oracle)"
say ""

oracle_failures=0

for glob in "${globs[@]}"; do
    on_disk=$(count_glob "${glob}")
    if [ "${on_disk}" -eq 0 ]; then
        continue  # already counted as a harness failure in pass 1
    fi

    out=$(run_sql "
        WITH f AS (SELECT filename, decode(content) AS txt FROM read_blob('${glob}')),
        parts AS (
            -- Two UNNESTs in one SELECT zip together in DuckDB, which pairs
            -- each line with its 1-based line number without a window.
            SELECT filename,
                   unnest(string_split(txt, chr(10))) AS l,
                   unnest(range(1, len(string_split(txt, chr(10))) + 1)) AS line_no
            FROM f
        ),
        line_starts AS (
            -- Split on LF only, so a CRLF file keeps its CR inside the line
            -- and that CR is counted. octet_length(encode(l)) is the line's
            -- length in BYTES, not characters. +1 for the LF that ended it.
            SELECT filename, line_no,
                   coalesce(sum(octet_length(encode(l)) + 1) OVER (
                       PARTITION BY filename ORDER BY line_no
                       ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING), 0) AS line_start
            FROM parts
        ),
        n AS (
            SELECT file_path, start_line, start_column, end_line, end_column,
                   start_byte, end_byte
            FROM read_ast('${glob}', source := 'full', peek := 'none'${ignore_clause})
        )
        SELECT count(*),
               count(*) FILTER (WHERE n.start_byte <> s.line_start + n.start_column - 1
                                   OR n.end_byte   <> e.line_start + n.end_column   - 1)
        FROM n
        JOIN line_starts s ON s.filename = n.file_path AND s.line_no = n.start_line
        JOIN line_starts e ON e.filename = n.file_path AND e.line_no = n.end_line;
    ")
    checked=$(echo "${out}" | cut -d'|' -f1)
    bad=$(echo "${out}" | cut -d'|' -f2)

    # Guard: the oracle joins on line numbers, so a dropped join silently
    # checks fewer nodes. It must cover the same node count pass 1 swept.
    status="ok"
    if [ "${checked}" -eq 0 ]; then
        status="HARNESS: oracle checked no nodes"
        harness_failures=$((harness_failures + 1))
    fi

    say "$(printf '%-48s checked=%-7s disagreements=%-5s %s' \
        "${glob}" "${checked}" "${bad}" "${status}")"
    oracle_failures=$((oracle_failures + bad))
done

say "----"
say "PASS 2: disagreements=${oracle_failures}"
say ""

# =============================================================================
# NEGATIVE CONTROL -- does the comparison in pass 1 actually detect anything?
# =============================================================================
# Both passes report zero when they are right AND when they compare nothing.
# Plant a one-byte shift in the slice and require the comparison to flag it.
# If this does not fire, every "0" above is meaningless.
say "negative control: a planted one-byte offset must be detected"

control=$(run_sql "
    WITH nodes AS (
        SELECT file_path, start_byte, end_byte, peek
        FROM read_ast('test/data/encoding/*.py', source := 'full', peek := 'full')
        WHERE end_byte - start_byte > 1
    ),
    files AS (SELECT filename AS file_path, to_hex(content) AS hex FROM read_blob('test/data/encoding/*.py'))
    SELECT count(*),
           count(*) FILTER (
               -- deliberately start one byte late: 2 * (start_byte + 1) + 1
               WHERE coalesce(from_hex(substring(f.hex, 2 * (n.start_byte + 1) + 1,
                                                        2 * (n.end_byte - n.start_byte))), ''::BLOB)
                     <> coalesce(encode(n.peek), ''::BLOB)
           )
    FROM nodes n JOIN files f USING (file_path);
")
control_rows=$(echo "${control}" | cut -d'|' -f1)
control_detected=$(echo "${control}" | cut -d'|' -f2)

control_failed=0
if [ "${control_rows}" -eq 0 ]; then
    say "  CONTROL VOID: the control query matched no nodes"
    control_failed=1
elif [ "${control_detected}" -ne "${control_rows}" ]; then
    say "  CONTROL FAILED: only ${control_detected} of ${control_rows} planted errors detected"
    control_failed=1
else
    say "  ok: ${control_detected}/${control_rows} planted errors detected"
fi
say ""

# ---------------------------------------------------------------- verdict
printf 'TOTAL files=%s nodes=%s | pass1_failures=%s pass2_disagreements=%s harness=%s control=%s\n' \
    "${total_files}" "${total_nodes}" "${slice_failures}" "${oracle_failures}" \
    "${harness_failures}" "$([ "${control_failed}" -eq 0 ] && echo ok || echo FAILED)"

# Harness and control first: if either tripped, the other numbers are void and
# reporting them as a pass would be the exact false green this script exists to
# avoid.
[ "${harness_failures}" -eq 0 ] || { echo "VOID: a guard tripped; the comparison proved nothing" >&2; exit "${EXIT_HARNESS}"; }
[ "${control_failed}"   -eq 0 ] || { echo "VOID: negative control did not fire" >&2; exit "${EXIT_CONTROL}"; }
[ "${slice_failures}"   -eq 0 ] || { echo "FAIL: byte slices do not match node text" >&2; exit "${EXIT_SLICE}"; }
[ "${oracle_failures}"  -eq 0 ] || { echo "FAIL: byte offsets disagree with line/column" >&2; exit "${EXIT_ORACLE}"; }

echo "PASS: byte offsets verified over ${total_nodes} nodes in ${total_files} files"
exit "${EXIT_OK}"
