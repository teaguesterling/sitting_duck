# AI Agent Guide to DuckDB AST Extension

**Last Updated:** December 2024  
**Extension Version:** Current (Semantic Taxonomy + Streaming API)

## Overview

The DuckDB AST Extension provides a **universal semantic taxonomy** for understanding code across programming languages. Instead of learning language-specific node types, AI agents can work with standardized semantic categories that remain consistent whether analyzing Python, JavaScript, C++, or any of the 27 supported languages.

The extension combines:
- **Universal semantic types** for cross-language understanding
- **Streaming table functions** for efficient large codebase analysis
- **Multi-file processing** with automatic language detection

## Why This Matters for AI Agents

**Problem**: Different languages use different terminology for the same concepts:
- Python: `function_definition` 
- JavaScript: `function_declaration`
- C++: `function_definition` (but with different semantics)
- SQL: `CREATE FUNCTION`

**Solution**: Universal semantic types that map all these to `DEFINITION_FUNCTION`, enabling cross-language understanding and code analysis.

## Core Taxonomy Structure

### 8-Bit Semantic Encoding

Each AST node has an 8-bit `semantic_type` field with the structure: `[ss kk tt ll]`

- **ss (bits 6-7)**: Super Kind (4 major categories)
- **kk (bits 4-5)**: Kind (16 subcategories) 
- **tt (bits 2-3)**: Super Type (4 variants per kind)
- **ll (bits 0-1)**: Language-specific (reserved for future use)

### The 16 Semantic Kinds

| Super Kind | Kinds | Purpose |
|------------|-------|---------|
| **DATA_STRUCTURE** | LITERAL, NAME, PATTERN, TYPE | Data representation and identification |
| **COMPUTATION** | OPERATOR, COMPUTATION_NODE, TRANSFORM, DEFINITION | Operations and definitions |
| **CONTROL_EFFECTS** | EXECUTION, FLOW_CONTROL, ERROR_HANDLING, ORGANIZATION | Program flow and effects |
| **META_EXTERNAL** | METADATA, EXTERNAL, PARSER_SPECIFIC, RESERVED | Meta-information and language specifics |

## Semantic Super Types

### LITERAL Types (Data Values)
- `LITERAL_NUMBER`: `42`, `3.14`, `0xFF` across all languages
- `LITERAL_STRING`: `"hello"`, `'world'`, `R"(raw)"` 
- `LITERAL_ATOMIC`: `true`, `false`, `null`, `None`, `undefined`
- `LITERAL_STRUCTURED`: `[1,2,3]`, `{a: 1}`, `{x, y, z}`

### OPERATOR Types (Operations)
- `OPERATOR_ARITHMETIC`: `+`, `-`, `*`, `/`, `%`, `**`, bitwise ops
- `OPERATOR_LOGICAL`: `&&`, `||`, `!`, `and`, `or`, `not`, `? :`
- `OPERATOR_COMPARISON`: `==`, `!=`, `<`, `>`, `===`, `is`, `in`
- `OPERATOR_ASSIGNMENT`: `=`, `+=`, `-=`, `:=`, compound assignments

### FLOW_CONTROL Types (Program Flow)
- `FLOW_CONDITIONAL`: `if`, `switch`, `match`, ternary operators
- `FLOW_LOOP`: `for`, `while`, `do-while`, `foreach`
- `FLOW_JUMP`: `return`, `break`, `continue`, `goto`
- `FLOW_SYNC`: `async`, `await`, `yield`, `synchronized`, coroutines

### EXECUTION Types (Actions)
- `EXECUTION_STATEMENT`: Expression statements, root executable nodes
- `EXECUTION_DECLARATION`: `let x`, `var y`, `int z`, variable introductions
- `EXECUTION_INVOCATION`: Function/method calls, constructor calls
- `EXECUTION_MUTATION`: Assignments, scope modifications

