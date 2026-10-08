#!/usr/bin/env bash
#
# conformance_kit.sh -- prove that a language module delivers what it declares.
#
# WHY THIS EXISTS
# ---------------
# tracker/features/044-v2-m1-contract.md, M1 deliverable 3:
#
#     "Conformance kit -- generalize the #91 pattern: every module proves call
#      nodes are named (#name binds), modifiers/signatures populate per
#      declared capabilities, raw-AST-join agreement, and no silent-empty
#      (#89). Out-of-tree modules run the same kit."
#
# and docs/planning/v2-architecture.md, "The contract":
#
#     "A module either passes conformance or it isn't a module.
#      Out-of-tree/third-party modules run the same kit."
#
# v2.0 splits this repo in three (M3) and moves language modules out, possibly
# out of tree. Once that happens, a module's claims are only worth what is
# CHECKABLE about them. This script is the checker, written against the current
# single-repo layout on purpose: it costs nothing to run today, and it is the
# thing that notices when files start moving.
#
# test/sql/bugs/issue_88_89_callee_name_and_guards.test (issue #91) is the
# pattern being generalized. That test hand-enumerates five grammars and pins
# their rows. This script asks the same questions of EVERY registered language,
# takes the language list from the runtime rather than a hardcoded array, and
# reports a language x check matrix instead of a single exit code -- because
# the expected state today is "most languages pass most checks, some
# legitimately do not", and one boolean cannot say that.
#
# CAPABILITY-AWARE, NOT UNIFORM
# -----------------------------
# 27 languages do not support the same things. `json` has no calls; `c` has no
# modifiers; `markdown` has no functions. A kit that demanded everything of
# everyone would either fail constantly or get watered down until it proved
# nothing. So every check is gated on a DECLARED capability, and a language
# that does not declare one is reported `n/a`, not failed.
#
# Where the declarations come from, in order of preference:
#
#   RUNTIME (out-of-tree safe) -- ast_type_map(<lang>) returns one row per
#       DEF_TYPE entry with its `name_strategy`. A language declares CALL
#       NAMING iff some call-semantic node type has a name strategy other than
#       'none'. This needs nothing but a loaded extension, so it works for a
#       module that lives in another repo entirely.
#
#   DEF-DERIVED (in-tree only) -- the `native_extraction` argument of
#       DEF_TYPE, scanned out of src/language_configs/*_types.def and the
#       adapter translation units that inline their tables (sql_adapter.cpp).
#       This is the ONLY source for the signature/parameter/modifier
#       capability, because ast_type_map() does not surface `native_strategy`.
#       See "KNOWN GAP" below.
#
#   OVERRIDE -- --force-capability LANG:CAP (or LANG:!CAP to negate). Exists
#       for two reasons: an out-of-tree module with no reachable .def can
#       declare its capabilities explicitly, and the negative controls below
#       need to plant a false declaration reproducibly. Every row whose
#       capability came from an override is marked `*` in the matrix so an
#       override can never quietly manufacture a pass.
#
#   UNDECLARED -- neither source could answer. The row is UNDECL, counted and
#       visible, and is neither a pass nor a fail.
#
# KNOWN GAP (an M1 finding, not a bug in this script)
# ---------------------------------------------------
# There is no capability declaration surface in the extension.
# ast_supported_languages() returns (language, extensions, parser_type,
# node_type_count) and nothing else; nothing anywhere in src/ declares what a
# language module supports. ast_type_map() exposes `name_strategy` but NOT the
# `native_strategy` that drives signature/parameter/modifier extraction, so the
# signature capability is not derivable at run time at all -- only by reading
# the .def sources, which an out-of-tree module may not ship. Adding
# native_strategy to ast_type_map() (or a dedicated capability surface) is what
# would make this script fully out-of-tree-capable on the native-context axis.
#
# EVERY CHECK MUST BE ABLE TO FAIL
# --------------------------------
# A conformance kit that passes everything proves nothing, and this repo has
# been burned twice by checks that could not fail (a sweep that reported 6
# breaks of which 5 were harness artifacts; a comparison that would have said
# "identical" against the same binary on both sides). So every check ships a
# negative control that plants a fault and requires the check to notice:
#
#   PARSE          control/broken.py is unparseable Python; the guard must see
#                  its ERROR nodes and VOID that row.
#   CALL-NAMED     control/unnamed_calls.sh is pure `$(...)` substitution, so
#                  no call node can carry a callee name; a language forced to
#                  declare call naming over it must FAIL.
#   NATIVE-ABSENT  python is forced to NOT declare native extraction
#                  (python:!native) while its corpus demonstrably produces
#                  parameters -- the undeclared-must-be-absent direction must
#                  FAIL.
#   RAW-JOIN       the raw self-join for callee A is compared against the
#                  selector for callee B; the set comparison must disagree.
#   NO-SILENT-     each must-error probe is re-run against a nonexistent file,
#   EMPTY          so it still errors but with the WRONG message; a kit that
#                  scores that as a pass cannot tell a guard firing from any
#                  other error, and the run is void.
#   DECLARED-VS-   json is forced to declare call naming although its
#   RUNTIME        ast_type_map() has no call-naming node type; the
#                  declaration-integrity check must FAIL.
#
# A control that does not fire makes the run VOID, not clean, and the verdict
# below checks harness guards and controls BEFORE it looks at any result.
#
# PARSE IS A GUARD, NOT A CHECK
# -----------------------------
# "The fixture produced nodes and no ERROR nodes" is a harness guard. A fixture
# that trips it makes that language's whole row VOID (fixture), reported
# separately from FAIL -- because a fixture that does not parse is a broken
# harness, and reporting it as a conformance defect is exactly the artifact
# this repo has already been bitten by.
#
set -euo pipefail

# ---------------------------------------------------------------- exit codes
# Distinct codes because "a module is non-conformant", "the harness compared
# nothing" and "a control did not fire" demand different responses, and a bare
# `exit 1` cannot tell CI which one happened.
readonly EXIT_OK=0
readonly EXIT_USAGE=1
readonly EXIT_HARNESS=2    # a guard tripped: the run is void, not clean
readonly EXIT_FAIL=3       # a conformance check failed
readonly EXIT_CONTROL=4    # a negative control did not fire

# ---------------------------------------------------------------- defaults
# Resolved from the script's own location, never from $PWD: read_ast globs and
# corpus paths that resolve to "whatever directory you happened to be in" are
# paths that silently sweep nothing.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)

binary="${repo_root}/build/release/duckdb"
extension=""
corpus="${repo_root}/test/data/conformance/corpus.tsv"
matrix_csv=""
languages_filter=""
quiet=0
controls_only=0
declare -a force_caps=()
declare -a def_sources=()

# DEF_TYPE tables live in the .def files plus the handful of adapters that
# inline their table (sql_adapter.cpp). Both are scanned; the language name
# comes from the file's own basename prefix, and the result is intersected with
# ast_supported_languages() so a .def with no registered adapter (yaml, fsharp,
# haskell, julia, scala today) cannot invent a phantom language.
readonly DEFAULT_DEF_SOURCES=(
    "${repo_root}/src/language_configs/*_types.def"
    "${repo_root}/src/language_adapters/*_adapter.cpp"
)

# The native-extraction strategies that amount to "this node type declares a
# function-shaped signature", i.e. the ones whose whole job is to populate
# parameters / signature_type / modifiers. From NativeExtractionStrategy in
# src/include/node_config.hpp.
readonly FUNCTION_NATIVE_STRATEGIES='FUNCTION_WITH_PARAMS|FUNCTION_WITH_DECORATORS|ARROW_FUNCTION|ASYNC_FUNCTION|GENERIC_FUNCTION|METHOD_DEFINITION|CONSTRUCTOR_DEFINITION'

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Runs the v2.0 M1 conformance kit (tracker/features/044-v2-m1-contract.md
deliverable 3) against every language the loaded build registers, and prints a
language x check matrix.

