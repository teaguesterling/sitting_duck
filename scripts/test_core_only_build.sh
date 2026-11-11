#!/usr/bin/env bash
# Build sitting_duck with NO tree-sitter language compiled in, and prove the core
# SQL macros still work (M2 stage 3, tracker/features/057).
#
# WHY THIS EXISTS. M2 stage 3 split src/sql_macros into a `core` half and an
# `addon` half and gave nine core macros a `_from` variant that reads an AST out
# of a named TABLE instead of parsing a path. The claim being made is that the
# core half is language-agnostic. scripts/verify_macro_boundary.py asserts that
# statically over macro text; this script asserts it on a real binary, built with
#
#     -DSITTING_DUCK_LANGUAGES=duckdb
#
# which gives exactly one adapter -- the NATIVE duckdb one, which wraps DuckDB's
# own parser -- and ZERO tree-sitter parsers.
#
# WHY NOT scripts/test_language_subset.sh. That script is hardcoded to
# "python;cpp" and its whole point is the opposite assertion: that a compiled-out
# language errors like an unknown one. This one is about the macros, not the
# language registry.
#
# USAGE
#   scripts/test_core_only_build.sh [build_dir]        # configure, build, test
#   SKIP_BUILD=1 scripts/test_core_only_build.sh DIR   # test an existing build
#
# build_dir defaults to build/core-only. IT MUST NOT BE build/release: that is
# the full-language build the rest of the suite needs, and overwriting it with a
# one-language build would silently remove 27 languages from every other test.
# The script refuses that path when it is going to build.
#
# SKIP_BUILD=1 is how the guard is proven to discriminate. Point it at
# build/release and the language-list assertion must FAIL:
#     SKIP_BUILD=1 scripts/test_core_only_build.sh build/release
#
# ON A SLURM HOST, run the build through the queue rather than directly:
#     srun -n 1 -c 8 --mem=24G -p batch -t 300 \
#          --export=ALL,CMAKE_BUILD_PARALLEL_LEVEL=8 \
#          scripts/test_core_only_build.sh
set -euo pipefail

BUILD_DIR=${1:-build/core-only}
SKIP_BUILD=${SKIP_BUILD:-0}
REPO_ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$REPO_ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }
ok()   { echo "ok: $1"; }

DUCKDB="$BUILD_DIR/duckdb"
UNITTEST="$BUILD_DIR/test/unittest"

# -----------------------------------------------------------------------------
# 1. Configure and build
# -----------------------------------------------------------------------------
# The cmake flags are TAKEN FROM `make -n release` rather than written out here.
# The release configure line carries a dozen flags (vcpkg toolchain, the
# extension config, UNITTEST_ROOT_DIRECTORY, autoloading off) and it changes
# whenever extension-ci-tools is bumped. A copy pasted into this file would
# drift silently and the build would stop resembling the real one.
if [ "$SKIP_BUILD" = "1" ]; then
    echo "SKIP_BUILD=1: reusing the existing build in $BUILD_DIR"
