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