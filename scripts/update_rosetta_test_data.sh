#!/bin/bash
# ==============================================================================
# update_rosetta_test_data.sh
# ==============================================================================
#
# PURPOSE:
#   Copies a curated subset of Rosetta Code examples into the test/data/rosetta/
#   directory for use in CI tests. This avoids requiring the full rosettacode-acmeism
#   submodule during CI builds (which fails on Windows due to files with colons
#   in their names).
#
# PREREQUISITES:
#   The rosettacode-acmeism submodule must be initialized:
#     git submodule update --init third_party/rosettacode-acmeism
#
# USAGE:
#   ./scripts/update_rosetta_test_data.sh
#
# LICENSE:
#   Rosetta Code content is licensed under GNU Free Documentation License (GFDL) 1.2
#   See: https://rosettacode.org/wiki/Rosetta_Code:Copyrights
#   Attribution is required - see test/data/rosetta/LICENSE for details.
#
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
SOURCE_DIR="$PROJECT_ROOT/third_party/rosettacode-acmeism"
TARGET_DIR="$PROJECT_ROOT/test/data/rosetta"

# Check if submodule is initialized
if [ ! -d "$SOURCE_DIR/Lang" ]; then
    echo "ERROR: Rosetta Code submodule not initialized."
    echo "Run: git submodule update --init third_party/rosettacode-acmeism"
    exit 1
fi

echo "Copying Rosetta Code test data..."
echo "Source: $SOURCE_DIR"
echo "Target: $TARGET_DIR"

# Create target directory structure
mkdir -p "$TARGET_DIR"

# Languages we need for tests (must not contain Windows-incompatible characters)
LANGUAGES="Python JavaScript Go Java Rust"

# Tasks we need for tests (selected to avoid problematic characters)
TASKS="A+B 99-bottles-of-beer 100-doors Hello-world-Text Fibonacci-sequence Abstract-type Factorial FizzBuzz"

# Copy selected language/task combinations
for lang in $LANGUAGES; do
    echo "Processing $lang..."
    mkdir -p "$TARGET_DIR/Lang/$lang"

    for task in $TASKS; do
        src="$SOURCE_DIR/Lang/$lang/$task"
        if [ -d "$src" ] || [ -L "$src" ]; then
            # Copy the task directory (follow symlinks with -L)
            cp -rL "$src" "$TARGET_DIR/Lang/$lang/" 2>/dev/null || true
        fi
    done

    # Count copied files
    count=$(find "$TARGET_DIR/Lang/$lang" -type f 2>/dev/null | wc -l)
    echo "  Copied $count files for $lang"
done

# Create/update the license file
cat > "$TARGET_DIR/LICENSE" << 'EOF'
Rosetta Code Test Data
======================

This directory contains code examples from Rosetta Code (https://rosettacode.org)
used for testing the sitting_duck AST parsing extension.

LICENSE
-------
This content is licensed under the GNU Free Documentation License (GFDL) 1.2.
See: https://www.gnu.org/licenses/old-licenses/fdl-1.2.html

ATTRIBUTION
-----------
Source: https://github.com/acmeism/RosettaCodeData
Original: https://rosettacode.org

The code examples in this directory were contributed by many authors to
Rosetta Code. Each file represents the work of one or more contributors
to the Rosetta Code project.

For the full list of contributors to any specific example, please visit:
https://rosettacode.org/wiki/<Task_Name>

USAGE
-----
This subset was extracted for use in automated testing. The full dataset
is available at https://github.com/acmeism/RosettaCodeData

DISCLAIMER
----------
These code samples are provided "as-is" for testing purposes. Some examples
may contain syntax errors or be incomplete, as they represent the state of
contributions to Rosetta Code at the time of extraction.
EOF

# Create a README for the test data
cat > "$TARGET_DIR/README.md" << 'EOF'
# Rosetta Code Test Data

This directory contains a curated subset of [Rosetta Code](https://rosettacode.org)
examples used for testing the `sitting_duck` AST parsing extension.

## Why This Exists

The full Rosetta Code dataset (via the `rosettacode-acmeism` submodule) contains
files with colons (`:`) in their names, which are invalid on Windows filesystems.
This curated subset only includes files with Windows-compatible names.

## Contents

Languages included:
- Python
- JavaScript
- Go
- Java
- Rust

Tasks included:
- A+B
- 99-bottles-of-beer
- 100-doors
- Hello-world-Text
- Fibonacci-sequence
- Abstract-type
- Factorial
- FizzBuzz

## Updating

To update this data from the full submodule:

```bash
# Initialize the submodule (Unix only - will fail on Windows)
git submodule update --init third_party/rosettacode-acmeism

# Run the update script
./scripts/update_rosetta_test_data.sh

# Commit the changes
git add test/data/rosetta/
git commit -m "Update Rosetta Code test data"
```

## License

See [LICENSE](LICENSE) - Content is under GNU Free Documentation License (GFDL) 1.2
EOF

echo ""
echo "Done! Rosetta Code test data copied to: $TARGET_DIR"
echo ""
echo "Next steps:"
echo "  1. Update tests to use test/data/rosetta/ instead of third_party/rosettacode-acmeism/"
echo "  2. git add test/data/rosetta/"
echo "  3. git commit -m 'Add Rosetta Code test data subset'"