else
    case "$BUILD_DIR" in
        build/release|build/release/|./build/release)
            fail "refusing to build into $BUILD_DIR: that is the full-language build the rest of the suite uses. Pass another directory, or SKIP_BUILD=1 to test it as-is."
            ;;
    esac

    #
    # MATCHED ON `-B build/release`, NOT on the flags that follow `cmake`. The
    # obvious pattern '^cmake *-D' is wrong and was MEASURED wrong before this
    # script ever ran in CI: with GEN=ninja, extension-ci-tools puts
    # `-G "Ninja" -DFORCE_COLORED_OUTPUT=1` FIRST, so the line reads
    # `cmake -G "Ninja" -D...` and that pattern matches ZERO lines. Every CI leg
    # here sets GEN: ninja, so the job would have failed on its first run with
    # "could not read the release configure line" and nothing else. `make -n
    # release` emits exactly two cmake lines, the configure and `cmake --build`,
    # and only the configure carries -B.
    CONFIGURE_LINE=$(make -n release 2>/dev/null | grep -m1 '^cmake .*-B build/release$')
    [ -n "$CONFIGURE_LINE" ] || fail "could not read the release configure line out of 'make -n release' (looked for a '^cmake ... -B build/release' line)"
    # Same flags, different build tree, one language.
    CONFIGURE_LINE=${CONFIGURE_LINE/-B build\/release/-B $BUILD_DIR}
    case "$CONFIGURE_LINE" in
        *"-B $BUILD_DIR"*) : ;;
        *) fail "did not find '-B build/release' in the configure line to redirect; got: $CONFIGURE_LINE" ;;
    esac

    echo "--- configure (one language: duckdb) ---"
    echo "$CONFIGURE_LINE -DSITTING_DUCK_LANGUAGES=duckdb"
    eval "$CONFIGURE_LINE -DSITTING_DUCK_LANGUAGES=duckdb"

    # POSITIVE EVIDENCE FROM THE CONFIGURE, not just exit 0. A configure that
    # silently enabled all 28 languages would also exit 0, so the generated
    # declaration table is read instead: it must hold the duckdb NATIVE entry
    # and no SD_BUILTIN_LANGUAGE_TS entry at all.
    GEN_DEF="$BUILD_DIR/extension/sitting_duck/sitting_duck_generated/sitting_duck_builtin_languages.def"
    [ -f "$GEN_DEF" ] || fail "cmake did not emit $GEN_DEF"
    #
    # ANCHORED AT THE START OF THE LINE, and that is not tidiness. The generated
    # file opens with a comment telling consumers to define
    # SD_BUILTIN_LANGUAGE_TS(name, AdapterClass) and
    # SD_BUILTIN_LANGUAGE_NATIVE(name, AdapterClass), so an unanchored grep
    # counts the instructions as entries: it reported 2 native and would have
    # reported 1 tree-sitter against a table holding one real native entry and
    # no tree-sitter entry at all. Measured, on the first run of this script.
    n_native=$(grep -c '^SD_BUILTIN_LANGUAGE_NATIVE(' "$GEN_DEF" || true)
    n_ts=$(grep -c '^SD_BUILTIN_LANGUAGE_TS(' "$GEN_DEF" || true)
    [ "$n_native" = "1" ] || fail "$GEN_DEF has $n_native native entries, expected 1"
    [ "$n_ts" = "0" ] || fail "$GEN_DEF has $n_ts tree-sitter entries, expected 0"
    grep -q '^SD_BUILTIN_LANGUAGE_NATIVE(duckdb,' "$GEN_DEF" \
        || fail "$GEN_DEF's one native entry is not duckdb"
    ok "generated language table: 1 native (duckdb), 0 tree-sitter"

    # The configure regenerates src/include/embedded_sql_macros.hpp INTO THE
    # SOURCE TREE. It must come back byte-identical under a restricted language
    # set: a diff here would mean the generator depends on the language
    # selection, which would be a defect in the stage's manifest work.
    if ! git diff --quiet -- src/include/embedded_sql_macros.hpp; then
        fail "configuring with one language changed src/include/embedded_sql_macros.hpp; the macro generator must not depend on the language selection"
    fi
    ok "embedded_sql_macros.hpp unchanged by a one-language configure"

    echo "--- build ---"
    cmake --build "$BUILD_DIR" --config Release
fi

[ -x "$DUCKDB" ]   || fail "no shell at $DUCKDB"
[ -x "$UNITTEST" ] || fail "no unittest binary at $UNITTEST"

run_sql() { "$DUCKDB" -no-agent -noheader -list -c "$1"; }

# Expect the SQL to error, with the message matching the given grep pattern.
expect_error() {
    local sql=$1 pattern=$2 label=$3 out
    if out=$("$DUCKDB" -no-agent -c "$sql" 2>&1); then
        fail "$label: expected an error, got success: $out"
    fi
    echo "$out" | grep -q "$pattern" || fail "$label: error did not match '$pattern': $out"
    ok "$label"
}

# -----------------------------------------------------------------------------
# 2. THE GUARD THAT STOPS THIS PASSING AGAINST A FULL BUILD
# -----------------------------------------------------------------------------
# Printed before it is asserted, so a failure shows what was actually there.
LANGS=$(run_sql "SELECT string_agg(language, ',' ORDER BY language) FROM ast_supported_languages();")
echo "--- compiled-in languages reported by $DUCKDB: [$LANGS] ---"
[ "$LANGS" = "duckdb" ] \
    || fail "ast_supported_languages() = '$LANGS', expected exactly 'duckdb'. This build has languages this test is supposed to prove it does not need."
ok "ast_supported_languages() lists exactly duckdb"

# ZERO TREE-SITTER PARSERS, said a second way. The list above could in principle
# be satisfied by a duckdb entry that was itself tree-sitter-backed, and the
# count below is over the same registry -- so this also pins that there is
# nothing else hiding behind an alias.
N_LANGS=$(run_sql "SELECT count(*) FROM ast_supported_languages();")
[ "$N_LANGS" = "1" ] || fail "ast_supported_languages() has $N_LANGS rows, expected 1"
# duckdb is the one language that claims no file extension, because it is
# selected explicitly rather than detected. That is the measured fact behind
# core_alone.test's single exemption (ast_unparse, which has no language
# parameter), so it is asserted here rather than left as a remark there.
N_EXT=$(run_sql "SELECT count(*) FROM (SELECT unnest(extensions) FROM ast_supported_languages());")
[ "$N_EXT" = "0" ] || fail "the compiled-in language set advertises $N_EXT file extensions, expected 0"
ok "no compiled-in language claims any file extension"

# -----------------------------------------------------------------------------
# 3. RESTRICTED-BUILD-ONLY ASSERTIONS
# -----------------------------------------------------------------------------
# These are the ones whose answer DEPENDS on the language set, which is exactly
# why they cannot live in core_alone.test -- that file is run against a full
# build too, to prove it is not vacuous.