### DEFINITION Types (Declarations)
- `DEFINITION_FUNCTION`: Function/method definitions across all languages
- `DEFINITION_VARIABLE`: Variable/constant declarations
- `DEFINITION_CLASS`: Classes, structs, interfaces, records
- `DEFINITION_MODULE`: Modules, namespaces, packages

## Semantic Type Convenience Functions

The extension provides several utility functions to work with semantic types more easily:

### Type Conversion Functions
- `semantic_type_to_string(code)` - Convert semantic type code to readable name
- `get_super_kind(code)` - Get the super kind (DATA_STRUCTURE, COMPUTATION, etc.)
- `get_kind(code)` - Get the full kind name

### Predicate Functions

**Core Predicates (Native C++ functions):**
- `is_definition(code)` - Check if it's any definition type
- `is_call(code)` - Check if it's a function/method call
- `is_control_flow(code)` - Check if it's control flow (if, loops, etc.)
- `is_identifier(code)` - Check if it's an identifier/name

**Specific Type Predicates (SQL macros):**

*Definition predicates:*
- `is_function_definition(st)` - Function/method definitions
- `is_class_definition(st)` - Class/struct/interface definitions
- `is_variable_definition(st)` - Variable declarations
- `is_module_definition(st)` - Module/namespace definitions
- `is_type_definition(st)` - Type aliases/typedefs

*Control flow predicates:*
- `is_conditional(st)` - If/switch/match statements
- `is_loop(st)` - For/while/do loops
- `is_jump(st)` - Return/break/continue/throw

*Literal predicates:*
- `is_literal(st)` - Any literal value
- `is_string_literal(st)` - String literals
- `is_number_literal(st)` - Numeric literals
- `is_boolean_literal(st)` - Boolean literals

*External/Import predicates:*
- `is_import(st)` - Import/require/use statements
- `is_export(st)` - Export statements
- `is_foreign(st)` - FFI declarations

*Metadata predicates:*
- `is_comment(st)` - Comments
- `is_annotation(st)` - Decorators/annotations
- `is_directive(st)` - Preprocessor directives

*Type predicates:*
- `is_type_primitive(st)` - Primitive types (int, string, bool)
- `is_type_composite(st)` - Struct/union/tuple types
- `is_type_reference(st)` - Reference/pointer types
- `is_type_generic(st)` - Generic/template types

### Example Usage
```sql
-- Human-readable semantic type analysis
SELECT 
    semantic_type_to_string(semantic_type) as type_name,
    get_super_kind(semantic_type) as category,
    COUNT(*) as count
FROM read_ast('main.py')
GROUP BY semantic_type
ORDER BY count DESC;

-- Find complex functions using predicates
SELECT name, file_path, descendant_count
FROM read_ast('**/*.py', ignore_errors := true)
WHERE is_definition(semantic_type) 
  AND semantic_type_to_string(semantic_type) = 'DEFINITION_FUNCTION'
  AND descendant_count > 50;
```

> **💡 Pro Tip**: Use convenience functions for readability during development, then switch to raw semantic type codes for performance in production queries. See the complete semantic type reference in [Semantic Types](api/semantic-types.md).

## DuckDB-Consistent Pattern Arrays (NEW!)

The extension now supports **DuckDB-style pattern arrays** following the same conventions as `read_csv`, `read_parquet`, and other DuckDB file functions.

### Array Syntax
```sql
-- Single pattern (traditional)
SELECT * FROM read_ast('src/**/*.py');

-- Pattern array (NEW! - DuckDB-consistent)
SELECT * FROM read_ast(['src/**/*.py', 'lib/**/*.js', 'tests/**/*.ts']);

-- Mixed files and patterns
SELECT * FROM read_ast(['main.py', 'src/**/*.js', 'specific_file.cpp']);
```

### Key Benefits for AI Agents
- **Precise control**: Specify exactly which file sets to analyze
- **File deduplication**: Same file matched by multiple patterns returns only once  
- **Consistent ordering**: Results sorted by file path (DuckDB convention)
- **Error resilience**: Use `ignore_errors := true` for robust multi-pattern processing
- **Performance**: Reduces file system traversal compared to broad glob patterns

