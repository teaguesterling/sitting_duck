#!/bin/bash
#
# Generate all tree-sitter parsers for the sitting_duck extension
#
# This script:
# 1. Applies patches to grammar submodules
# 2. Runs tree-sitter generate for each grammar
# 3. Copies generated files to generated_parsers/ directory
#
# Usage: ./scripts/generate_all_parsers.sh [--clean]
#
# Requirements:
# - tree-sitter CLI (cargo install tree-sitter-cli)
# - Node.js (for tree-sitter generate)
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
GRAMMARS_DIR="$ROOT_DIR/grammars"
OUTPUT_DIR="$ROOT_DIR/generated_parsers"
PATCHES_DIR="$ROOT_DIR/patches"
STATUS_DIR="$ROOT_DIR/build/patch_status"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check requirements
check_requirements() {
    # Check for local tree-sitter CLI first (built from submodule)
    local LOCAL_TS_CLI="$ROOT_DIR/third_party/tree-sitter/target/release/tree-sitter"
    if [ -x "$LOCAL_TS_CLI" ]; then
        # Add to PATH for this script
        export PATH="$(dirname "$LOCAL_TS_CLI"):$PATH"
        log_info "Using local tree-sitter CLI: $LOCAL_TS_CLI"
    elif ! command -v tree-sitter &> /dev/null; then
        log_error "tree-sitter CLI not found."
        log_error "Either build it from submodule: cd third_party/tree-sitter && cargo build --release"
        log_error "Or install globally: cargo install tree-sitter-cli"
        exit 1
    fi

    if ! command -v node &> /dev/null; then
        log_error "Node.js not found. Please install Node.js"
        exit 1
    fi

    log_info "tree-sitter version: $(tree-sitter --version)"
    log_info "Node.js version: $(node --version)"
}

# Clean generated parsers directory
clean_output() {
    if [ -d "$OUTPUT_DIR" ]; then
        log_info "Cleaning $OUTPUT_DIR"
        rm -rf "$OUTPUT_DIR"
    fi
    mkdir -p "$OUTPUT_DIR"
}

# Reset a grammar submodule to clean state
reset_grammar() {
    local grammar_path="$1"
    local full_path="$GRAMMARS_DIR/$grammar_path"

    if [ -d "$full_path" ]; then
        log_info "Resetting $grammar_path"
        cd "$full_path"
        git checkout -- . 2>/dev/null || true
        git clean -fd 2>/dev/null || true
        cd "$ROOT_DIR"
    fi
}

# Apply patch to a grammar
apply_patch() {
    local patch_name="$1"
    local patch_file="$PATCHES_DIR/${patch_name}.patch"

    if [ -f "$patch_file" ]; then
        log_info "Applying patch: $patch_name"
        cd "$ROOT_DIR"
        if patch -p1 -i "$patch_file" --dry-run --silent >/dev/null 2>&1; then
            patch -p1 -i "$patch_file" --silent
            log_info "Patch $patch_name applied successfully"
        elif patch -p1 -i "$patch_file" --dry-run -R --silent >/dev/null 2>&1; then
            log_info "Patch $patch_name already applied"
        else
            log_warn "Patch $patch_name cannot be applied cleanly, skipping"
        fi
    fi
}

# Generate parser for a grammar
generate_parser() {
    local grammar_name="$1"
    local grammar_subdir="$2"  # e.g., "tree-sitter-typescript/typescript"
    local has_scanner="$3"     # "true" or "false"

    local grammar_path="$GRAMMARS_DIR/$grammar_subdir"
    local output_path="$OUTPUT_DIR/$grammar_subdir/src"

    if [ ! -d "$grammar_path" ]; then
        log_error "Grammar not found: $grammar_path"
        return 1
    fi

    log_info "Generating parser for $grammar_name"

    # Create output directory
    mkdir -p "$output_path"

    # Run tree-sitter generate
    cd "$grammar_path"

    # Install npm dependencies if package.json exists and node_modules doesn't
    if [ -f "package.json" ] && [ ! -d "node_modules" ]; then
        log_info "  -> Installing npm dependencies..."
        timeout 120 npm install --silent 2>/dev/null || {
            log_warn "  -> npm install timed out or failed, continuing..."
        }
    fi

    # Generate parser
    log_info "  -> Running tree-sitter generate..."
    if ! timeout 120 tree-sitter generate 2>&1; then
        log_error "  -> tree-sitter generate failed!"
        cd "$ROOT_DIR"
        return 1
    fi

    # Copy generated files
    if [ -f "src/parser.c" ]; then
        cp "src/parser.c" "$output_path/"
        log_info "  -> Copied parser.c"
    else
        log_error "  -> parser.c not generated!"
        cd "$ROOT_DIR"
        return 1
    fi

    if [ "$has_scanner" = "true" ] && [ -f "src/scanner.c" ]; then
        cp "src/scanner.c" "$output_path/"
        log_info "  -> Copied scanner.c"
    fi

    # Copy any additional .h files from src/ (e.g., tag.h for HTML)
    # These are grammar-specific headers included by scanner.c
    local header_count=$(find "src" -maxdepth 1 -name "*.h" -type f 2>/dev/null | wc -l)
    if [ "$header_count" -gt 0 ]; then
        find "src" -maxdepth 1 -name "*.h" -type f -exec cp {} "$output_path/" \;
        log_info "  -> Copied $header_count header file(s)"
    fi

    # Copy tree_sitter directory if it exists (contains alloc.h, array.h, parser.h)
    if [ -d "src/tree_sitter" ]; then
        mkdir -p "$output_path/tree_sitter"
        cp -r "src/tree_sitter/"* "$output_path/tree_sitter/"
        log_info "  -> Copied tree_sitter headers"
    fi

    # Handle grammars with shared directories (e.g., TypeScript's common/ directory)
    # The scanner.c may include relative paths like "../../common/scanner.h"
    local parent_grammar_dir="$(dirname "$grammar_path")"
    if [ -d "$parent_grammar_dir/common" ]; then
        local common_output="$OUTPUT_DIR/$(dirname "$grammar_subdir")/common"
        mkdir -p "$common_output"
        # Copy common files, excluding .orig backup files from patching
        find "$parent_grammar_dir/common" -maxdepth 1 -type f ! -name "*.orig" -exec cp {} "$common_output/" \;
        log_info "  -> Copied common/ shared directory"
    fi

    cd "$ROOT_DIR"
}

