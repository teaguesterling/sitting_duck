PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=duckdb_ast
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile

############################
# Parser Generation Targets
############################
# These targets manage pre-generated tree-sitter parsers, allowing builds
# without tree-sitter CLI, Node.js, or Cargo dependencies.

.PHONY: generate-parsers clean-parsers regenerate-parsers

# Generate all tree-sitter parsers and store in generated_parsers/
# Requires: tree-sitter CLI, Node.js
generate-parsers:
	@echo "Generating tree-sitter parsers..."
	chmod +x scripts/generate_all_parsers.sh
	./scripts/generate_all_parsers.sh

# Clean the generated parsers directory
clean-parsers:
	@echo "Cleaning generated parsers..."
	rm -rf generated_parsers

# Regenerate parsers (clean first, then generate)
regenerate-parsers: clean-parsers generate-parsers

############################
# Dynamic Grammar Test Library
############################
# Builds a standalone tree-sitter grammar shared library from the pre-generated
# JSON parser. Used by test/sql/dynamic_languages/ to exercise register_language().
# The library is named .so on all platforms: dlopen() ignores the suffix, and a
# uniform name keeps the sqllogictests platform-independent.
# Run the dynamic language tests with:
#   make test-grammar-lib
#   SITTING_DUCK_TEST_GRAMMAR_DIR=build/test_grammars make test

TEST_GRAMMAR_DIR := build/test_grammars

.PHONY: test-grammar-lib
test-grammar-lib:
	mkdir -p $(TEST_GRAMMAR_DIR)
	cc -shared -fPIC -O2 \
		-I generated_parsers/tree-sitter-json/src \
		generated_parsers/tree-sitter-json/src/parser.c \
		-o $(TEST_GRAMMAR_DIR)/libjson_dyn.so

############################
# Test Runner Budget (DuckDB v2.0 line only)
############################
# DuckDB's v2.0 test runner (duckdb/scripts/ci/run_tests.py, which does not exist
# on the v1.5 line) enforces a per-batch timeout: 600s by default, and 300s once
# workers >= 10 — and workers default to "75%" of cores, so the budget silently
# shrinks on a larger runner.
#
# Four ast_select-heavy suites exceed that budget on the v2.0 line. They are not
# failing: duckdb#26036 regressed planning cost ~20x (~22s per ast_select call on
# v2.0 vs ~1s on v1.5.5), and the cost is compile-side — a 0-row table costs the
# same as a full one. All four PASS under this budget; measured against
# v2.0.0-dev85467 via run_tests.py, "1 passed" each:
#   test/sql/bugs/issue_88_89_callee_name_and_guards.test   25 calls   709s
#   test/sql/ast_select_combinator_steps.test               37 calls  1203s
#   test/sql/ast_select_pseudo_classes.test                 65 calls  2431s
#   test/sql/css_selectors_multilang.test                  122 calls  3168s
#
# 7200 rather than something nearer those numbers, because CI timing differs from
# a dev box in BOTH directions and the budget is a ceiling, not a delay — a
# generous value costs nothing while tests pass. Non-globbing suites may be
# FASTER on CI (css_selectors_multilang uses 11 single-file fixtures, no globs;
# CI has run ~19s/call where this box measures ~26-29s), while glob-heavy ones
# are slower on a cold checkout (ast_select_combinator_steps globs *.py, and that
# is the family whose 8-call chunks still blew a 600s budget in PR #178).
#
# Raising the budget keeps the coverage. The alternatives were worse: deleting or
# skipping the suites loses real v2.0 signal, and splitting them into per-file
# chunks proved to be whack-a-mole against a moving upstream number (PR #178 --
# 41/43 chunks passed, two still timed out because CI runs slower than a dev box).
#
# batch-size 1 gives every file its own budget. With the default batch of 10, one
# slow file times out the whole batch and the runner then re-runs each file in
# isolation, burning a full timeout before making progress.
#
# This is a no-op wherever the runner is absent (the v1.5 line, and older
# extension-ci-tools pins that predate TEST_RUNNER): TEST_RUNNER is empty there,
# so nothing is appended and the unittest binary is invoked directly as before.
#
# SCOPE: no in-repo job executes these flags today. duckdb-next-build is the only
# leg on ci_tools_version: main, and it carries skip_tests: true (#179), so the
# first real execution is a duckdb/community-extensions release PR, whose build.yml
# passes ci_tools_version: 'main'. That is the point — it makes the registry's
# test_against_latest leg completable, which is what lets a release pin
# ref_next. It does mean an upstream flag rename would surface in a release PR
# rather than here.
#
# RE-ENABLING CANARY TESTS is now gated on RUNTIME COST, not on duckdb#26036:
# this budget already makes all four suites pass. The four alone are ~7500s of
# serial work on a dev box, so dropping skip_tests trades a build-only canary for
# a multi-hour one. That is a deliberate call to make with a release, not a
# consequence of the upstream bug.
#
# REMOVE THIS once duckdb#26036 is fixed and v2.0 planning cost is back to v1.5
# levels — at that point the suites fit the stock budget and none of this is needed.