### Error Handling Patterns
```sql
-- Empty array validation
SELECT * FROM read_ast([]);
-- Error: File pattern list cannot be empty

-- NULL value validation  
SELECT * FROM read_ast(['file.py', NULL]);
-- Error: File pattern list cannot contain NULL values

-- Robust multi-language analysis
SELECT language, COUNT(*) as files_processed
FROM read_ast([
    'src/**/*.py',           -- Python source
    'frontend/**/*.js',      -- JavaScript frontend
    'backend/**/*.ts',       -- TypeScript backend  
    'native/**/*.cpp',       -- C++ native code
    'docs/**/*.md'           -- Documentation
], ignore_errors := true)
GROUP BY language;
```

### AI Agent Workflow with Arrays
```sql
-- 1. Define language-specific file sets
WITH language_patterns AS (
    SELECT unnest([
        'src/**/*.py',
        'lib/**/*.js', 
        'api/**/*.ts',
        'core/**/*.cpp'
    ]) as pattern
),

-- 2. Analyze each language set
analysis AS (
    SELECT 
        language,
        COUNT(DISTINCT file_path) as files,
        COUNT(*) as total_nodes,
        COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions,
        COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as classes
    FROM read_ast([
        'src/**/*.py', 'lib/**/*.js', 'api/**/*.ts', 'core/**/*.cpp'
    ], ignore_errors := true)
    GROUP BY language
)

-- 3. Generate insights
SELECT 
    language,
    files,
    ROUND(functions::FLOAT / files, 2) as avg_functions_per_file,
    ROUND(total_nodes::FLOAT / files, 2) as avg_complexity_per_file
FROM analysis
ORDER BY avg_complexity_per_file DESC;
```

## Usage Examples for AI Agents

## Quick Start

### 1. Load the Extension
```sql
-- In DuckDB CLI or any SQL interface
LOAD 'sitting_duck';
```

### 2. Basic Usage - Analyze Single Files
```sql
-- Analyze a Python file with automatic language detection
SELECT COUNT(*) as total_nodes FROM read_ast('script.py');

-- Analyze with explicit language specification
SELECT COUNT(*) as total_nodes FROM read_ast('script.js', 'javascript');

-- Find all functions using semantic types (readable approach)
SELECT name, type, file_path FROM read_ast('code.py') 
WHERE is_definition(semantic_type) AND semantic_type_to_string(semantic_type) = 'DEFINITION_FUNCTION';

-- Or use raw semantic type codes for performance
SELECT name, type, file_path FROM read_ast('code.py')
WHERE semantic_type = 240; -- DEFINITION_FUNCTION
```

### 3. Multi-File Analysis

```sql
-- Analyze entire directories with glob patterns
SELECT file_path, COUNT(*) as nodes_per_file 
FROM read_ast('src/**/*.py', ignore_errors := true)
GROUP BY file_path;

-- DuckDB-style pattern arrays for precise control (NEW!)
SELECT file_path, language, COUNT(*) as nodes_per_file 
FROM read_ast([
    'src/**/*.py',     -- Python source files
    'lib/**/*.js',     -- JavaScript libraries  
    'tests/**/*.ts',   -- TypeScript tests
    'include/**/*.hpp' -- C++ headers
], ignore_errors := true)
GROUP BY file_path, language
ORDER BY nodes_per_file DESC;

-- Cross-language analysis using convenience functions
SELECT 
    language, 
    COUNT(*) as total_functions,
    get_super_kind(semantic_type) as category
FROM read_ast(['**/*.py', '**/*.js', '**/*.cpp'], ignore_errors := true)
WHERE is_definition(semantic_type) AND semantic_type_to_string(semantic_type) = 'DEFINITION_FUNCTION'
GROUP BY language, get_super_kind(semantic_type);

-- Performance-optimized version with raw codes and arrays
SELECT language, COUNT(*) as total_functions
FROM read_ast(['**/*.py', '**/*.js', '**/*.cpp'], ignore_errors := true)
WHERE semantic_type = 240 -- DEFINITION_FUNCTION
GROUP BY language;
```

