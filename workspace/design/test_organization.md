# Test Organization Design

## Current Issue
Single monolithic test file with 18 tests makes it hard to:
- Debug specific failures
- Run subset of tests during development
- Understand what each component is testing
- Add new features incrementally

## Proposed Structure

```
test/sql/
├── basic/
│   ├── extension_loading.test         # Extension loads correctly
│   ├── basic_functionality.test      # Basic read_ast works
│   └── error_handling.test           # File not found, bad language
├── parsing/
│   ├── python_simple.test            # Basic Python parsing
│   ├── python_classes.test           # Class extraction
│   ├── python_functions.test         # Function extraction  
│   ├── python_unicode.test           # Unicode support
│   ├── python_comments.test          # Comments and docstrings
│   ├── python_syntax_errors.test     # Malformed code handling
│   └── python_deep_nesting.test      # Complex nesting
├── ast_objects/
│   ├── json_output.test               # read_ast_objects basic
│   ├── ast_functions.test             # ast_functions() helper
│   ├── ast_classes.test               # ast_classes() helper
│   └── ast_imports.test               # ast_imports() helper
├── performance/
│   ├── large_files.test               # Handle big files
│   └── memory_usage.test              # Memory efficiency
└── integration/
    ├── multi_file.test                # Multiple files
    └── real_world.test                # Realistic codebases
```

## Benefits

### 1. Faster Development Cycle
```bash
# Test just what you're working on
make test TEST_FILES="test/sql/parsing/python_functions.test"

# Test a category
make test TEST_FILES="test/sql/parsing/*.test"
```

### 2. Clearer Failure Attribution
Instead of "Test 15 failed", you get "python_unicode.test failed"

### 3. Incremental Development
- Implement feature X
- Create test_x.test
- Verify it passes
- Move to feature Y

### 4. Better CI/CD
```yaml
- name: Test Basic Functionality
  run: make test TEST_FILES="test/sql/basic/*.test"
  
- name: Test Python Parsing  
  run: make test TEST_FILES="test/sql/parsing/python_*.test"
```

### 5. Documentation Value
Each test file documents expected behavior for that specific capability.

## Implementation Plan

1. **Split current test file** into logical components
2. **Create test runner script** that can run subsets
3. **Add Makefile targets** for different test categories
4. **Update CI to run in stages** (basic → parsing → advanced)

## Example Split

### basic/extension_loading.test
```sql
# name: test/sql/basic/extension_loading.test
# description: Test extension loading
# group: [duckdb_ast]

# Before loading extension
statement error
SELECT * FROM read_ast('test.py', 'python');
----
Catalog Error: Table Function with name read_ast does not exist!

# Load extension
require duckdb_ast

# After loading extension
query I
SELECT COUNT(*) > 0 FROM read_ast('test/data/python/simple.py', 'python');
----
true
```

### parsing/python_functions.test
```sql
# name: test/sql/parsing/python_functions.test  
# description: Test Python function extraction
# group: [duckdb_ast]

require duckdb_ast

# Test function definitions with names
query III
SELECT type, name, start_line
FROM read_ast('test/data/python/simple.py', 'python')
WHERE type = 'function_definition'
ORDER BY start_line;
----
function_definition	hello	1
function_definition	__init__	7
function_definition	add	10
function_definition	main	13
```