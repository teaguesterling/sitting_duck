#!/bin/bash

# Grammar patching utility for tree-sitter submodules
# Usage: apply_grammar_patches.sh <grammar_name> <source_dir> <patch_dir> <status_dir>

set -e

GRAMMAR_NAME="$1"
SOURCE_DIR="$2"
PATCH_DIR="$3"
STATUS_DIR="$4"

if [ $# -ne 4 ]; then
    echo "Usage: $0 <grammar_name> <source_dir> <patch_dir> <status_dir>"
    exit 1
fi

# Create status directory
mkdir -p "$STATUS_DIR"

# Function to apply patch if not already applied (only output when actually applying)
apply_patch() {
    local patch_name="$1"
    local patch_file="$PATCH_DIR/$patch_name.patch"
    local status_file="$STATUS_DIR/${patch_name}_applied"
    
    if [ ! -f "$status_file" ]; then
        if [ -f "$patch_file" ]; then
            echo "Applying patch: $patch_name"
            cd "$SOURCE_DIR"
            # Try to apply patch, but handle cases where it's already applied
            if patch -p1 -i "$patch_file" --dry-run --silent >/dev/null 2>&1; then
                # Patch can be applied cleanly
                if patch -p1 -i "$patch_file" --silent; then
                    touch "$status_file"
                    echo "Patch $patch_name applied successfully"
                else
                    echo "Failed to apply patch: $patch_name"
                    exit 1
                fi
            elif patch -p1 -i "$patch_file" --dry-run -R --silent >/dev/null 2>&1; then
                # Patch is already applied (would apply in reverse)
                touch "$status_file"
                echo "Patch $patch_name already applied (detected via reverse check)"
            else
                echo "Patch $patch_name cannot be applied (conflicts or other issues)"
                exit 1
            fi
        fi
        # No output for missing patch files - just skip silently
    fi
    # No output for already applied patches - just skip silently
}

# Apply patches based on grammar name (only output if patches exist and are applied)
case "$GRAMMAR_NAME" in
    "tree-sitter-cpp")
        apply_patch "fix-cpp-grammar-require"
        ;;
    "typescript")
        apply_patch "fix-typescript-grammar-require"
        ;;
    "tree-sitter-rust")
        apply_patch "fix-rust-grammar-regex"
        ;;
    *)
        # No output for grammars without patches - just skip silently
        ;;
esac

# No final "complete" message - only output when actually doing work