## Supported Languages

The extension supports **27 programming languages** with automatic detection:

| Category | Languages | Semantic Support |
|----------|-----------|------------------|
| **Web** | JavaScript, TypeScript, HTML, CSS | ✅ Full |
| **Systems** | C, C++, Go, Rust, Zig | ✅ Full |
| **Scripting** | Python, Ruby, PHP, Lua, R, Bash | ✅ Full |
| **Enterprise** | Java, C#, Kotlin, Swift, Dart | ✅ Full |
| **Data/Query** | SQL, DuckDB, GraphQL, JSON | ✅ Full |
| **Config** | HCL (Terraform), TOML | ✅ Full |
| **Docs** | Markdown | ✅ Full |

### Cross-Language Function Finding
```sql
-- Find all functions regardless of language using semantic types
SELECT name, type, language, file_path FROM read_ast('**/*.*', ignore_errors := true)
WHERE semantic_type = 240; -- DEFINITION_FUNCTION

-- Same query works across Python, JavaScript, C++, etc.
SELECT COUNT(*) as function_count, language
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 240
GROUP BY language;
```

## Advanced Usage with Semantic Types

### Pattern Matching with Bit Operations
```sql
-- Find all arithmetic operations across languages
SELECT file_path, type, name, language
FROM read_ast('**/*.{py,js,cpp}', ignore_errors := true)
WHERE (semantic_type & 252) = 192; -- OPERATOR_ARITHMETIC (base code 192)

-- Find all conditionals across languages
SELECT COUNT(*) as conditional_count, language
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 144 -- FLOW_CONDITIONAL
GROUP BY language;
```

### Cross-Language Refactoring Analysis
```sql
-- Find assignment patterns to refactor
SELECT file_path, type, name, semantic_type, start_line
FROM read_ast('**/*.{py,js,cpp}', ignore_errors := true)
WHERE semantic_type = 204 -- OPERATOR_ASSIGNMENT
ORDER BY file_path, start_line;

-- Find complex functions (high depth) for refactoring candidates
SELECT file_path, name, depth, descendant_count
FROM read_ast('**/*.py', ignore_errors := true)
WHERE semantic_type = 240 AND depth > 3 -- Deep function definitions
ORDER BY descendant_count DESC;
```

## API Reference

### Core Table Function

#### `read_ast(file_patterns, language?, options...)`
The main table function for parsing and analyzing code:

```sql
-- Single file
read_ast('script.py')
read_ast('script.js', 'javascript')

-- Multiple files with glob patterns  
read_ast('src/**/*.py')
read_ast('**/*.{js,ts,py}')

-- DuckDB-style pattern arrays (NEW!)
read_ast(['src/**/*.py', 'lib/**/*.js', 'tests/**/*.ts'])
read_ast(['main.py', 'utils.js'], 'auto')  -- Mixed files with auto-detection

-- With options (works with both single patterns and arrays)
read_ast('src/**/*.*', ignore_errors := true)
read_ast(['**/*.py', '**/*.js'], ignore_errors := true, peek_size := 200)
read_ast(['script.py'], peek_mode := 'lines')
```

