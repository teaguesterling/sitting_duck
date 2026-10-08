#!/usr/bin/env bash
#
# verify_unparse_byte_exact.sh -- prove the byte-exact unparse law
#
#     write_ast(read_ast(x, source := 'full')) = x          byte for byte
#
# over a whole corpus, one language at a time, and measure the rules-based
# unparser against the same files for contrast.
#
# WHY THIS EXISTS
# ---------------
# tracker/features/047-unparse-level4-coverage-and-templates.md records, as a
# MEASURED result, that byte-identity held for none of the 26 corpus
# languages: the rules-based `ast_unparse*` macros reconstruct source from node
# types and names and never read a position column, so inter-token whitespace
# is normalised. `ast_unparse_exact*` (src/sql_macros/ast_unparse.sql) closes
# that gap by splicing the original bytes at [start_byte, end_byte) instead.
#
# The committed sqllogictest, test/sql/ast_unparse_exact.test, pins the
# contract and the error paths. What it cannot do is sweep a corpus and print
# per-language numbers; that is this script's job, and it is the thing to re-run
# after a grammar bump or a new language.
#
# WHAT IS COMPARED, AND WHY IT IS NOT CIRCULAR
# --------------------------------------------
# The oracle is the FILE ITSELF, read as a BLOB by read_blob() and compared as
# a BLOB. Nothing derived from the parse takes part in the comparison -- not
# `peek`, not line/column, not the byte offsets. So a wrong byte offset cannot
# hide: it would move a slice boundary and the result would differ from the
# file. (This is a strictly stronger oracle than scripts/verify_byte_offsets.sh
# pass 1, which compares a slice against `peek` -- also derived from the same
# offsets.)
#
# THREE WAYS THIS SCRIPT COULD LIE, AND THE GUARD FOR EACH
# --------------------------------------------------------
#   1. IT COMPARED NOTHING. A typo'd glob, a parse that produced no rows, or a
#      macro that returned zero rows all yield "0 mismatches".
#      GUARD: every file must produce exactly one output row, and the glob must
#      match on disk. A file that does not is a harness failure, not a pass.
#
#   2. THE SPLICE WAS VACUOUS. If the leaf frontier were empty, or every leaf
#      zero-width, the output would be prologue-gap + tail-gap = the whole
#      file: byte-exact and meaningless.
#      GUARD (hard, per file): `leaves > 0`, and `leaf_bytes > 0` for any file
#      with content -- the leaf slices must actually carry bytes.
#      REPORTED (not a hard per-file guard): `gap_bytes`, the bytes NOT inside
#      any leaf -- whitespace, hidden delimiters such as tree-sitter-kotlin's
#      string quotes, the trailing newline. A real source file always has some,
#      but a file whose tokens tile it exactly (minified JSON, say) legitimately
#      has none, so a per-file zero is printed rather than failed. The corpus
#      total must be > 0 or the gap machinery was never exercised at all.
#      A zero-byte file is counted as `trivial` and exempted: every claim about
#      it is vacuous by nature, which is a property of the fixture.
#
#   3. THE EQUALITY CANNOT FAIL. A test that passes no matter what is worth
#      nothing.
#      NEGATIVE CONTROL: the same splice is recomputed with every leaf's
#      start_byte shifted by one, and with one leaf's text replaced. Both must
#      be detected as different from the file. If either does not fire, the
#      whole report is void.
#
# NOT COVERED
# -----------
# The `duckdb` language adapter: it wraps DuckDB's own SQL parser, has no byte
# positions (start_byte = end_byte = 0 for every node) and reports
# children_count = 0 for every node (issue #197). ast_unparse_exact ERRORS on
# it rather than emitting anything, which is the intended behaviour; do not
# point this script at it.
#
set -euo pipefail

readonly EXIT_OK=0
readonly EXIT_USAGE=1
readonly EXIT_HARNESS=2    # a guard tripped; the comparison is void
readonly EXIT_EXACT=3      # a file did not reproduce byte for byte
readonly EXIT_VACUOUS=4    # the splice proved nothing (no leaves, or no gaps)
readonly EXIT_CONTROL=5    # the negative control failed to fire

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)