# Main grammar list with their configurations
# Format: grammar_name:subdir:has_scanner
GRAMMARS=(
    "c:tree-sitter-c:false"
    "cpp:tree-sitter-cpp:true"
    "javascript:tree-sitter-javascript:true"
    "typescript:tree-sitter-typescript/typescript:true"
    "python:tree-sitter-python:true"
    "sql:tree-sitter-sql:true"
    "go:tree-sitter-go:false"
    "ruby:tree-sitter-ruby:true"
    "markdown:tree-sitter-markdown/tree-sitter-markdown:true"
    "java:tree-sitter-java:false"
    "php:tree-sitter-php/php:true"
    "html:tree-sitter-html:true"
    "css:tree-sitter-css:true"
    "rust:tree-sitter-rust:true"
    "json:tree-sitter-json:false"
    "bash:tree-sitter-bash:true"
    "swift:tree-sitter-swift:true"
    "r:tree-sitter-r:true"
    "kotlin:tree-sitter-kotlin:true"
    "csharp:tree-sitter-c-sharp:true"
    "lua:tree-sitter-lua:true"
)

# Patches to apply (grammar_name:patch_name)
PATCHES=(
    "cpp:fix-cpp-grammar-require"
    "typescript:fix-typescript-grammar-require"
    "rust:fix-rust-grammar-regex"
    "php:fix-php-scanner-assert"
)

main() {
    local clean_first=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --clean)
                clean_first=true
                shift
                ;;
            --help|-h)
                echo "Usage: $0 [--clean]"
                echo ""
                echo "Options:"
                echo "  --clean    Reset all grammar submodules before generating"
                echo "  --help     Show this help message"
                exit 0
                ;;
            *)
                log_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done

    log_info "Starting parser generation"
    log_info "Root directory: $ROOT_DIR"
    log_info "Output directory: $OUTPUT_DIR"

    # Check requirements
    check_requirements

    # Clean output directory
    clean_output

    # Optionally reset grammars
    if [ "$clean_first" = true ]; then
        log_info "Resetting grammar submodules..."
        for grammar_config in "${GRAMMARS[@]}"; do
            IFS=':' read -r name subdir has_scanner <<< "$grammar_config"
            reset_grammar "$subdir"
        done
    fi

    # Apply patches
    log_info "Applying patches..."
    for patch_config in "${PATCHES[@]}"; do
        IFS=':' read -r grammar_name patch_name <<< "$patch_config"
        apply_patch "$patch_name"
    done

    # Generate parsers
    log_info "Generating parsers..."
    local success_count=0
    local fail_count=0

    for grammar_config in "${GRAMMARS[@]}"; do
        IFS=':' read -r name subdir has_scanner <<< "$grammar_config"
        if generate_parser "$name" "$subdir" "$has_scanner"; then
            success_count=$((success_count + 1))
        else
            fail_count=$((fail_count + 1))
        fi
    done

    echo ""
    log_info "Generation complete!"
    log_info "  Success: $success_count"
    if [ $fail_count -gt 0 ]; then
        log_warn "  Failed: $fail_count"
    fi

    # Copy unified tree_sitter headers from tree-sitter library
    log_info "Copying unified tree_sitter headers..."
    local TS_LIB_SRC="$ROOT_DIR/third_party/tree-sitter/lib/src"
    local TS_HEADERS_DIR="$OUTPUT_DIR/tree_sitter"
    mkdir -p "$TS_HEADERS_DIR"

    if [ -f "$TS_LIB_SRC/parser.h" ]; then
        cp "$TS_LIB_SRC/parser.h" "$TS_HEADERS_DIR/"
        cp "$TS_LIB_SRC/array.h" "$TS_HEADERS_DIR/"
        cp "$TS_LIB_SRC/alloc.h" "$TS_HEADERS_DIR/"
        log_info "  -> Copied tree_sitter headers to $TS_HEADERS_DIR"
    else
        log_warn "  -> tree_sitter headers not found in $TS_LIB_SRC"
    fi

    # Create a manifest file
    echo "# Generated parsers manifest" > "$OUTPUT_DIR/MANIFEST"
    echo "# Generated on: $(date -Iseconds)" >> "$OUTPUT_DIR/MANIFEST"
    echo "# tree-sitter version: $(tree-sitter --version)" >> "$OUTPUT_DIR/MANIFEST"
    echo "" >> "$OUTPUT_DIR/MANIFEST"
    for grammar_config in "${GRAMMARS[@]}"; do
        IFS=':' read -r name subdir has_scanner <<< "$grammar_config"
        echo "$name:$subdir" >> "$OUTPUT_DIR/MANIFEST"
    done

    log_info "Manifest written to $OUTPUT_DIR/MANIFEST"

    # Clean up any .orig backup files from patching that may have been copied
    local orig_count=$(find "$OUTPUT_DIR" -name "*.orig" -type f | wc -l)
    if [ "$orig_count" -gt 0 ]; then
        find "$OUTPUT_DIR" -name "*.orig" -type f -delete
        log_info "Cleaned up $orig_count .orig backup files"
    fi
}

main "$@"