**Returns columns (20 default, 22 with `source := 'full'`):**
- `node_id`: Unique identifier for each AST node
- `type`: Language-specific node type (e.g., 'function_definition')
- `semantic_type`: 8-bit universal semantic category (SEMANTIC_TYPE)
- `flags`: Node property flags (use `has_body()`, `is_declaration_only()`)
- `name`: Node name/identifier (if applicable)
- `signature_type`: Type/return type information
- `parameters`: Function parameters (STRUCT array with name and type)
- `modifiers`: Access modifiers and keywords (VARCHAR array)
- `annotations`: Decorator/annotation text
- `qualified_name`: Fully qualified name
- `file_path`: Source file path
- `language`: Detected or specified language
- `start_line`, `end_line`: Position info (line numbers)
- `start_column`, `end_column`: Column positions (**only with `source := 'full'`**)
- `parent_id`: Parent node ID (for tree structure)
- `depth`: Nesting depth in the AST
- `sibling_index`: Position among siblings
- `children_count`, `descendant_count`: Tree size metrics
- `peek`: Sample of source code for this node

### Language Support Functions

```sql
-- Get list of supported languages
SELECT * FROM ast_supported_languages();

-- Parse code string directly (table function)
SELECT * FROM parse_ast('def hello(): pass', 'python');
```

### Options and Parameters

- **`ignore_errors`**: Continue processing when files have syntax errors
- **`peek_size`**: Number of characters to include in peek field (default: 120)
- **`peek_mode`**: How to extract peek text ('auto', 'chars', 'lines')

## AI Agent Scenarios

### Scenario 1: Code Discovery and Inventory
```sql
-- "What's in this codebase?"
SELECT
    language,
    COUNT(*) as total_nodes,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as classes,
    COUNT(DISTINCT file_path) as files
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;
```

### Scenario 2: Cross-Language Complexity Analysis
```sql
-- "Which files are most complex?"
SELECT
    file_path,
    language,
    MAX(depth) as max_depth,
    COUNT(*) as total_nodes,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as function_count
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY file_path, language
HAVING function_count > 5
ORDER BY max_depth DESC, total_nodes DESC;
```

### Scenario 3: Finding Specific Patterns
```sql
-- "Find all test functions across languages"
SELECT file_path, name, type, language, start_line
FROM read_ast('**/*.*', ignore_errors := true)
WHERE semantic_type = 240 -- DEFINITION_FUNCTION
  AND (name ILIKE '%test%' OR name ILIKE '%spec%')
ORDER BY file_path, start_line;

-- "Find all error handling constructs"
SELECT file_path, type, language, COUNT(*) as error_handling_count
FROM read_ast('**/*.*', ignore_errors := true)
WHERE (semantic_type & 252) >= 160 AND (semantic_type & 252) <= 172 -- ERROR_* kinds (160-172)
GROUP BY file_path, type, language
ORDER BY error_handling_count DESC;
```

### Scenario 4: Code Quality and Technical Debt
```sql
-- "Find deeply nested code that might need refactoring"
SELECT
    file_path,
    name,
    type,
    depth,
    descendant_count,
    start_line
FROM read_ast('**/*.{py,js,cpp}', ignore_errors := true)
WHERE depth > 6
  AND semantic_type IN (240, 248) -- Functions or classes
ORDER BY depth DESC, descendant_count DESC;

-- "Find files with high cyclomatic complexity indicators"
SELECT
    file_path,
    COUNT(CASE WHEN semantic_type = 144 THEN 1 END) as conditionals,
    COUNT(CASE WHEN semantic_type = 148 THEN 1 END) as loops,
    COUNT(*) as total_nodes
FROM read_ast('**/*.py', ignore_errors := true)
GROUP BY file_path
HAVING (conditionals + loops) > 10
ORDER BY (conditionals + loops) DESC;
```

### Scenario 5: Documentation and Code Understanding
```sql
-- "Generate a function inventory with context"
SELECT
    file_path,
    name as function_name,
    type,
    start_line,
    children_count as parameter_indicators,
    descendant_count as complexity_score,
    SUBSTR(peek, 1, 50) || '...' as preview
FROM read_ast('src/**/*.py', ignore_errors := true)
WHERE semantic_type = 240 AND name IS NOT NULL
ORDER BY file_path, start_line;
```

## Best Practices for AI Agents

