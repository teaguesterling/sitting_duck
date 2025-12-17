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