# A tree-sitter language really is gone. Both halves: detection misses, and an
# explicit request errors.
DET=$(run_sql "SELECT coalesce(detect_language('x.py'), '<null>');")
[ "$DET" = "<null>" ] || fail "detect_language('x.py') = '$DET', expected NULL in a build with no python parser"
ok "detect_language misses .py"
expect_error "SELECT * FROM read_ast('test/data/python/sample_app.py', 'python');" \
    "Unsupported language: python" "read_ast with explicit language 'python' errors cleanly"
expect_error "SELECT * FROM read_ast('test/data/python/sample_app.py');" \
    "Could not detect language" "read_ast on a .py file errors like an unknown extension"

# THE FACT THE WHOLE FIXTURE APPROACH RESTS ON, measured on this binary rather
# than inferred from src/unparse_rules.cpp. That file #includes all 53 languages'
# *_unparse.def files unconditionally, so python's UNPARSE RULES are compiled in
# even though python's PARSER is not. If that were false, core_alone.test's
# ast_unparse_from digest would differ between the two builds and the test would
# be impossible to share -- so this is checked before the test runs, to give a
# clear message instead of a digest mismatch.
PY_RULES=$(run_sql "SELECT count(*) FROM ast_unparse_rules('python') WHERE language = 'python';")
[ "$PY_RULES" -gt 0 ] \
    || fail "ast_unparse_rules('python') has $PY_RULES python-specific rules in this build; the unparse rule map is supposed to be language-selection independent"
ok "unparse rules for python are compiled in without a python parser ($PY_RULES language-specific rules)"

# WHAT THIS BUILD DOES *NOT* SEPARATE, measured and printed rather than claimed.
# The language selection and the SQL macro set are independent: cmake regenerates
# the embedded macro header from the whole `sql_macros` manifest regardless of
# -DSITTING_DUCK_LANGUAGES. So BOTH sides of the core/add-on boundary are still
# registered here, and a core macro that reached an add-on macro would still
# succeed. This script proves "core without a tree-sitter language", not "core
# without the add-ons". Printed so the distinction is visible in a CI log.
N_MACROS=$(run_sql "SELECT count(*) FROM duckdb_functions() WHERE database_name = 'memory';")
echo "--- macros registered in this build: $N_MACROS (both sides of the core/add-on boundary) ---"
[ "$N_MACROS" -gt 0 ] || fail "duckdb_functions() reported $N_MACROS macros; the extension did not register"

# -----------------------------------------------------------------------------
# 4. COVERAGE GATE: every core macro must have a call site in core_alone.test
# -----------------------------------------------------------------------------
# The names are DERIVED from spec/layout/layout.yaml with verify_macro_boundary's
# own parser, never transcribed. A hand-written list drifts from the manifest in
# silence, and the manifest is what both the gate and the embedded-header
# generator read. So a core macro added in a later change fails HERE, instead of
# quietly never being exercised in a parser-free build.
echo "--- coverage gate: core macros vs call sites in test/sql/core_alone.test ---"
python3 -I - <<'PY'
import sys, re
from pathlib import Path
root = Path.cwd()
sys.path.insert(0, str(root / 'scripts'))
import verify_macro_boundary as vmb
import yaml

layout = root / 'spec' / 'layout' / 'layout.yaml'
manifest = yaml.safe_load(layout.read_text(encoding='utf-8'))['sql_macros']
owner, _body, empty = vmb.parse_macros(root / 'src' / 'sql_macros', manifest)
if empty:
    sys.exit(f"FAIL: no macro definitions found in {empty}; the parser is wrong, not the code")
side = {e['file']: e['side'] for e in manifest}
core = sorted(n for n in owner if side[owner[n]] == 'core')
if not core:
    sys.exit("FAIL: derived zero core macros; a gate that matches nothing proves nothing")

test = (root / 'test' / 'sql' / 'core_alone.test').read_text(encoding='utf-8')
# Strip `#` comment lines: a macro NAMED in prose is not a macro CALLED.
body = '\n'.join(l for l in test.splitlines() if not l.lstrip().startswith('#'))
missing = [n for n in core
           if not re.search(r'\b' + re.escape(n) + r'\s*\(', body)]
print(f"derived {len(core)} core macros from the manifest; "
      f"{len(core) - len(missing)} have a call site")
if missing:
    sys.exit("FAIL: these core macros have no call site in test/sql/core_alone.test: "
             + ', '.join(missing))
PY
ok "every core macro derived from the manifest is called in core_alone.test"

# -----------------------------------------------------------------------------
# 5. The test itself
# -----------------------------------------------------------------------------
# ONE path per invocation. Two .test arguments concatenate into a single bogus
# filter and the runner reports "No tests ran" at exit 0.
echo "--- test/sql/core_alone.test against $UNITTEST ---"
"$UNITTEST" test/sql/core_alone.test

echo "PASS: the core SQL macros run with no tree-sitter language compiled in"