binary="${repo_root}/build/release/duckdb"
extension=""
quiet=0

# One file per language. test/data/unparse_leaf_text/ is the 26-language corpus
# 047's 4a built; the two extra fixtures are the ones that make byte-exactness
# non-trivial -- CRLF line endings together with multi-byte UTF-8, and
# multi-byte UTF-8 with LF. Both are marked `-text` in .gitattributes so git
# cannot normalize them into testing nothing.
readonly DEFAULT_GLOBS=(
    'test/data/unparse_leaf_text/*'
    'test/data/encoding/*.py'
    'test/data/python/unicode.py'
)

usage() {
    cat <<EOF
Usage: $(basename "$0") [options] [glob ...]

Verifies, for every file matched by each glob:

    ast_unparse_exact(file) == the file's bytes          (compared as BLOBs)

and reports, for contrast, whether the rules-based ast_unparse(file) matches
too (it is not expected to -- below source := 'full' the law is structural
only, and normalising whitespace there is conformant, per issue #89).

Per file it also reports:
  leaves      nodes in the leaf frontier (descendant_count = 0); must be > 0,
              and their slices must carry bytes, or the equality proved nothing
  gap_bytes   bytes NOT inside any leaf -- whitespace and hidden delimiters.
              Printed per file; the corpus total must be > 0.
A zero-byte file is counted as `trivial` and exempted from both.

Globs are repo-relative (resolved against ${repo_root}).
With no arguments, sweeps ${#DEFAULT_GLOBS[@]} globs covering 26 languages plus
the CRLF and multi-byte fixtures.

Options:
  --binary PATH      duckdb binary (default: ${binary})
  --extension PATH   sitting_duck extension to LOAD (omit for a static build)
  -q, --quiet        only print the summary
  -h, --help         this text

Exit codes:
  ${EXIT_OK}  all files reproduced byte for byte
  ${EXIT_USAGE}  usage error
  ${EXIT_HARNESS}  a guard tripped (empty glob, no output row) -- result is void
  ${EXIT_EXACT}  a file did not reproduce byte for byte
  ${EXIT_VACUOUS}  a file's equality was vacuous (no leaves, or no leaf bytes),
                   or no file in the corpus had any gap bytes
  ${EXIT_CONTROL}  the negative control did not fire

Do NOT point this at the \`duckdb\` language: no byte positions (issue #197).
ast_unparse_exact errors on it by design.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --binary)    binary="${2:?--binary needs a path}"; shift 2 ;;
        --extension) extension="${2:?--extension needs a path}"; shift 2 ;;
        -q|--quiet)  quiet=1; shift ;;
        -h|--help)   usage; exit "${EXIT_OK}" ;;
        --)          shift; break ;;
        -*)          echo "unknown option: $1" >&2; usage >&2; exit "${EXIT_USAGE}" ;;
        *)           break ;;
    esac
done
globs=("$@")
[ ${#globs[@]} -eq 0 ] && globs=("${DEFAULT_GLOBS[@]}")

if [ ! -x "${binary}" ]; then
    echo "not an executable duckdb binary: ${binary}" >&2
    echo "(build it with: GEN=ninja CMAKE_BUILD_PARALLEL_LEVEL=10 make release)" >&2
    exit "${EXIT_USAGE}"
fi
if [ -n "${extension}" ] && [ ! -f "${extension}" ]; then
    echo "no such extension file: ${extension}" >&2
    exit "${EXIT_USAGE}"
fi

cd -- "${repo_root}"

load_prelude=""
[ -n "${extension}" ] && load_prelude="LOAD '${extension}';"

run_sql() { "${binary}" -noheader -list -c "${load_prelude} $1"; }
say() { [ "${quiet}" -eq 1 ] || printf '%s\n' "$*"; }

# Expand a repo-relative glob to the files it matches. Deliberately not
# `ls $glob`: under `set -e -o pipefail` a non-matching glob kills the script
# mid-sweep with a status that looks like a guard firing but printed no report.
expand_glob() {
    local pattern="$1"
    local -a matches
    # shellcheck disable=SC2206  # unquoted expansion is the point
    matches=( $pattern )
    if [ "${#matches[@]}" -eq 1 ] && [ ! -e "${matches[0]}" ]; then
        return 0
    fi
    printf '%s\n' "${matches[@]}"
}

say "binary:  ${binary}"
say "repo:    ${repo_root}"
[ -n "${extension}" ] && say "extension: ${extension}"
say ""
say "law: ast_unparse_exact(x) == bytes(x)   |   contrast: ast_unparse(x) == bytes(x)"
say ""
say "$(printf '%-44s %-11s %-7s %-10s %-9s %-11s %s' \
    file language leaves gap_bytes exact rules_based note)"

files=()
harness_failures=0
for glob in "${globs[@]}"; do
    mapfile -t matched < <(expand_glob "${glob}")
    if [ "${#matched[@]}" -eq 0 ]; then
        printf 'HARNESS  %-48s glob matched no files on disk\n' "${glob}"
        harness_failures=$((harness_failures + 1))
        continue
    fi
    files+=("${matched[@]}")
done

exact_failures=0
vacuous_failures=0
rules_exact=0
n_checked=0
n_trivial=0
total_gap=0

for f in "${files[@]}"; do
    # One query per file: ast_unparse_exact errors on a multi-file table by
    # design (the textual law is stated over a single x), so a glob cannot be
    # swept in one statement.
    #
    # `leaves` and `gap_bytes` come from an independent read_ast scan, not from
    # the macro, so they measure the tree rather than restating the macro's own
    # arithmetic. gap_bytes = file size - sum of leaf span widths.
    out=$(run_sql "
        WITH b AS (SELECT content AS cblob, octet_length(content) AS nbytes
                   FROM read_blob('${f}')),
        n AS (SELECT * FROM read_ast('${f}', source := 'full', peek := 'none')),
        frontier AS (SELECT count(*) AS leaves,
                            sum(end_byte - start_byte)::BIGINT AS leaf_bytes,
                            any_value(language) AS lang
                     FROM n WHERE descendant_count = 0),
        x AS (SELECT source FROM ast_unparse_exact('${f}')),
        r AS (SELECT source FROM ast_unparse('${f}'))
        SELECT (SELECT count(*) FROM x),
               (SELECT lang FROM frontier),
               (SELECT leaves FROM frontier),
               (SELECT nbytes FROM b) - COALESCE((SELECT leaf_bytes FROM frontier), 0),
               (SELECT encode(source) FROM x) = (SELECT cblob FROM b),
               (SELECT encode(source) FROM r) = (SELECT cblob FROM b),
               (SELECT nbytes FROM b),
               COALESCE((SELECT leaf_bytes FROM frontier), 0);
    " 2>&1) || {
        printf 'ERROR    %-48s %s\n' "${f}" "$(printf '%s' "${out}" | head -1)"
        harness_failures=$((harness_failures + 1))
        continue
    }

    rows=$(echo "${out}" | cut -d'|' -f1)
    lang=$(echo "${out}" | cut -d'|' -f2)
    leaves=$(echo "${out}" | cut -d'|' -f3)
    gap=$(echo "${out}" | cut -d'|' -f4)
    exact=$(echo "${out}" | cut -d'|' -f5)
    rules=$(echo "${out}" | cut -d'|' -f6)
    nbytes=$(echo "${out}" | cut -d'|' -f7)
    leaf_bytes=$(echo "${out}" | cut -d'|' -f8)

    status=""
    if [ "${rows}" != "1" ]; then
        status="HARNESS: ${rows} output rows, expected 1"
        harness_failures=$((harness_failures + 1))
    fi
    # Guard 2: a vacuous equality.
    if [ "${nbytes}" = "0" ]; then
        # A zero-byte file: byte-exactness holds trivially and proves nothing.
        # A property of the fixture, not a failure -- counted and shown.
        status="trivial: zero-byte file"
        n_trivial=$((n_trivial + 1))
    elif [ "${leaves}" = "0" ] || [ "${leaves}" = "" ]; then
        status="VACUOUS: empty leaf frontier"
        vacuous_failures=$((vacuous_failures + 1))
    elif [ "${leaf_bytes}" -le 0 ] 2>/dev/null; then
        status="VACUOUS: every leaf is zero-width — the output is all gap"
        vacuous_failures=$((vacuous_failures + 1))
    else
        total_gap=$((total_gap + gap))
    fi
    [ "${exact}" = "true" ] || { exact_failures=$((exact_failures + 1)); status="NOT BYTE-EXACT"; }
    [ "${rules}" = "true" ] && rules_exact=$((rules_exact + 1))

    say "$(printf '%-44s %-11s %-7s %-10s %-9s %-11s %s' \
        "${f}" "${lang}" "${leaves}" "${gap}" "${exact}" "${rules}" "${status}")"
    n_checked=$((n_checked + 1))
done

# Corpus-level non-vacuity: if NO file had bytes outside its leaf frontier, the
# gap machinery -- the half of the splice that recovers whitespace and hidden
# delimiters -- was never exercised, whatever the per-file numbers say.
if [ "${n_checked}" -gt "${n_trivial}" ] && [ "${total_gap}" -le 0 ]; then
    say "VACUOUS: no file in the corpus had any bytes outside its leaf frontier"
    vacuous_failures=$((vacuous_failures + 1))
fi

say "----"
say "checked ${n_checked} files (${n_trivial} trivial/zero-byte) | byte-exact failures: ${exact_failures} | vacuous: ${vacuous_failures}"
say "gap bytes spliced across the corpus: ${total_gap} (bytes no leaf covers)"
say "rules-based ast_unparse() was byte-exact for ${rules_exact} of ${n_checked} (expected: 0 --"
say "  below source := 'full' only the structural law applies, #89)"
say ""

# =============================================================================
# NEGATIVE CONTROL -- can the comparison fail at all?
# =============================================================================
# Two planted faults, each a different way the splice could be wrong:
#   (a) every slice boundary shifted one byte late  -> boundary error
#   (b) one leaf's text replaced with 'ZZZZ'        -> content error
# Both must come out different from the file.
#
# The control is computed in HEX SPACE -- to_hex() of the BLOB, sliced with
# character-indexed substring() (two hex characters per byte, pure ASCII, so
# character indexing over the hex string IS byte indexing over the bytes; the
# spelling API_REFERENCE documents). Two reasons:
#
#   * A one-byte shift lands in the middle of a UTF-8 sequence, so a decode()
#     of the shifted pieces fails with a conversion error instead of producing
#     a wrong string. An error is not a detection: it would make the control
#     pass for the wrong reason on a fixture with no multi-byte characters and
#     blow up on one that has them. Hex has no such constraint.
#   * It cross-checks the two byte-slicing primitives. The UNSHIFTED hex splice
#     is computed too and must equal the file -- that is what makes a detected
#     difference attributable to the plant rather than to the control's own
#     arithmetic, and it independently confirms that hex slicing and the
#     macro's `blob[i:j]` agree.
say "negative control: a shifted boundary and a replaced leaf must both be detected"

control_file='test/data/encoding/crlf_utf8.py'
control=$(run_sql "
    WITH b AS (SELECT to_hex(content) AS hex FROM read_blob('${control_file}')),
    l AS (SELECT start_byte, end_byte, node_id,
                 COALESCE(lag(end_byte) OVER (ORDER BY start_byte, end_byte, node_id), 0) AS prev_end
          FROM read_ast('${control_file}', source := 'full', peek := 'none')
          WHERE descendant_count = 0),
    -- the control's own arithmetic, unshifted: must equal the file
    clean AS (
        SELECT string_agg(substring(b.hex, 2 * l.prev_end   + 1, 2 * (l.start_byte - l.prev_end)) ||
                          substring(b.hex, 2 * l.start_byte + 1, 2 * (l.end_byte - l.start_byte)),
                          '' ORDER BY l.start_byte, l.end_byte, l.node_id) AS head,
               max(l.end_byte) AS last_end
        FROM l, b),
    shifted AS (
        SELECT string_agg(substring(b.hex, 2 * (l.prev_end   + 1) + 1, 2 * (l.start_byte - l.prev_end)) ||
                          substring(b.hex, 2 * (l.start_byte + 1) + 1, 2 * (l.end_byte - l.start_byte)),
                          '' ORDER BY l.start_byte, l.end_byte, l.node_id) AS head,
               max(l.end_byte) AS last_end
        FROM l, b),
    replaced AS (
        SELECT string_agg(substring(b.hex, 2 * l.prev_end + 1, 2 * (l.start_byte - l.prev_end)) ||
                          CASE WHEN l.node_id = (SELECT min(node_id) FROM l WHERE end_byte > start_byte)
                               THEN '5A5A5A5A'
                               ELSE substring(b.hex, 2 * l.start_byte + 1, 2 * (l.end_byte - l.start_byte)) END,
                          '' ORDER BY l.start_byte, l.end_byte, l.node_id) AS head,
               max(l.end_byte) AS last_end
        FROM l, b)
    SELECT (SELECT count(*) FROM l),
           (SELECT c.head || substring(b.hex, 2 * c.last_end + 1) FROM clean    c, b) =  (SELECT hex FROM b),
           (SELECT s.head || substring(b.hex, 2 * s.last_end + 1) FROM shifted  s, b) <> (SELECT hex FROM b),
           (SELECT r.head || substring(b.hex, 2 * r.last_end + 1) FROM replaced r, b) <> (SELECT hex FROM b);
")
control_rows=$(echo "${control}" | cut -d'|' -f1)
clean_ok=$(echo "${control}" | cut -d'|' -f2)
shift_detected=$(echo "${control}" | cut -d'|' -f3)
replace_detected=$(echo "${control}" | cut -d'|' -f4)

control_failed=0
if [ "${control_rows}" = "0" ] || [ "${control_rows}" = "" ]; then
    say "  CONTROL VOID: the control query found no leaves in ${control_file}"
    control_failed=1
elif [ "${clean_ok}" != "true" ]; then
    say "  CONTROL VOID: the unshifted control splice does not match the file —"
    say "                the control's own arithmetic is wrong, so a detected"
    say "                difference would prove nothing"
    control_failed=1
elif [ "${shift_detected}" != "true" ]; then
    say "  CONTROL FAILED: a one-byte boundary shift was NOT detected"
    control_failed=1
elif [ "${replace_detected}" != "true" ]; then
    say "  CONTROL FAILED: a replaced leaf was NOT detected"
    control_failed=1
else
    say "  ok: unshifted control splice matches the file; both planted faults"
    say "      detected over ${control_rows} leaves (and hex slicing agrees with"
    say "      the macro's blob[i:j] on a CRLF + multi-byte fixture)"
fi
say ""

printf 'TOTAL files=%s | not_byte_exact=%s vacuous=%s harness=%s control=%s\n' \
    "${n_checked}" "${exact_failures}" "${vacuous_failures}" "${harness_failures}" \
    "$([ "${control_failed}" -eq 0 ] && echo ok || echo FAILED)"

# Harness and control first: if either tripped the other numbers prove nothing,
# and reporting them as a pass is the exact false green this script exists to
# prevent.
[ "${harness_failures}"  -eq 0 ] || { echo "VOID: a guard tripped; the comparison proved nothing" >&2; exit "${EXIT_HARNESS}"; }
[ "${control_failed}"    -eq 0 ] || { echo "VOID: negative control did not fire" >&2; exit "${EXIT_CONTROL}"; }
[ "${vacuous_failures}"  -eq 0 ] || { echo "VOID: a file's equality was vacuous" >&2; exit "${EXIT_VACUOUS}"; }
[ "${exact_failures}"    -eq 0 ] || { echo "FAIL: a file did not reproduce byte for byte" >&2; exit "${EXIT_EXACT}"; }

echo "PASS: ${n_checked} files reproduced byte for byte by ast_unparse_exact"
exit "${EXIT_OK}"