# --max-retries 0 is load-bearing, not tidiness. run_tests.py forces retry=2 when
# CI is set, and it cannot be turned off with --retry: `retry = max(0, args.retry)`
# then `if retry == 0 and os.environ.get("CI"): retry = 2`, so passing --retry 0 is
# indistinguishable from the default. --max-retries is the lever that works:
# can_retry() requires `retry_count < config.max_retries`, so 0 disables retries.
# Without it a batch that overruns the budget is attempted 3 times — 3 x 7200s =
# 21600s = exactly GitHub's 6h default job cap, which _extension_distribution.yml
# never overrides. Build and test are steps of the SAME job, so the cap is shared
# with a full DuckDB build and would be hit mid-retry: GitHub cancels the leg and
# the test report is lost. A clean "timeout after 7200s for <file>" is strictly
# more useful than a cancelled 6h job, and retrying a deterministic budget
# overrun cannot succeed anyway.
#
# Each flag is guarded on a non-empty value so that `make test_release
# TEST_BATCH_TIMEOUT=` opts out cleanly instead of emitting a bare --batch-timeout
# and failing argparse ("expected one argument"). ?= alone does not do this: an
# explicitly-empty override stays defined-but-empty.

TEST_BATCH_TIMEOUT ?= 7200
TEST_BATCH_SIZE ?= 1
TEST_MAX_RETRIES ?= 0

ifneq ($(TEST_RUNNER),)
ifneq ($(strip $(TEST_BATCH_TIMEOUT)),)
TEST_RUNNER := $(TEST_RUNNER) --batch-timeout $(TEST_BATCH_TIMEOUT)
endif
ifneq ($(strip $(TEST_BATCH_SIZE)),)
TEST_RUNNER := $(TEST_RUNNER) --batch-size $(TEST_BATCH_SIZE)
endif
ifneq ($(strip $(TEST_MAX_RETRIES)),)
TEST_RUNNER := $(TEST_RUNNER) --max-retries $(TEST_MAX_RETRIES)
endif
endif

############################
# Format Target Overrides
############################
# Override format targets from extension-ci-tools to exclude test/data/.
# Test data files are parsed as AST fixtures with exact line numbers and node
# counts asserted in tests. Formatting them shifts lines and adds nodes,
# breaking those assertions.

FORMAT_DIRS := src test/sql test/unittest

format-check:
	python3 duckdb/scripts/format.py --all --check --directories $(FORMAT_DIRS)

format:
	python3 duckdb/scripts/format.py --all --fix --noconfirm --directories $(FORMAT_DIRS)

format-fix:
	python3 duckdb/scripts/format.py --all --fix --noconfirm --directories $(FORMAT_DIRS)

format-main:
	python3 duckdb/scripts/format.py main --fix --noconfirm --directories $(FORMAT_DIRS)
.PHONY: test-require
# A TEST RUN THAT SKIPPED REPORTS SUCCESS. `require sitting_duck` in a .test file means
# "skip unless available", so when the extension cannot be loaded EVERY file skips and
# the harness still prints "all tests passed" with exit 0. That is how 680 tests across
# this fleet went unrun behind 12 green registry checks in October 2026
# (duckdb/community-extensions#3086: the recorded extension version is truncated to 32
# bytes while the engine compares the untruncated value, so `require` finds nothing).
#
# `--require <name>` makes a MISSING require a hard failure instead -- see
# sqllogic_test_runner.cpp, where IsRequired(param) turns it into
# parser.Fail("require %s: FAILED"). It is SCOPED to the named requirement, so files
# gated on anything else (none here) still skip normally. That scoping is why this
# is used rather than run_tests.py's --fail-require-skip, which collapses every skip
# reason into one string and so cannot tell an unloadable extension from a deliberate
# platform gate -- and which is a no-op as shipped anyway, searching stdout while the
# summary goes to stderr (duckdb/duckdb#27413, with a verified fix).
#
# THE NAME IS WRITTEN OUT, not $(EXT_NAME), because the two are not always the same:
# sitting_duck sets EXT_NAME=duckdb_ast while its tests say `require sitting_duck`, so
# $(EXT_NAME) there would match no token and make this guard a silent no-op -- the exact
# failure class it exists to catch.
#
# WATCHED IT FAIL BOTH WAYS on DuckDB 2.0 (b15358c371d) before being trusted, by moving
# the built extension aside (measured on duck_block_utils):
#     make test-require, present : exit 0  "2 skipped, 813 assertions in 16 cases"
#     make test-require, hidden  : exit 2  "require duck_block_utils: FAILED"
#     make test      , hidden    : exit 0  "ran tests: 0 passed, 18 skipped"  <-- the bug
# The present case's legitimate skips passing is the proof the scoping works.
#
# INERT ON THE v1.5.6 LINE, where `require` cannot fail: hiding the artifact changes
# nothing (measured on markdown and yaml). Harmless there, load-bearing on 2.0.
# Report the ASSERTION count this prints, never `make test`'s file count.
test-require:
	./build/release/test/unittest --require sitting_duck "$(TESTS_BASE_DIRECTORY)*"