Checks (each gated on a DECLARED capability; see the header for where
declarations come from):
  PARSE        harness guard -- the fixture yields nodes and no ERROR nodes.
               A failure VOIDs that language's row; it is not a conformance
               verdict.
  CALL-NAMED   every call-semantic node carries a callee name (#88/#91).
               Gated on the runtime call-naming declaration.
  NATIVE-DECL  a language declaring a function-shaped native strategy produces
               at least one function definition with parameters.
               Gated on the def-derived native declaration.
  NATIVE-ABST  node types whose declared native strategy is NONE produce NO
               native payload. The direction with no "untyped code" excuse: if
               the .def says NONE and the engine emits parameters anyway, the
               declaration is a lie.
  RAW-JOIN     ast_select()'s semantic view set-equals the raw read_ast
               parent/descendant self-join, for both \`.call#X\` and
               \`.func:has(.call#X)\`, with X chosen from the corpus.
  NO-EMPTY     #89. A selector that cannot be answered ERRORS (with the
               expected message); one that is merely not satisfied returns 0
               rows cleanly.
  DECL-RUNTIME declaration integrity: an overridden or def-derived capability
               must not contradict what ast_type_map() shows at run time.

Verdicts: ok | FAIL | n/a (capability not declared) | UNDECL (no declaration
source could answer) | void (no fixture, or the fixture did not parse).
A \`*\` suffix marks a verdict reached under a --force-capability override.

Options:
  --binary PATH          duckdb binary to use
                         (default: ${binary})
  --extension PATH       sitting_duck extension to LOAD
                         (omit when the build links it statically)
  --corpus FILE          corpus index TSV: <language><TAB><path>, paths
                         relative to the FILE's own directory
                         (default: ${corpus})
  --def-source GLOB      where to scan DEF_TYPE tables for the native-strategy
                         declaration. Repeatable. Defaults to this repo's
                         language_configs/ and language_adapters/.
                         Pass --def-source '' to scan nothing and see what the
                         kit can still conclude from the runtime alone -- which
                         is what an out-of-tree module gets.
  --languages LIST       comma-separated subset to check (default: every
                         language ast_supported_languages() reports)
  --force-capability S   LANG:CAP forces a capability on; LANG:!CAP forces it
                         off. CAP is one of: call_names, native.
                         Repeatable. Overridden rows are marked \`*\`.
  --matrix-csv FILE      also write the matrix as CSV
  --controls-only        run only the negative controls and exit
  -q, --quiet            only print the matrix and the verdict
  -h, --help             this text

Exit codes:
  ${EXIT_OK}  every declared capability was delivered
  ${EXIT_USAGE}  usage error
  ${EXIT_HARNESS}  a harness guard tripped -- the run is VOID, not clean
  ${EXIT_FAIL}  a conformance check failed
  ${EXIT_CONTROL}  a negative control did not fire -- the run is VOID

The \`duckdb\` language adapter wraps DuckDB's own parser rather than
tree-sitter. It is expected to fail several checks (see #197); that is a result,
not a harness problem.
EOF
}

while [ $# -gt 0 ]; do
    case "$1" in
        --binary)            binary="${2:?--binary needs a path}"; shift 2 ;;
        --extension)         extension="${2:?--extension needs a path}"; shift 2 ;;
        --corpus)            corpus="${2:?--corpus needs a path}"; shift 2 ;;
        --def-source)        def_sources+=("${2-}"); shift 2 ;;
        --languages)         languages_filter="${2:?--languages needs a list}"; shift 2 ;;
        --force-capability)  force_caps+=("${2:?--force-capability needs LANG:CAP}"); shift 2 ;;
        --matrix-csv)        matrix_csv="${2:?--matrix-csv needs a path}"; shift 2 ;;
        --controls-only)     controls_only=1; shift ;;
        -q|--quiet)          quiet=1; shift ;;
        -h|--help)           usage; exit "${EXIT_OK}" ;;
        --)                  shift; break ;;
        *)                   echo "unknown option: $1" >&2; usage >&2; exit "${EXIT_USAGE}" ;;
    esac
done

if [ ! -x "${binary}" ]; then
    echo "not an executable duckdb binary: ${binary}" >&2
    echo "(build it with: GEN=ninja CMAKE_BUILD_PARALLEL_LEVEL=10 make release)" >&2
    exit "${EXIT_USAGE}"
fi
if [ -n "${extension}" ] && [ ! -f "${extension}" ]; then
    echo "no such extension file: ${extension}" >&2
    exit "${EXIT_USAGE}"
fi
if [ ! -f "${corpus}" ]; then
    echo "no such corpus index: ${corpus}" >&2
    exit "${EXIT_USAGE}"
fi
corpus_dir=$(cd -- "$(dirname -- "${corpus}")" && pwd)

if [ ${#def_sources[@]} -eq 0 ]; then
    def_sources=("${DEFAULT_DEF_SOURCES[@]}")
fi

workdir=$(mktemp -d "${TMPDIR:-/tmp}/conformance_kit.XXXXXX")
trap 'rm -rf -- "${workdir}"' EXIT

say()  { [ "${quiet}" -eq 1 ] || printf '%s\n' "$*"; }
warn() { printf '%s\n' "$*" >&2; }

load_prelude=""
[ -n "${extension}" ] && load_prelude="LOAD '${extension}';"

# Runs one SQL statement, returns pipe-separated rows. Any DuckDB error is a
# hard stop for the caller to interpret: the callers that EXPECT an error use
# run_sql_expect_error instead, so an unexpected error here is never silently
# read as an empty result.
run_sql() {
    "${binary}" -noheader -list -c "${load_prelude} $1"
}

# Runs one SQL statement that is expected to FAIL. Prints the error text on
# success-of-failing; prints nothing and returns 1 if the statement succeeded.
# This is the #89 primitive: "the engine must say so" is only testable if the
# harness can tell an error from a result.
run_sql_expect_error() {
    local out rc
    set +e
    out=$("${binary}" -noheader -list -c "${load_prelude} $1" 2>&1)
    rc=$?
    set -e
    if [ "${rc}" -eq 0 ]; then
        return 1
    fi
    printf '%s' "${out}"
    return 0
}

sql_quote() { printf "%s" "$1" | sed "s/'/''/g"; }

# =============================================================================
# STEP 1 -- the language list, from the RUNTIME
# =============================================================================
# 044 requires the kit to run against an out-of-tree module, so the language
# list must come from ast_supported_languages() and never from an array in this
# file. A hardcoded list would silently stop checking a language the moment one
# was added, removed or compiled out (-DSITTING_DUCK_LANGUAGES).
runtime_langs_raw=$(run_sql "SELECT language FROM ast_supported_languages() ORDER BY language;")
if [ -z "${runtime_langs_raw}" ]; then
    warn "VOID: ast_supported_languages() returned no rows -- is the extension loaded?"
    exit "${EXIT_HARNESS}"
fi
mapfile -t runtime_langs <<<"${runtime_langs_raw}"

declare -A want_lang=()
if [ -n "${languages_filter}" ]; then
    IFS=',' read -r -a _filter <<<"${languages_filter}"
    for l in "${_filter[@]}"; do want_lang["${l}"]=1; done
    for l in "${_filter[@]}"; do
        found=0
        for r in "${runtime_langs[@]}"; do [ "${r}" = "${l}" ] && found=1; done
        if [ "${found}" -eq 0 ]; then
            warn "no such registered language: ${l}"
            exit "${EXIT_USAGE}"
        fi
    done
fi

declare -a langs=()
for l in "${runtime_langs[@]}"; do
    if [ -n "${languages_filter}" ] && [ -z "${want_lang[${l}]+x}" ]; then continue; fi
    langs+=("${l}")
done

# =============================================================================
# STEP 2 -- capability declarations
# =============================================================================

# ---- 2a. RUNTIME: call naming, from ast_type_map()'s name_strategy ----------
# A language declares call naming iff at least one call-semantic node type has
# a name strategy other than 'none'. semantic_type_code() turns ast_type_map's
# semantic type NAME back into the byte that is_semantic_type() filters on, so
# the CALL set here is the same set read_ast rows are filtered by below --
# no second, drifting definition of "a call".
declare -A cap_call_runtime=()
while IFS='|' read -r lang n_call n_named; do
    [ -z "${lang}" ] && continue
    cap_call_runtime["${lang}"]="${n_call}:${n_named}"
done < <(run_sql "
    SELECT language,
           count(*) FILTER (WHERE is_semantic_type(semantic_type_code(semantic_type), 'CALL')),
           count(*) FILTER (WHERE is_semantic_type(semantic_type_code(semantic_type), 'CALL')
                              AND name_strategy <> 'none')
    FROM ast_type_map()
    GROUP BY language ORDER BY language;
")

# ---- 2b. DEF-DERIVED: native extraction, from the DEF_TYPE tables -----------
# Emits <language>\t<node_type>\t<name_strategy>\t<native_strategy>, one row
# per DEF_TYPE entry, for the whole scanned tree. Parsed from the RIGHT: the
# last four comma-separated fields are semantic_type, name_strategy,
# native_strategy, flags, and none of those four can contain a comma -- whereas
# the quoted raw_type CAN (DEF_TYPE(",", PARSER_PUNCTUATION, ...) is a real
# entry in most .def files). Splitting from the left would silently mis-field
# those rows.
def_table="${workdir}/def_table.tsv"
: >"${def_table}"
def_files_scanned=0
for pattern in "${def_sources[@]}"; do
    [ -z "${pattern}" ] && continue
    # shellcheck disable=SC2206  # unquoted expansion is the point
    matches=( $pattern )
    for f in "${matches[@]}"; do
        [ -f "${f}" ] || continue
        base=$(basename -- "${f}")
        lang="${base%%_types.def}"
        [ "${lang}" = "${base}" ] && lang="${base%%_adapter.cpp}"
        [ "${lang}" = "${base}" ] && continue
        def_files_scanned=$((def_files_scanned + 1))
        awk -v lang="${lang}" '
            /DEF_TYPE[ \t]*\(/ {
                line = $0
                sub(/[ \t\r]+$/, "", line)
                p = index(line, "DEF_TYPE")
                if (p == 0) next
                open_paren = index(substr(line, p), "(")
                if (open_paren == 0) next
                start = p + open_paren
                if (substr(line, length(line), 1) != ")") next
                inner = substr(line, start, length(line) - start)
                n = split(inner, f, ",")
                if (n < 5) next
                raw = f[1]
                for (i = 2; i <= n - 4; i++) raw = raw "," f[i]
                gsub(/^[ \t]*"|"[ \t]*$/, "", raw)
                nm = f[n-2]; nat = f[n-1]
                gsub(/^[ \t]+|[ \t]+$/, "", nm)
                gsub(/^[ \t]+|[ \t]+$/, "", nat)
                # The macro definition itself (DEF_TYPE(raw_type, semantic_type,
                # name_strat, native_strat, flags)) looks like an entry; its
                # raw_type is the unquoted parameter name, so the quote strip
                # above leaves it as-is and this catches it.
                if (raw == "raw_type") next
                printf "%s\t%s\t%s\t%s\n", lang, raw, nm, nat
            }
        ' "${f}" >>"${def_table}"
    done
done

def_rows=$(wc -l <"${def_table}" | tr -d ' ')

# Which languages the def scan could speak for at all, and whether each
# declares a function-shaped native strategy.
declare -A cap_native_def=()
declare -A def_seen=()
while IFS='|' read -r lang total fnnative; do
    [ -z "${lang}" ] && continue
    def_seen["${lang}"]=1
    cap_native_def["${lang}"]="${total}:${fnnative}"
done < <(awk -F'\t' -v re="^(${FUNCTION_NATIVE_STRATEGIES})$" '
    { total[$1]++; if ($4 ~ re) fn[$1]++ }
    END { for (l in total) printf "%s|%s|%s\n", l, total[l], (l in fn ? fn[l] : 0) }
' "${def_table}" | sort)

# ---- 2c. OVERRIDES ----------------------------------------------------------
declare -A override=()
for spec in "${force_caps[@]}"; do
    case "${spec}" in
        *:*) ;;
        *) warn "--force-capability needs LANG:CAP, got: ${spec}"; exit "${EXIT_USAGE}" ;;
    esac
    olang="${spec%%:*}"; ocap="${spec#*:}"
    oval=1
    if [ "${ocap#\!}" != "${ocap}" ]; then oval=0; ocap="${ocap#\!}"; fi
    case "${ocap}" in
        call_names|native) ;;
        *) warn "unknown capability in --force-capability: ${ocap}"; exit "${EXIT_USAGE}" ;;
    esac
    override["${olang}:${ocap}"]="${oval}"
done

# Resolved capability for (lang, cap): prints "<0|1|?> <source>".
# `?` means UNDECLARED -- no source could answer, which is a reportable state
# and deliberately neither a pass nor a fail.
resolve_cap() {
    local lang="$1" cap="$2"
    if [ -n "${override[${lang}:${cap}]+x}" ]; then
        printf '%s override' "${override[${lang}:${cap}]}"
        return
    fi
    case "${cap}" in
        call_names)
            if [ -n "${cap_call_runtime[${lang}]+x}" ]; then
                local n_named="${cap_call_runtime[${lang}]#*:}"
                printf '%s runtime' "$([ "${n_named}" -gt 0 ] && echo 1 || echo 0)"
            else
                printf '? none'
            fi ;;
        native)
            if [ -n "${cap_native_def[${lang}]+x}" ]; then
                local fnnative="${cap_native_def[${lang}]#*:}"
                printf '%s def' "$([ "${fnnative}" -gt 0 ] && echo 1 || echo 0)"
            else
                printf '? none'
            fi ;;
    esac
}

# =============================================================================
# STEP 3 -- the corpus
# =============================================================================
# Paths resolve against the corpus file's own directory so a module can ship
# its own conformance/ dir and be checked from anywhere.
declare -A corpus_files=()
corpus_rows=0
while IFS=$'\t' read -r clang cpath; do
    case "${clang}" in ''|'#'*) continue ;; esac
    [ -z "${cpath}" ] && continue
    abs="${corpus_dir}/${cpath}"
    if [ ! -f "${abs}" ]; then
        warn "HARNESS: corpus row '${clang}' points at a missing file: ${cpath}"
        corpus_rows=-1000000
        continue
    fi
    corpus_files["${clang}"]="${corpus_files[${clang}]-}${abs}"$'\n'
    corpus_rows=$((corpus_rows + 1))
done <"${corpus}"

if [ "${corpus_rows}" -le 0 ]; then
    warn "VOID: the corpus index yielded no usable fixtures (${corpus})"
    exit "${EXIT_HARNESS}"
fi

# Build a DuckDB list literal of absolute paths for one language.
#
# Once check_parse has run it prefers clean_paths[lang] -- the fixtures that
# parsed with no ERROR node. Downstream checks must never measure a fixture the
# guard quarantined: an ERROR subtree shifts node ids, loses names and would
# make every number after it a guess.
paths_list() {
    local lang="$1" out="" p src
    src="${clean_paths[${lang}]-}"
    [ -z "${src}" ] && src="${corpus_files[${lang}]-}"
    while IFS= read -r p; do
        [ -z "${p}" ] && continue
        out="${out}${out:+, }'$(sql_quote "${p}")'"
    done <<<"${src}"
    printf '[%s]' "${out}"
}

first_path() {
    local lang="$1" src
    src="${clean_paths[${lang}]-}"
    [ -z "${src}" ] && src="${corpus_files[${lang}]-}"
    printf '%s' "${src}" | head -1
}

# =============================================================================
# The checks
# =============================================================================
# Each returns one of: ok | FAIL | n/a | UNDECL | void, plus a detail string.
# A check NEVER returns ok on an empty measurement: "nothing to compare" is
# `void`, which is reported apart from `ok` and cannot be read as a pass.

# ---- guard: PARSE -----------------------------------------------------------
# Nodes > 0 and zero ERROR nodes. A harness guard, not a conformance verdict:
# a fixture that does not parse says nothing about the module.
# Reports PER FIXTURE and QUARANTINES the dirty ones rather than voiding the
# whole language: one fixture that does not parse should cost that fixture's
# coverage, not every verdict for its language. (test/data/kotlin/simple.kt has
# a single ERROR node at line 27 -- that is a real finding about the kotlin
# grammar, and it should not also erase kotlin's six other verdicts.)
# Side effect: sets clean_paths[lang] to the fixtures downstream checks may use.
declare -A clean_paths=()
check_parse() {
    local lang="$1" plist out nodes errs files clean row p
    plist=$(paths_list "${lang}")
    # coalesce(nullif(file_path, ''), ...): the `duckdb` adapter returns an
    # EMPTY file_path for every node (it wraps DuckDB's own parser and does not
    # thread the path through). Keying the quarantine on file_path without this
    # dropped every duckdb row and reported "no nodes" for a language that
    # parses fine -- a harness artifact that would have been read as a defect.
    # When the path is unavailable the quarantine cannot be per-fixture, so it
    # degrades to language granularity, which is stated rather than hidden.
    local unknown='(file_path not reported)'
    if ! out=$(run_sql "
        SELECT coalesce(nullif(file_path, ''), '$(sql_quote "${unknown}")'),
               count(*), count(*) FILTER (WHERE type = 'ERROR' OR type = 'MISSING')
        FROM read_ast(${plist}, '$(sql_quote "${lang}")', ignore_errors := true)
        GROUP BY 1 ORDER BY 1;
    " 2>&1); then
        printf 'void\tparse raised: %s' "$(printf '%s' "${out}" | head -1)"
        return
    fi
    nodes=0; errs=0; files=0; clean=""
    local dirty="" pathless=0
    while IFS='|' read -r p n e; do
        [ -z "${p}" ] && continue
        files=$((files + 1)); nodes=$((nodes + n)); errs=$((errs + e))
        if [ "${p}" = "${unknown}" ]; then
            pathless=1
            [ "${e}" -eq 0 ] && clean="${corpus_files[${lang}]-}"
            [ "${e}" -gt 0 ] && dirty="${dirty}${dirty:+, }${unknown}:${e}"
            continue
        fi
        if [ "${e}" -eq 0 ]; then
            clean="${clean}${p}"$'\n'
        else
            dirty="${dirty}${dirty:+, }$(basename -- "${p}"):${e}"
        fi
    done <<<"${out}"
    clean_paths["${lang}"]="${clean}"
    local pnote=""
    [ "${pathless}" -eq 1 ] && pnote="; file_path not reported by this adapter, quarantine is language-wide"
    if [ "${files}" -eq 0 ] || [ "${nodes}" -eq 0 ]; then
        printf 'void\tno nodes'
    elif [ -z "${clean}" ]; then
        printf 'void\tno fixture parsed cleanly (%s)' "${dirty}"
    elif [ "${errs}" -gt 0 ]; then
        # `dirty`, deliberately NOT `FAIL`: a fixture that will not parse is
        # ambiguous between a bad fixture and a grammar gap, and this repo has
        # already been burned by reporting harness artifacts as breaks. It is
        # counted and named, it does not set the exit code, and the remaining
        # clean fixtures still earn their language a verdict.
        printf 'dirty\t%s ERROR/MISSING nodes of %s; quarantined %s%s' "${errs}" "${nodes}" "${dirty}" "${pnote}"
    else
        printf 'ok\t%s nodes in %s fixtures%s' "${nodes}" "${files}" "${pnote}"
    fi
}

# ---- CALL-NAMED (#88 / #91) -------------------------------------------------
# Requirement: 044 "every module proves call nodes are named (#name binds)".
# A call node with no name is #89's silent empty: `.call#foo` over it returns
# nothing and looks like a real answer.
# plist_override (arg 2) exists so the negative control can run THIS function --
# not a copy of its predicate -- over a fixture whose calls cannot be named.
check_call_named() {
    local lang="$1" plist_override="${2-}" plist out total named cap capval capsrc
    cap=$(resolve_cap "${lang}" call_names); capval="${cap%% *}"; capsrc="${cap##* }"
    [ "${capval}" = "?" ] && { printf 'UNDECL\tno declaration source'; return; }
    [ "${capval}" = "0" ] && { printf 'n/a\tdoes not declare call naming (%s)' "${capsrc}"; return; }
    plist=$(paths_list "${lang}")
    [ -n "${plist_override}" ] && plist="${plist_override}"
    # THE GATE IS PER NODE TYPE, not per language -- this is what "per declared
    # capabilities" means in practice, and a language-level gate gets it wrong
    # in both directions. hcl declares FIND_CALL_TARGET on one call node type
    # and NONE on `template_interpolation`; a language-level gate reported hcl's
    # legitimately-unnamed interpolations as a failure. Conversely a language
    # with one naming type among many would pass on the strength of the one.
    # So each call node is judged against ITS OWN node type's declared
    # name_strategy, read from ast_type_map() at run time.
    #
    # `NOT is_syntax_only(flags)` is also load-bearing. javascript and
    # typescript both map the bare `new` KEYWORD TOKEN to COMPUTATION_CALL:
    #     DEF_TYPE("new", COMPUTATION_CALL, NONE, NONE, ASTNodeFlags::IS_KEYWORD)
    # and IS_KEYWORD is IS_SYNTAX_ONLY. A keyword token is not a call site and
    # cannot carry a callee name. (That the same quirk makes `new` double-count
    # against `new_expression` under `.call` is a separate, real finding --
    # reported, not papered over here.)
    #
    # When the capability is FORCED on, the per-node-type gate is dropped and
    # every call node must be named: that is what asserting the capability over
    # the module's own declaration means, and it is what lets the negative
    # control exercise this function rather than a copy of its predicate.
    local gate="AND tm.name_strategy <> 'none'"
    [ "${capsrc}" = "override" ] && gate=""
    out=$(run_sql "
        WITH tm AS (SELECT node_type, name_strategy FROM ast_type_map('$(sql_quote "${lang}")')),
        calls AS (
            SELECT type, name FROM read_ast(${plist}, '$(sql_quote "${lang}")', ignore_errors := true)
            WHERE is_semantic_type(semantic_type, 'CALL') AND NOT is_syntax_only(flags)
        ),
        syn AS (
            SELECT count(*) AS n FROM read_ast(${plist}, '$(sql_quote "${lang}")', ignore_errors := true)
            WHERE is_semantic_type(semantic_type, 'CALL') AND is_syntax_only(flags)
        ),
        judged AS (
            SELECT c.type, c.name FROM calls c JOIN tm ON tm.node_type = c.type ${gate}
        )
        SELECT (SELECT count(*) FROM judged),
               (SELECT count(*) FROM judged WHERE name IS NOT NULL AND name <> ''),
               (SELECT count(*) FROM calls) - (SELECT count(*) FROM judged),
               (SELECT n FROM syn),
               coalesce((SELECT string_agg(DISTINCT type, ', ') FROM judged
                         WHERE name IS NULL OR name = ''), '');
    ")
    total=$(printf '%s' "${out}" | cut -d'|' -f1)
    named=$(printf '%s' "${out}" | cut -d'|' -f2)
    local ungated synth offenders
    ungated=$(printf '%s' "${out}" | cut -d'|' -f3)
    synth=$(printf '%s' "${out}" | cut -d'|' -f4)
    offenders=$(printf '%s' "${out}" | cut -d'|' -f5)
    local note=""
    [ "${ungated}" -gt 0 ] && note="${note}; ${ungated} call nodes whose type declares no naming (excluded)"
    [ "${synth}" -gt 0 ]  && note="${note}; ${synth} syntax-only call-classified tokens (excluded)"
    if [ "${total}" -eq 0 ]; then
        printf 'void\tno naming-declared call nodes in corpus%s' "${note}"
    elif [ "${named}" -lt "${total}" ]; then
        printf 'FAIL\t%s of %s naming-declared call nodes unnamed [%s]%s' \
            "$((total - named))" "${total}" "${offenders}" "${note}"
    else
        printf 'ok\t%s/%s naming-declared call nodes named%s' "${named}" "${total}" "${note}"
    fi
}

# ---- NATIVE-DECL: declared => delivered -------------------------------------
# Requirement: 044 "modifiers/signatures populate per declared capabilities".
# A language whose .def gives some node type a function-shaped native strategy
# claims to extract a signature there; it must produce at least one function
# definition with a parameter list somewhere in its corpus.
check_native_declared() {
    local lang="$1" plist out fns withparams cap capval capsrc
    cap=$(resolve_cap "${lang}" native); capval="${cap%% *}"; capsrc="${cap##* }"
    [ "${capval}" = "?" ] && { printf 'UNDECL\tno .def reachable for this language'; return; }
    [ "${capval}" = "0" ] && { printf 'n/a\tdeclares no function-shaped native strategy (%s)' "${capsrc}"; return; }
    plist=$(paths_list "${lang}")
    # The SOURCE-SIDE GUARD is what keeps this check from reporting a fixture
    # gap as a defect. "No function definition carries parameters" is the right
    # verdict only if some function in the corpus TAKES a parameter in the
    # first place -- `auto greet() -> std::string` and `void main()` legitimately
    # deliver nothing, and an earlier revision of this script reported exactly
    # that for cpp and dart as conformance failures. It was wrong both times.
    #
    # The guard reads the first parenthesised group of the node's own peek and
    # asks whether it contains a word character. That is deliberately crude and
    # deliberately language-agnostic: it only ever decides `void` vs `FAIL`,
    # never `ok` vs `FAIL`, so a miss costs coverage, not correctness.
    #
    # And the FAIL threshold is "delivered NOTHING", not "delivered fewer than
    # the guard counted": `int main(void)` has a word character between its
    # parens and no parameters, so a ratio test would manufacture noise.
    local src_params
    if ! out=$(run_sql "
        WITH fns AS (
            SELECT parameters,
                   regexp_extract(coalesce(peek, ''), '^[^(]*\((.*?)\)', 1) AS first_parens
            FROM read_ast(${plist}, '$(sql_quote "${lang}")', context := 'native',
                          peek := 'full', ignore_errors := true)
            WHERE is_function_definition(semantic_type) AND NOT is_syntax_only(flags)
        )
        SELECT count(*),
               count(*) FILTER (WHERE regexp_matches(first_parens, '[A-Za-z_]')),
               count(*) FILTER (WHERE parameters IS NOT NULL AND len(parameters) > 0)
        FROM fns;
    " 2>&1); then
        printf 'void\tnative read raised: %s' "$(printf '%s' "${out}" | head -1)"
        return
    fi
    fns=$(printf '%s' "${out}" | cut -d'|' -f1)
    src_params=$(printf '%s' "${out}" | cut -d'|' -f2)
    withparams=$(printf '%s' "${out}" | cut -d'|' -f3)
    if [ "${fns}" -eq 0 ]; then
        printf 'void\tno function definitions in corpus'
    elif [ "${src_params}" -eq 0 ]; then
        printf 'void\t%s function defs, none takes a parameter in source (fixture gap)' "${fns}"
    elif [ "${withparams}" -eq 0 ]; then
        printf 'FAIL\t%s of %s function defs take parameters in source, 0 delivered' "${src_params}" "${fns}"
    else
        printf 'ok\t%s/%s function defs carry parameters (%s take one in source)' \
            "${withparams}" "${fns}" "${src_params}"
    fi
}

# ---- NATIVE-ABST: undeclared => absent --------------------------------------
# The other half of "per declared capabilities", and the half with no excuse.
# All-NULL signature/modifiers on a DECLARED node type is legitimate (untyped
# code -- #89 deliberately does not guard it, and #91 says so). But a node type
# the .def declares as native_strategy NONE must produce NOTHING: if the engine
# emits parameters or modifiers there anyway, the declaration is a lie and the
# capability means nothing.
# table_override (arg 2) exists so the negative control can run THIS function
# against a PLANTED declaration table -- the fault belongs in the declaration,
# which is the input this check is supposed to be sensitive to.
check_native_absent() {
    local lang="$1" table_override="${2-}" plist out checked leaked declnone
    local tbl="${table_override:-${def_table}}"
    if [ -z "${def_seen[${lang}]+x}" ]; then
        printf 'UNDECL\tno .def reachable for this language'
        return
    fi
    declnone=$(awk -F'\t' -v l="${lang}" '$1==l && $4=="NONE"' "${tbl}" | wc -l | tr -d ' ')
    if [ "${declnone}" -eq 0 ]; then
        printf 'void\tno node type declares native_strategy NONE'
        return
    fi
    plist=$(paths_list "${lang}")
    # `HAVING count(DISTINCT native_strategy) = 1` restricts this to node types
    # whose declaration is UNAMBIGUOUS. Several .def files declare the same
    # raw_type twice with different native strategies -- ruby_types.def has 21
    # such raw_types, including `class` (CLASS_WITH_METHODS at :71, NONE at
    # :593) and `singleton_method` (FUNCTION_WITH_PARAMS at :98, NONE at :902).
    # Only one of each survives into the adapter's node_configs map, so reading
    # the NONE variant as "the declaration" and then reporting the payload as a
    # leak would blame the extractor for an ambiguous .def. The ambiguity itself
    # is a finding, and DECL-UNIQUE below is the check that reports it.
    if ! out=$(run_sql "
        WITH decl AS (
            SELECT column2 AS node_type
            FROM read_csv('$(sql_quote "${tbl}")', delim := '\t', header := false,
                          columns := {'column1': 'VARCHAR', 'column2': 'VARCHAR',
                                      'column3': 'VARCHAR', 'column4': 'VARCHAR'})
            WHERE column1 = '$(sql_quote "${lang}")'
            GROUP BY column2
            HAVING count(DISTINCT column4) = 1 AND any_value(column4) = 'NONE'
        ),
        nodes AS (
            SELECT type, signature_type, parameters, modifiers
            FROM read_ast(${plist}, '$(sql_quote "${lang}")', context := 'native', ignore_errors := true)
        )
        SELECT count(*),
               count(*) FILTER (WHERE n.signature_type IS NOT NULL
                                   OR (n.parameters IS NOT NULL AND len(n.parameters) > 0)
                                   OR (n.modifiers  IS NOT NULL AND len(n.modifiers)  > 0))
        FROM nodes n JOIN decl d ON d.node_type = n.type;
    " 2>&1); then
        printf 'void\tjoin raised: %s' "$(printf '%s' "${out}" | head -1)"
        return
    fi
    checked=$(printf '%s' "${out}" | cut -d'|' -f1)
    leaked=$(printf '%s' "${out}" | cut -d'|' -f2)
    if [ "${checked}" -eq 0 ]; then
        printf 'void\tno NONE-declared node type appears in corpus'
    elif [ "${leaked}" -gt 0 ]; then
        printf 'FAIL\t%s of %s NONE-declared nodes carry native payload' "${leaked}" "${checked}"
    else
        printf 'ok\t%s NONE-declared nodes, none carry payload' "${checked}"
    fi
}

# ---- RAW-JOIN ---------------------------------------------------------------
# Requirement: 044 "raw-AST-join agreement" -- the semantic view and the raw
# tree must agree. Generalizes the #91 test's hand-written C assertion:
#   ast_select(f, '.call#X')            == raw read_ast call filter
#   ast_select(f, '.func:has(.call#X)') == raw parent/descendant self-join
# X is picked FROM THE CORPUS (the most frequent named callee), not hardcoded,
# so this works for a language nobody wrote a test for.
# selector_b, when non-empty, is substituted for X on the ast_select side only:
# that is the negative control, and it must make the comparison disagree.
check_raw_join() {
    local lang="$1" selector_b="${2-}" f callee out a b c d
    f=$(first_path "${lang}")
    [ -z "${f}" ] && { printf 'void\tno fixture'; return; }
    callee=$(run_sql "
        SELECT name FROM read_ast('$(sql_quote "${f}")', '$(sql_quote "${lang}")', ignore_errors := true)
        WHERE is_semantic_type(semantic_type, 'CALL') AND name IS NOT NULL AND name <> ''
          AND regexp_matches(name, '^[A-Za-z_][A-Za-z0-9_]*\$')
        GROUP BY name ORDER BY count(*) DESC, name LIMIT 1;
    ")
    [ -z "${callee}" ] && { printf 'void\tno named call in fixture'; return; }
    local sel_callee="${callee}"
    [ -n "${selector_b}" ] && sel_callee="${selector_b}"
    # `language := ${lang}` on BOTH sides is load-bearing. ast_select()
    # auto-detects the language from the extension when it is not told, so
    # without this the `duckdb` row compared the tree-sitter `sql` grammar's
    # semantic view against the native DuckDB parser's raw tree and reported
    # the disagreement as a conformance failure. Two adapters is not a
    # semantic-vs-raw comparison at all.
    if ! out=$(run_sql "
        WITH sel_call AS (
            SELECT name, start_line FROM ast_select('$(sql_quote "${f}")', '.call#$(sql_quote "${sel_callee}")',
                                                    language := '$(sql_quote "${lang}")')
        ),
        raw_call AS (
            SELECT name, start_line FROM read_ast('$(sql_quote "${f}")', '$(sql_quote "${lang}")')
            WHERE is_semantic_type(semantic_type, 'CALL') AND name = '$(sql_quote "${callee}")'
        ),
        sel_fn AS (
            SELECT DISTINCT name, start_line
            FROM ast_select('$(sql_quote "${f}")', '.func:has(.call#$(sql_quote "${sel_callee}"))',
                            language := '$(sql_quote "${lang}")')
        ),
        raw_fn AS (
            SELECT DISTINCT fn.name, fn.start_line
            FROM read_ast('$(sql_quote "${f}")', '$(sql_quote "${lang}")') fn
            JOIN read_ast('$(sql_quote "${f}")', '$(sql_quote "${lang}")') c
              ON c.node_id > fn.node_id
             AND c.node_id <= fn.node_id + fn.descendant_count
             AND is_semantic_type(c.semantic_type, 'CALL')
             AND c.name = '$(sql_quote "${callee}")'
            WHERE is_function_definition(fn.semantic_type) AND NOT is_syntax_only(fn.flags)
        )
        SELECT (SELECT count(*) FROM raw_call),
               (SELECT count(*) FROM ((SELECT * FROM sel_call EXCEPT SELECT * FROM raw_call)
                                      UNION ALL
                                      (SELECT * FROM raw_call EXCEPT SELECT * FROM sel_call))),
               (SELECT count(*) FROM raw_fn),
               (SELECT count(*) FROM ((SELECT * FROM sel_fn EXCEPT SELECT * FROM raw_fn)
                                      UNION ALL
                                      (SELECT * FROM raw_fn EXCEPT SELECT * FROM sel_fn)));
    " 2>&1); then
        printf 'void\tcomparison raised: %s' "$(printf '%s' "${out}" | head -1)"
        return
    fi
    a=$(printf '%s' "${out}" | cut -d'|' -f1)
    b=$(printf '%s' "${out}" | cut -d'|' -f2)
    c=$(printf '%s' "${out}" | cut -d'|' -f3)
    d=$(printf '%s' "${out}" | cut -d'|' -f4)
    # Guard: with no raw call rows the comparison proves nothing, so this is
    # void rather than a pass. (The #91 convention: never let an empty
    # comparison read as agreement.)
    if [ "${a}" -eq 0 ]; then
        printf 'void\tcallee #%s matched 0 raw rows' "${callee}"
    elif [ "${b}" -ne 0 ] || [ "${d}" -ne 0 ]; then
        printf 'FAIL\t#%s: call diff=%s, :has diff=%s (raw %s calls, %s funcs)' \
            "${callee}" "${b}" "${d}" "${a}" "${c}"
    else
        printf 'ok\t#%s: %s calls, %s enclosing funcs agree' "${callee}" "${a}" "${c}"
    fi
}

# ---- NO-EMPTY (#89) ---------------------------------------------------------
# Requirement: the repo's stated engine principle 1 -- "when the engine can't
# honor a query's semantics, it says so; 0 rows always means 'searched
# correctly, not there'". Both directions are checked, because a guard that
# fires too eagerly breaks the clean-zero promise just as badly as a missing
# guard breaks the loud-failure one:
#
#   must-error    five selectors the engine documents as unanswerable (unknown
#                 attribute, unknown pseudo-element, two documented-but-
#                 unimplemented operator combinations, unsupported :has
#                 argument). Each must raise AND the message must match -- an
#                 error for any other reason (a typo'd path, say) is not a
#                 guard firing, and scoring it as one is how a kit stops being
#                 able to tell the difference.
#   clean-zero    `.call#<absent>` over a source whose calls ARE named must
#                 return 0 rows with no error. A guard that fires here instead
#                 breaks "0 rows means searched correctly, not there" just as
#                 badly as a missing guard breaks the other direction.
#   cannot-answer `.call#<absent>` over a source whose call nodes carry NO
#                 names must ERROR. This is the #88/#89 pairing itself, and the
#                 per-language direction of the check.
#
# NOTE on probe choice: `[type^=fun]` is NOT usable as a probe. #91 rejected
# `^= $= *=` on `type`, but those operators were implemented afterwards and the
# selector now works -- verified against this build. `language` and `modifier`
# are the attributes that still only implement a subset, so they are the live
# guards. A probe list that drifts out of date is a kit that quietly stops
# checking, which is why each probe asserts its MESSAGE and not just a failure.
#
# bad_path, when set, makes every must-error probe point at a nonexistent file:
# the probes still error, but with the wrong message. That is this check's
# negative control.
readonly -a EMPTY_PROBES=(
    '.func[frobnicate=x]|unknown attribute'
    '.func::bogus_pseudo|pseudo-element'
    '.func[language^=py]|only supports exact match'
    '.func[modifier^=asy]|supports = and *='
    '.func:has(.call[name=x])|attribute filters inside :has'
)
check_no_empty() {
    local lang="$1" bad_path="${2-}" f probe sel expect err errs=0 oks=0 msg
    f=$(first_path "${lang}")
    [ -z "${f}" ] && { printf 'void\tno fixture'; return; }
    local target="${f}"
    [ -n "${bad_path}" ] && target="${bad_path}"
    for probe in "${EMPTY_PROBES[@]}"; do
        sel="${probe%%|*}"; expect="${probe##*|}"
        if ! err=$(run_sql_expect_error "
            SELECT count(*) FROM ast_select('$(sql_quote "${target}")', '$(sql_quote "${sel}")',
                                            language := '$(sql_quote "${lang}")');
        "); then
            errs=$((errs + 1)); msg="${msg-}${sel} did not error; "
            continue
        fi
        if ! printf '%s' "${err}" | grep -qiF -- "${expect}"; then
            errs=$((errs + 1))
            msg="${msg-}${sel} errored without '${expect}'; "
            continue
        fi
        oks=$((oks + 1))
    done
    # clean-zero / cannot-answer directions. Which one applies is decided by
    # the fixture, not by this script: a source whose calls are named owes a
    # clean 0; a source whose call nodes carry no name owes an error.
    # (Skipped when the control has redirected the probes to a bogus path.)
    local zero_note="skipped"
    if [ -z "${bad_path}" ]; then
        local calls named out
        out=$(run_sql "
            SELECT count(*) FILTER (WHERE is_semantic_type(semantic_type, 'CALL')),
                   count(*) FILTER (WHERE is_semantic_type(semantic_type, 'CALL')
                                      AND name IS NOT NULL AND name <> '')
            FROM read_ast('$(sql_quote "${f}")', '$(sql_quote "${lang}")', ignore_errors := true);
        ")
        calls=$(printf '%s' "${out}" | cut -d'|' -f1)
        named=$(printf '%s' "${out}" | cut -d'|' -f2)
        local probe_sql="SELECT count(*) FROM ast_select('$(sql_quote "${f}")', '.call#zz_absent_callee_xyz',
                                                        language := '$(sql_quote "${lang}")');"
        if [ "${named}" -gt 0 ]; then
            set +e
            out=$("${binary}" -noheader -list -c "${load_prelude} ${probe_sql}" 2>&1)
            local rc=$?
            set -e
            if [ "${rc}" -ne 0 ]; then
                errs=$((errs + 1)); zero_note="clean-zero ERRORED (over-eager guard)"
            elif [ "${out}" != "0" ]; then
                errs=$((errs + 1)); zero_note="clean-zero returned '${out}'"
            else
                zero_note="clean-zero ok"
            fi
        elif [ "${calls}" -gt 0 ]; then
            if err=$(run_sql_expect_error "${probe_sql}"); then
                if printf '%s' "${err}" | grep -qiF -- 'no call node in this source carries a callee name'; then
                    zero_note="cannot-answer errored ok"
                else
                    errs=$((errs + 1)); zero_note="cannot-answer errored for the wrong reason"
                fi
            else
                errs=$((errs + 1)); zero_note="cannot-answer returned rows SILENTLY (#89 violation)"
            fi
        else
            zero_note="no call nodes; neither direction applies"
        fi
    fi
    if [ "${errs}" -gt 0 ]; then
        printf 'FAIL\t%s/%s guards fired; %s; %s' "${oks}" "${#EMPTY_PROBES[@]}" "${zero_note}" "${msg-}"
    else
        printf 'ok\t%s/%s guards fired, %s' "${oks}" "${#EMPTY_PROBES[@]}" "${zero_note}"
    fi
}

# ---- DECL-UNIQUE: one node type, one declaration ----------------------------
# Requirement: 044's "modifiers/signatures populate per DECLARED capabilities"
# presupposes that a node type HAS a declaration -- singular. A DEF_TYPE table
# is loaded into an unordered_map keyed by raw_type, so declaring the same
# raw_type twice with different strategies silently drops one of them: the
# surviving behaviour depends on map insertion order, not on anything written
# down. That is a contract hole independent of any extractor bug, and it is
# invisible at run time because ast_type_map() reports only the survivor.
#
# Duplicates that AGREE are harmless redundancy and are reported, not failed.
# Only a CONFLICTING duplicate fails.
#
# table_override (arg 2) lets the negative control plant a conflicting
# duplicate and run this function over it.
check_decl_unique() {
    local lang="$1" table_override="${2-}" out dups conflicts offenders
    local tbl="${table_override:-${def_table}}"
    if [ -z "${def_seen[${lang}]+x}" ] && [ -z "${table_override}" ]; then
        printf 'UNDECL\tno .def reachable for this language'
        return
    fi
    out=$(awk -F'\t' -v l="${lang}" '
        $1 == l {
            n[$2]++
            key = $3 "|" $4
            if (!($2 SUBSEP key in seen)) { seen[$2 SUBSEP key] = 1; variants[$2]++ }
        }
        END {
            d = 0; c = 0; names = ""
            for (t in n) if (n[t] > 1) {
                d++
                if (variants[t] > 1) { c++; names = names (names == "" ? "" : ", ") t }
            }
            printf "%d|%d|%s\n", d, c, names
        }
    ' "${tbl}")
    dups=$(printf '%s' "${out}" | cut -d'|' -f1)
    conflicts=$(printf '%s' "${out}" | cut -d'|' -f2)
    offenders=$(printf '%s' "${out}" | cut -d'|' -f3)
    if [ "${conflicts}" -gt 0 ]; then
        # Trim the offender list: ruby has 21 and the matrix detail should stay
        # readable. The count is the assertion; the names are a pointer.
        local shown; shown=$(printf '%s' "${offenders}" | cut -c1-90)
        printf 'FAIL\t%s raw_types declared more than once with CONFLICTING strategies [%s%s]' \
            "${conflicts}" "${shown}" "$([ "${#offenders}" -gt 90 ] && echo ' ...')"
    elif [ "${dups}" -gt 0 ]; then
        printf 'ok\t%s redundant (but agreeing) duplicate declarations' "${dups}"
    else
        printf 'ok\tevery raw_type declared exactly once'
    fi
}

# ---- DECL-RUNTIME: declaration integrity ------------------------------------
# What makes "declared capability" mean anything. A capability asserted by an
# override (or, for call naming, by anything other than the runtime) must not
# contradict what ast_type_map() shows. Without this, --force-capability would
# be a way to declare anything and a manifest would be a way to pick winners.
check_decl_runtime() {
    local lang="$1" cap capval capsrc n_call n_named
    cap=$(resolve_cap "${lang}" call_names); capval="${cap%% *}"; capsrc="${cap##* }"
    if [ -z "${cap_call_runtime[${lang}]+x}" ]; then
        printf 'UNDECL\tast_type_map() has no rows for this language'
        return
    fi
    n_call="${cap_call_runtime[${lang}]%%:*}"
    n_named="${cap_call_runtime[${lang}]#*:}"
    if [ "${capsrc}" != "override" ]; then
        printf 'ok\truntime: %s call node types, %s named\n' "${n_call}" "${n_named}" | tr -d '\n'
        return
    fi
    if [ "${capval}" = "1" ] && [ "${n_named}" -eq 0 ]; then
        printf 'FAIL\toverride declares call naming; ast_type_map() shows %s call node types, 0 named' "${n_call}"
    elif [ "${capval}" = "0" ] && [ "${n_named}" -gt 0 ]; then
        printf 'FAIL\toverride denies call naming; ast_type_map() shows %s named call node types' "${n_named}"
    else
        printf 'ok\toverride agrees with ast_type_map() (%s named of %s)' "${n_named}" "${n_call}"
    fi
}

# =============================================================================
# STEP 4 -- run the matrix
# =============================================================================
readonly -a CHECK_NAMES=(PARSE CALL-NAMED NATIVE-DECL NATIVE-ABST RAW-JOIN NO-EMPTY DECL-UNIQUE DECL-RUNTIME)

declare -A verdict=() detail=()
n_fail=0; n_void=0; n_undecl=0; n_na=0; n_ok=0; n_nofixture=0; n_dirty=0

run_one_language() {
    local lang="$1" r
    if [ -z "${corpus_files[${lang}]+x}" ]; then
        for c in "${CHECK_NAMES[@]}"; do
            verdict["${lang}:${c}"]="void"; detail["${lang}:${c}"]="no fixture in corpus"
        done
        n_nofixture=$((n_nofixture + 1))
        return
    fi
    r=$(check_parse "${lang}")
    verdict["${lang}:PARSE"]="${r%%$'\t'*}"; detail["${lang}:PARSE"]="${r#*$'\t'}"
    if [ "${verdict[${lang}:PARSE]}" = "void" ]; then
        # Nothing parsed cleanly: nothing downstream can be read as a verdict.
        # DECL-UNIQUE is the exception -- it reads the declaration, not the
        # corpus, so a dead fixture says nothing about it.
        for c in CALL-NAMED NATIVE-DECL NATIVE-ABST RAW-JOIN NO-EMPTY DECL-RUNTIME; do
            verdict["${lang}:${c}"]="void"; detail["${lang}:${c}"]="no fixture parsed cleanly"
        done
        r=$(check_decl_unique "${lang}")
        verdict["${lang}:DECL-UNIQUE"]="${r%%$'\t'*}"; detail["${lang}:DECL-UNIQUE"]="${r#*$'\t'}"
        return
    fi
    r=$(check_call_named "${lang}")
    verdict["${lang}:CALL-NAMED"]="${r%%$'\t'*}"; detail["${lang}:CALL-NAMED"]="${r#*$'\t'}"
    r=$(check_native_declared "${lang}")
    verdict["${lang}:NATIVE-DECL"]="${r%%$'\t'*}"; detail["${lang}:NATIVE-DECL"]="${r#*$'\t'}"
    r=$(check_native_absent "${lang}")
    verdict["${lang}:NATIVE-ABST"]="${r%%$'\t'*}"; detail["${lang}:NATIVE-ABST"]="${r#*$'\t'}"
    r=$(check_raw_join "${lang}")
    verdict["${lang}:RAW-JOIN"]="${r%%$'\t'*}"; detail["${lang}:RAW-JOIN"]="${r#*$'\t'}"
    r=$(check_no_empty "${lang}")
    verdict["${lang}:NO-EMPTY"]="${r%%$'\t'*}"; detail["${lang}:NO-EMPTY"]="${r#*$'\t'}"
    r=$(check_decl_unique "${lang}")
    verdict["${lang}:DECL-UNIQUE"]="${r%%$'\t'*}"; detail["${lang}:DECL-UNIQUE"]="${r#*$'\t'}"
    r=$(check_decl_runtime "${lang}")
    verdict["${lang}:DECL-RUNTIME"]="${r%%$'\t'*}"; detail["${lang}:DECL-RUNTIME"]="${r#*$'\t'}"
}

if [ "${controls_only}" -eq 0 ]; then
    say "binary:    ${binary}"
    [ -n "${extension}" ] && say "extension: ${extension}"
    say "corpus:    ${corpus} (${corpus_rows} fixtures)"
    say "languages: ${#langs[@]} of ${#runtime_langs[@]} registered (from ast_supported_languages())"
    say "def scan:  ${def_rows} DEF_TYPE entries from ${def_files_scanned} files, ${#def_seen[@]} languages"
    if [ ${#force_caps[@]} -gt 0 ]; then
        say "overrides: ${force_caps[*]}"
    fi
    say ""

    for lang in "${langs[@]}"; do
        run_one_language "${lang}"
    done

    # ------------------------------------------------------------ the matrix
    printf '%-12s' "LANGUAGE"
    for c in "${CHECK_NAMES[@]}"; do printf '%-14s' "${c}"; done
    printf '\n'
    printf '%-12s' "------------"
    for c in "${CHECK_NAMES[@]}"; do printf '%-14s' "-------------"; done
    printf '\n'
    for lang in "${langs[@]}"; do
        printf '%-12s' "${lang}"
        for c in "${CHECK_NAMES[@]}"; do
            v="${verdict[${lang}:${c}]-?}"
            mark=""
            case "${c}" in
                CALL-NAMED|DECL-RUNTIME) [ -n "${override[${lang}:call_names]+x}" ] && mark="*" ;;
                NATIVE-DECL)             [ -n "${override[${lang}:native]+x}" ] && mark="*" ;;
            esac
            printf '%-14s' "${v}${mark}"
            case "${v}" in
                ok)     n_ok=$((n_ok + 1)) ;;
                FAIL)   n_fail=$((n_fail + 1)) ;;
                void)   n_void=$((n_void + 1)) ;;
                dirty)  n_dirty=$((n_dirty + 1)) ;;
                UNDECL) n_undecl=$((n_undecl + 1)) ;;
                n/a)    n_na=$((n_na + 1)) ;;
            esac
        done
        printf '\n'
    done
    printf '\n'

    # ------------------------------------------- details for anything not ok
    say "DETAIL (every cell that is not a plain pass)"
    for lang in "${langs[@]}"; do
        for c in "${CHECK_NAMES[@]}"; do
            v="${verdict[${lang}:${c}]-?}"
            [ "${v}" = "ok" ] && continue
            say "$(printf '  %-6s %-12s %-12s %s' "${v}" "${lang}" "${c}" "${detail[${lang}:${c}]-}")"
        done
    done
    say ""
    say "ALSO PASSING (for the record)"
    for lang in "${langs[@]}"; do
        for c in "${CHECK_NAMES[@]}"; do
            [ "${verdict[${lang}:${c}]-}" = "ok" ] || continue
            say "$(printf '  ok     %-12s %-12s %s' "${lang}" "${c}" "${detail[${lang}:${c}]-}")"
        done
    done
    say ""

    if [ -n "${matrix_csv}" ]; then
        {
            printf 'language,check,verdict,overridden,detail\n'
            for lang in "${langs[@]}"; do
                for c in "${CHECK_NAMES[@]}"; do
                    ov=0
                    case "${c}" in
                        CALL-NAMED|DECL-RUNTIME) [ -n "${override[${lang}:call_names]+x}" ] && ov=1 ;;
                        NATIVE-DECL)             [ -n "${override[${lang}:native]+x}" ] && ov=1 ;;
                    esac
                    printf '%s,%s,%s,%s,"%s"\n' "${lang}" "${c}" "${verdict[${lang}:${c}]-?}" "${ov}" \
                        "$(printf '%s' "${detail[${lang}:${c}]-}" | sed 's/"/""/g')"
                done
            done
        } >"${matrix_csv}"
        say "matrix CSV: ${matrix_csv}"
    fi
fi

# =============================================================================
# STEP 5 -- NEGATIVE CONTROLS
# =============================================================================
# Every check above reports its happy answer both when it is right AND when it
# compared nothing. These plant a fault and require each check to notice. A
# control that does not fire makes the whole run void.
control_failures=0
control_note() {
    local name="$1" result="$2" want="$3" got="$4"
    if [ "${result}" = "fired" ]; then
        say "$(printf '  ok       %-14s %s' "${name}" "${got}")"
    else
        say "$(printf '  NOTFIRED %-14s wanted %s, got %s' "${name}" "${want}" "${got}")"
        control_failures=$((control_failures + 1))
    fi
}

say "NEGATIVE CONTROLS (each plants a fault; the named check must notice)"

control_dir="${corpus_dir}/control"

# --- PARSE guard: an unparseable fixture must VOID, not pass ----------------
if [ -f "${control_dir}/broken.py" ]; then
    out=$(run_sql "
        SELECT count(*) FILTER (WHERE type = 'ERROR' OR type = 'MISSING'), count(*)
        FROM read_ast('$(sql_quote "${control_dir}/broken.py")', 'python', ignore_errors := true);
    ")
    cerr=$(printf '%s' "${out}" | cut -d'|' -f1)
    ctot=$(printf '%s' "${out}" | cut -d'|' -f2)
    if [ "${ctot}" -gt 0 ] && [ "${cerr}" -gt 0 ]; then
        control_note "PARSE" fired ">0 ERROR nodes" "${cerr} ERROR/MISSING of ${ctot} nodes -> row would be void"
    else
        control_note "PARSE" notfired ">0 ERROR nodes" "${cerr} ERROR of ${ctot} nodes"
    fi
else
    control_note "PARSE" notfired "control/broken.py" "missing"
fi

# --- CALL-NAMED: a forced declaration over an all-unnamed fixture must FAIL --
# Runs check_call_named itself: bash's own ast_type_map() shows 0 named call
# node types, so the capability is FORCED on (and the forcing is visible), and
# the fixture is pure `$(...)` substitution, so no call node can be named.
if [ -f "${control_dir}/unnamed_calls.sh" ]; then
    override[bash:call_names]=1
    r=$(check_call_named bash "['$(sql_quote "${control_dir}/unnamed_calls.sh")']")
    unset 'override[bash:call_names]'
    cv="${r%%$'\t'*}"
    if [ "${cv}" = "FAIL" ]; then
        control_note "CALL-NAMED" fired "unnamed call nodes" "${r#*$'\t'}"
    else
        control_note "CALL-NAMED" notfired "unnamed call nodes" "${cv}: ${r#*$'\t'}"
    fi
else
    control_note "CALL-NAMED" notfired "control/unnamed_calls.sh" "missing"
fi

# --- NATIVE-ABST: re-declare every python node type as native_strategy NONE --
# python demonstrably emits parameters on function_definition. Plant that in
# the DECLARATION table -- the input this check reads -- and run the real
# check: it has to see the payload leak onto NONE-declared node types.
planted_def="${workdir}/planted_def.tsv"
awk -F'\t' 'BEGIN{OFS="\t"} $1=="python" {print $1,$2,$3,"NONE"}' "${def_table}" >"${planted_def}"
if [ -s "${planted_def}" ] && [ -n "${corpus_files[python]+x}" ]; then
    r=$(check_native_absent python "${planted_def}")
    cv="${r%%$'\t'*}"
    if [ "${cv}" = "FAIL" ]; then
        control_note "NATIVE-ABST" fired "payload on NONE-declared nodes" "${r#*$'\t'}"
    else
        control_note "NATIVE-ABST" notfired "payload on NONE-declared nodes" "${cv}: ${r#*$'\t'}"
    fi
else
    control_note "NATIVE-ABST" notfired "python def rows + fixture" "missing"
fi

# --- RAW-JOIN: compare the raw join for callee A against the selector for B --
if [ -n "${corpus_files[python]+x}" ]; then
    r=$(check_raw_join python "zz_wrong_callee_for_control")
    cv="${r%%$'\t'*}"
    if [ "${cv}" = "FAIL" ]; then
        control_note "RAW-JOIN" fired "set disagreement" "${r#*$'\t'}"
    else
        control_note "RAW-JOIN" notfired "set disagreement" "${cv}: ${r#*$'\t'}"
    fi
else
    control_note "RAW-JOIN" notfired "python fixture" "missing"
fi

# --- NO-EMPTY: the probes must be scored on their MESSAGE, not just on -------
# "something failed". Point them at a nonexistent file: they still error, but
# for the wrong reason, and the check must not call that a pass.
if [ -n "${corpus_files[python]+x}" ]; then
    r=$(check_no_empty python "${workdir}/zz_no_such_fixture.py")
    cv="${r%%$'\t'*}"
    if [ "${cv}" = "FAIL" ]; then
        control_note "NO-EMPTY" fired "wrong-reason errors rejected" "${r#*$'\t'}"
    else
        control_note "NO-EMPTY" notfired "wrong-reason errors rejected" "${cv}: ${r#*$'\t'}"
    fi
else
    control_note "NO-EMPTY" notfired "python fixture" "missing"
fi

# --- DECL-UNIQUE: plant a conflicting duplicate declaration -----------------
# python declares every raw_type exactly once, so it passes this check cleanly.
# Append one extra row for an existing raw_type with a different native
# strategy and the real check has to see the conflict.
planted_dup="${workdir}/planted_dup.tsv"
awk -F'\t' 'BEGIN{OFS="\t"} $1=="python"' "${def_table}" >"${planted_dup}"
if [ -s "${planted_dup}" ]; then
    baseline_dup=$(check_decl_unique python "${planted_dup}")
    awk -F'\t' 'BEGIN{OFS="\t"} $1=="python" && $2=="function_definition" {
        print $1, $2, $3, ($4 == "NONE" ? "FUNCTION_WITH_PARAMS" : "NONE"); exit
    }' "${planted_dup}" >>"${planted_dup}"
    r=$(check_decl_unique python "${planted_dup}")
    cv="${r%%$'\t'*}"
    if [ "${cv}" = "FAIL" ] && [ "${baseline_dup%%$'\t'*}" = "ok" ]; then
        control_note "DECL-UNIQUE" fired "conflicting duplicate declaration" "${r#*$'\t'}"
    else
        control_note "DECL-UNIQUE" notfired "conflicting duplicate declaration" \
            "baseline ${baseline_dup%%$'\t'*}, planted ${cv}: ${r#*$'\t'}"
    fi
else
    control_note "DECL-UNIQUE" notfired "python def rows" "missing"
fi

# --- DECL-RUNTIME: a false declaration must be caught -----------------------
# json has no call-naming node type at all, so forcing the capability on it is
# a declaration the runtime contradicts.
override[json:call_names]=1
r=$(check_decl_runtime json)
unset 'override[json:call_names]'
cv="${r%%$'\t'*}"
if [ "${cv}" = "FAIL" ]; then
    control_note "DECL-RUNTIME" fired "runtime contradicts a forced declaration" "${r#*$'\t'}"
else
    control_note "DECL-RUNTIME" notfired "runtime contradicts a forced declaration" "${cv}: ${r#*$'\t'}"
fi
say ""

# =============================================================================
# VERDICT
# =============================================================================
# Controls first. If a control did not fire, every number above is meaningless
# and printing a green summary would be the exact false green this script
# exists to avoid.
if [ "${control_failures}" -ne 0 ]; then
    printf 'VOID: %s negative control(s) did not fire; the matrix proves nothing\n' "${control_failures}" >&2
    exit "${EXIT_CONTROL}"
fi

if [ "${controls_only}" -eq 1 ]; then
    echo "CONTROLS OK: every check demonstrably detects its planted fault"
    exit "${EXIT_OK}"
fi

printf 'TOTALS ok=%s FAIL=%s dirty=%s void=%s UNDECL=%s n/a=%s | languages=%s (no fixture: %s)\n' \
    "${n_ok}" "${n_fail}" "${n_dirty}" "${n_void}" "${n_undecl}" "${n_na}" "${#langs[@]}" "${n_nofixture}"

if [ "${n_fail}" -ne 0 ]; then
    printf 'FAIL: %s conformance check(s) failed\n' "${n_fail}" >&2
    exit "${EXIT_FAIL}"
fi

echo "PASS: every declared capability was delivered (${n_void} void, ${n_undecl} undeclared, ${n_na} n/a)"
exit "${EXIT_OK}"