### 1. Use Semantic Types for Cross-Language Analysis
```sql
-- ✅ GOOD: Works across all languages
SELECT * FROM read_ast('**/*.*') WHERE semantic_type = 240; -- DEFINITION_FUNCTION

-- ❌ AVOID: Language-specific types
SELECT * FROM read_ast('**/*.*') WHERE type = 'function_definition'; -- Only works for some languages
```

### 2. Always Use ignore_errors for Large Codebases
```sql
-- ✅ GOOD: Robust against syntax errors
SELECT * FROM read_ast('**/*.*', ignore_errors := true);

-- ❌ RISKY: Will fail on first syntax error
SELECT * FROM read_ast('**/*.*');
```

### 3. Leverage Bit Patterns for Efficient Filtering
```sql
-- Find all LITERAL types (codes 64-79)
WHERE (semantic_type & 252) >= 64 AND (semantic_type & 252) < 80;

-- Find all OPERATOR types (codes 192-207)
WHERE (semantic_type & 252) >= 192 AND (semantic_type & 252) < 208;

-- Find all FLOW_CONTROL constructs (codes 144-159)
WHERE (semantic_type & 252) >= 144 AND (semantic_type & 252) < 160;

-- Find all DEFINITION types (codes 240-255)
WHERE (semantic_type & 252) >= 240;
```

### 4. Use File Patterns Effectively
```sql
-- Traditional glob patterns
read_ast('**/*.py')           -- Python only
read_ast('**/*.{js,ts}')      -- JavaScript/TypeScript
read_ast('**/*.{cpp,hpp,h}')  -- C/C++

-- DuckDB-style pattern arrays (NEW! - More precise control)
read_ast(['src/**/*.py', 'lib/**/*.py'])           -- Python from specific dirs
read_ast(['frontend/**/*.js', 'backend/**/*.ts'])  -- JS/TS by role
read_ast(['core/**/*.cpp', 'include/**/*.hpp'])    -- C++ source + headers

-- Mixed approaches
read_ast(['main.py', 'src/**/*.js', 'config.cpp']) -- Specific files + patterns

-- All supported languages (choose based on your needs)
read_ast('**/*.*', ignore_errors := true)           -- Broad pattern
read_ast([                                          -- Explicit control
    '**/*.py', '**/*.js', '**/*.ts', '**/*.cpp', 
    '**/*.java', '**/*.go', '**/*.rb'
], ignore_errors := true)
```

## Integration with AI Workflows

### Recommended Analysis Pipeline
1. **Load**: `LOAD 'sitting_duck';`
2. **Inventory**: Get overview with `read_ast('**/*.*', ignore_errors := true)`
3. **Filter**: Use semantic types to find constructs of interest
4. **Analyze**: Combine with SQL analytics for insights
5. **Act**: Generate reports, refactoring plans, or documentation

### Example Complete Workflow
```sql
-- 1. Load extension
LOAD 'sitting_duck';

-- 2. Get codebase overview
WITH overview AS (
    SELECT language, COUNT(*) as files,
           COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as functions
    FROM read_ast('**/*.*', ignore_errors := true)
    GROUP BY language
),

-- 3. Find complex functions needing attention
complex_functions AS (
    SELECT file_path, name, depth, descendant_count
    FROM read_ast('**/*.*', ignore_errors := true)
    WHERE semantic_type = 240 AND depth > 5
),

-- 4. Identify potential issues
issues AS (
    SELECT file_path, 'Deep nesting' as issue_type, COUNT(*) as count
    FROM complex_functions
    GROUP BY file_path
)

-- 5. Generate actionable report
SELECT 
    o.language,
    o.files,
    o.functions,
    COALESCE(i.count, 0) as complexity_issues
FROM overview o
LEFT JOIN issues i ON TRUE
ORDER BY complexity_issues DESC;
```

## Performance and Scalability

