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