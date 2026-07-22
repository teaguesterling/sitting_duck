#!/usr/bin/env bash
# Smoke test for a language-subset build (see cmake/BuiltinLanguages.cmake and
# docs/development/contributing.md). Expects a build configured with
#   -DSITTING_DUCK_LANGUAGES="python;cpp"
# and verifies that:
#   1. ast_supported_languages() lists exactly the subset
#   2. a compiled-in language parses (python file, plus the "py" alias)
#   3. a compiled-out language fails exactly like an unknown one
#      (detect_language() misses; explicit language errors cleanly)
#
# Usage: scripts/test_language_subset.sh [path/to/duckdb]
set -euo pipefail

DUCKDB=${1:-build/release/duckdb}
WORKDIR=$(mktemp -d)
trap 'rm -rf "$WORKDIR"' EXIT

fail() {
    echo "FAIL: $1" >&2
    exit 1
}

run_sql() {
    "$DUCKDB" -noheader -list -c "$1"
}

# Expect the SQL to error, with the message matching the given grep pattern.
expect_error() {
    local sql=$1 pattern=$2 label=$3
    local out
    if out=$("$DUCKDB" -c "$sql" 2>&1); then
        fail "$label: expected an error, got success: $out"
    fi
    echo "$out" | grep -q "$pattern" || fail "$label: error did not match '$pattern': $out"
    echo "ok: $label"
}

cat > "$WORKDIR/hello.py" <<'EOF'
def hello(name):
    return f"hello {name}"
EOF

cat > "$WORKDIR/hello.go" <<'EOF'
package main

func main() {}
EOF

# 1. Supported languages reflect exactly the compiled subset
langs=$(run_sql "SELECT string_agg(language, ',' ORDER BY language) FROM ast_supported_languages();")
[ "$langs" = "cpp,python" ] || fail "ast_supported_languages() = '$langs', expected 'cpp,python'"
echo "ok: ast_supported_languages() lists exactly cpp,python"

# 2. Compiled-in language parses, by extension and by alias
count=$(run_sql "SELECT count(*) FROM read_ast('$WORKDIR/hello.py');")
[ "$count" -gt 0 ] || fail "read_ast on python file returned $count nodes"
echo "ok: read_ast parses python ($count nodes)"

count=$(run_sql "SELECT count(*) FROM parse_ast('def f(): pass', 'py');")
[ "$count" -gt 0 ] || fail "parse_ast with 'py' alias returned $count nodes"
echo "ok: parse_ast accepts the 'py' alias"

detected=$(run_sql "SELECT detect_language('$WORKDIR/hello.py');")
[ "$detected" = "python" ] || fail "detect_language(.py) = '$detected', expected 'python'"
echo "ok: detect_language maps .py to python"

# 3. Compiled-out language behaves exactly like an unknown language
detected=$(run_sql "SELECT coalesce(detect_language('$WORKDIR/hello.go'), '<null>');")
[ "$detected" = "<null>" ] || fail "detect_language(.go) = '$detected', expected NULL (detection miss)"
echo "ok: detect_language misses .go (compiled out)"

expect_error "SELECT * FROM read_ast('$WORKDIR/hello.go');" \
    "Could not detect language" "read_ast on a .go file errors like an unknown extension"

expect_error "SELECT * FROM read_ast('$WORKDIR/hello.go', 'go');" \
    "Unsupported language: go" "read_ast with explicit language 'go' errors cleanly"

expect_error "SELECT * FROM parse_ast('package main', 'go');" \
    "Unsupported language: go" "parse_ast with 'go' errors cleanly"

# Excluded language's extensions are gone from the extension map
exts=$(run_sql "SELECT count(*) FROM (SELECT unnest(extensions) FROM ast_supported_languages() WHERE language = 'go');")
[ "$exts" = "0" ] || fail "go still advertises extensions in ast_supported_languages()"

echo "PASS: language subset build behaves correctly"