### Efficient Large Codebase Analysis
```sql
-- Use ignore_errors for robustness
read_ast('**/*.*', ignore_errors := true)

-- Pattern arrays for precise control (NEW! - Often more efficient)
read_ast([
    'src/**/*.py',     -- Only source Python files
    'lib/**/*.js',     -- Only library JavaScript files  
    'api/**/*.ts'      -- Only API TypeScript files
], ignore_errors := true)

-- Stream processing - no memory limits
-- Results are processed in chunks, suitable for massive codebases

-- Filter early for performance
SELECT file_path, COUNT(*) as function_count
FROM read_ast(['**/*.py', '**/*.js'], ignore_errors := true)
WHERE semantic_type = 240  -- Filter at source
GROUP BY file_path;
```

### Array Performance Benefits
- **Reduced file system traversal**: Specific patterns avoid scanning irrelevant directories
- **File deduplication**: Built-in at C++ level, more efficient than SQL DISTINCT
- **Predictable ordering**: Results sorted by file path, enabling efficient downstream processing
- **Better error isolation**: Failed patterns don't affect successful ones with `ignore_errors := true`

### Memory Considerations
- The extension uses **streaming processing** - no memory limits
- File-by-file parsing with lazy evaluation
- Suitable for analyzing repositories with thousands of files
- Results can be materialized into tables for repeated analysis

## Troubleshooting and Common Issues

### File Pattern Issues
```sql
-- ✅ CORRECT: Recursive patterns
read_ast('**/*.py')           -- All Python files recursively
read_ast('src/**/*.*')        -- All files in src/ recursively

-- ✅ CORRECT: Pattern arrays (DuckDB-consistent)
read_ast(['**/*.py', '**/*.js'])              -- Multiple languages
read_ast(['src/**/*.py', 'lib/**/*.js'])      -- Specific directories
read_ast(['main.py', 'src/**/*.js'])          -- Mixed files and patterns

-- ❌ INCORRECT: Missing recursiveness
read_ast('*.py')              -- Only current directory
read_ast('src/*.*')           -- Only immediate src/ children

-- ❌ INCORRECT: Array syntax errors
read_ast([])                  -- Empty array
read_ast(['file.py', NULL])   -- NULL values in array
read_ast('file1.py', 'file2.py')  -- Multiple parameters instead of array
```

### Language Detection
```sql
-- Auto-detection (recommended)
read_ast('script.py')         -- Detects Python from .py extension

-- Explicit language (when needed)
read_ast('script', 'python')  -- Force Python for extensionless files
```

### Error Handling
```sql
-- Robust parsing (recommended for large codebases)
read_ast('**/*.*', ignore_errors := true)

-- Strict parsing (fails on first syntax error)
read_ast('**/*.*')  -- Will stop on syntax errors
```

## Future Enhancements

🔄 **Planned Features:**
- **Multi-threading**: Parallel file parsing for massive codebases
- **Semantic descriptions**: Human-readable names for semantic types
- **Advanced graph analysis**: Call graphs, dependency analysis
- **Incremental parsing**: Parse only changed files
- **Git-aware analysis**: Integration with git history for change analysis

## Current Status

✅ **Fully Implemented:**
- 27 programming languages with full semantic support
- 8-bit semantic type taxonomy (64 semantic categories)
- Streaming multi-file analysis with glob patterns
- Automatic language detection
- Comprehensive error handling
- Production-ready performance
- Complete set of semantic predicate macros

✅ **Languages Supported:**
- **Web**: JavaScript, TypeScript, HTML, CSS
- **Systems**: C, C++, Go, Rust, Zig
- **Scripting**: Python, Ruby, PHP, Lua, R, Bash
- **Enterprise**: Java, C#, Kotlin, Swift, Dart
- **Data/Query**: SQL, DuckDB, GraphQL, JSON
- **Config**: HCL (Terraform), TOML
- **Docs**: Markdown

---

*This extension transforms traditional AST analysis from complex language-specific parsing into universal semantic understanding, enabling AI agents to analyze code at the conceptual level across all programming languages.*