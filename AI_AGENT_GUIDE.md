# AI Agent Guide to DuckDB AST Extension

**Last Updated:** December 2024  
**Extension Version:** Current (Semantic Taxonomy + Streaming API)

## Overview

The DuckDB AST Extension provides a **universal semantic taxonomy** for understanding code across programming languages. Instead of learning language-specific node types, AI agents can work with standardized semantic categories that remain consistent whether analyzing Python, JavaScript, C++, or any of the 12 supported languages.

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

-- Find all functions regardless of language using semantic types
SELECT name, type, file_path FROM read_ast('code.py') 
WHERE semantic_type = 115; -- DEFINITION_FUNCTION
```

### 3. Multi-File Analysis
```sql
-- Analyze entire directories with glob patterns
SELECT file_path, COUNT(*) as nodes_per_file 
FROM read_ast('src/**/*.py', ignore_errors := true)
GROUP BY file_path;

-- Cross-language analysis
SELECT language, COUNT(*) as total_functions
FROM read_ast('**/*.{py,js,cpp}', ignore_errors := true)
WHERE semantic_type = 115 -- DEFINITION_FUNCTION
GROUP BY language;
```

## Supported Languages

The extension supports **12 programming languages** with automatic detection:

| Language | Extensions | Semantic Support |
|----------|------------|------------------|
| **Python** | `.py` | ✅ Full |
| **JavaScript** | `.js`, `.jsx` | ✅ Full |
| **TypeScript** | `.ts`, `.tsx` | ✅ Full |
| **C++** | `.cpp`, `.hpp`, `.cc`, `.h` | ✅ Full |
| **C** | `.c`, `.h` | ✅ Full |
| **Java** | `.java` | ✅ Full |
| **Go** | `.go` | ✅ Full |
| **Ruby** | `.rb` | ✅ Full |
| **SQL** | `.sql` | ✅ Full |
| **CSS** | `.css` | ✅ Basic |
| **HTML** | `.html`, `.htm` | ✅ Basic |
| **Markdown** | `.md` | ✅ Basic |

### Cross-Language Function Finding
```sql
-- Find all functions regardless of language using semantic types
SELECT name, type, language, file_path FROM read_ast('**/*.*', ignore_errors := true) 
WHERE semantic_type = 115; -- DEFINITION_FUNCTION

-- Same query works across Python, JavaScript, C++, etc.
SELECT COUNT(*) as function_count, language
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 115
GROUP BY language;
```

## Advanced Usage with Semantic Types

### Pattern Matching with Bit Operations
```sql
-- Find all arithmetic operations across languages
SELECT file_path, type, name, language
FROM read_ast('**/*.{py,js,cpp}', ignore_errors := true)
WHERE (semantic_type & 0xF0) = 64 -- OPERATOR kind
  AND (semantic_type & 0x0C) = 0;  -- ARITHMETIC super type

-- Find all conditionals across languages  
SELECT COUNT(*) as conditional_count, language
FROM read_ast('src/**/*.*', ignore_errors := true)
WHERE semantic_type = 136 -- FLOW_CONDITIONAL
GROUP BY language;
```

### Cross-Language Refactoring Analysis
```sql
-- Find assignment patterns to refactor
SELECT file_path, type, name, semantic_type, start_line
FROM read_ast('**/*.{py,js,cpp}', ignore_errors := true) 
WHERE semantic_type = 140 -- EXECUTION_MUTATION
ORDER BY file_path, start_line;

-- Find complex functions (high depth) for refactoring candidates
SELECT file_path, name, depth, descendant_count
FROM read_ast('**/*.py', ignore_errors := true)
WHERE semantic_type = 115 AND depth > 3 -- Deep function definitions
ORDER BY descendant_count DESC;
```

## API Reference

### Core Table Function

#### `read_ast(file_pattern, language?, options...)`
The main table function for parsing and analyzing code:

```sql
-- Single file
read_ast('script.py')
read_ast('script.js', 'javascript')

-- Multiple files with glob patterns  
read_ast('src/**/*.py')
read_ast('**/*.{js,ts,py}')

-- With options
read_ast('src/**/*.*', ignore_errors := true)
read_ast('script.py', peek_size := 200)
read_ast('script.py', peek_mode := 'lines')
```

**Returns columns:**
- `node_id`: Unique identifier for each AST node
- `type`: Language-specific node type (e.g., 'function_definition')
- `name`: Node name/identifier (if applicable)
- `file_path`: Source file path
- `language`: Detected or specified language
- `start_line`, `start_column`, `end_line`, `end_column`: Position info
- `parent_id`: Parent node ID (for tree structure)
- `depth`: Nesting depth in the AST
- `sibling_index`: Position among siblings
- `children_count`, `descendant_count`: Tree size metrics
- `peek`: Sample of source code for this node
- `semantic_type`: 8-bit universal semantic category
- `universal_flags`: Additional semantic flags
- `arity_bin`: Binned arity for analysis

### Language Support Functions

```sql
-- Get list of supported languages
SELECT * FROM ast_supported_languages();

-- Parse code string directly (scalar function)
SELECT parse_ast_objects('def hello(): pass', 'python');
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
    COUNT(CASE WHEN semantic_type = 115 THEN 1 END) as functions,
    COUNT(CASE WHEN semantic_type = 119 THEN 1 END) as classes,
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
    COUNT(CASE WHEN semantic_type = 115 THEN 1 END) as function_count
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
WHERE semantic_type = 115 -- DEFINITION_FUNCTION
  AND (name ILIKE '%test%' OR name ILIKE '%spec%')
ORDER BY file_path, start_line;

-- "Find all error handling constructs"
SELECT file_path, type, language, COUNT(*) as error_handling_count
FROM read_ast('**/*.*', ignore_errors := true) 
WHERE (semantic_type & 0xF0) = 128 -- ERROR_HANDLING kind
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
  AND semantic_type IN (115, 119) -- Functions or classes
ORDER BY depth DESC, descendant_count DESC;

-- "Find files with high cyclomatic complexity indicators"
SELECT 
    file_path,
    COUNT(CASE WHEN semantic_type = 136 THEN 1 END) as conditionals,
    COUNT(CASE WHEN semantic_type = 132 THEN 1 END) as loops,
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
WHERE semantic_type = 115 AND name IS NOT NULL
ORDER BY file_path, start_line;
```

## Best Practices for AI Agents

### 1. Use Semantic Types for Cross-Language Analysis
```sql
-- ✅ GOOD: Works across all languages
SELECT * FROM read_ast('**/*.*') WHERE semantic_type = 115; -- DEFINITION_FUNCTION

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
-- Find all LITERAL types (any super type)
WHERE (semantic_type & 0xF0) = 0;

-- Find all OPERATOR types  
WHERE (semantic_type & 0xF0) = 64;

-- Find all CONTROL_FLOW constructs
WHERE (semantic_type & 0xC0) = 128;
```

### 4. Use File Patterns Effectively
```sql
-- Specific languages
read_ast('**/*.py')           -- Python only
read_ast('**/*.{js,ts}')      -- JavaScript/TypeScript
read_ast('**/*.{cpp,hpp,h}')  -- C/C++

-- All supported languages
read_ast('**/*.*', ignore_errors := true)
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
           COUNT(CASE WHEN semantic_type = 115 THEN 1 END) as functions
    FROM read_ast('**/*.*', ignore_errors := true)
    GROUP BY language
),

-- 3. Find complex functions needing attention
complex_functions AS (
    SELECT file_path, name, depth, descendant_count
    FROM read_ast('**/*.*', ignore_errors := true)
    WHERE semantic_type = 115 AND depth > 5
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

-- Stream processing - no memory limits
-- Results are processed in chunks, suitable for massive codebases

-- Filter early for performance
SELECT file_path, COUNT(*) as function_count
FROM read_ast('**/*.py', ignore_errors := true)
WHERE semantic_type = 115  -- Filter at source
GROUP BY file_path;
```

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

-- ❌ INCORRECT: Missing recursiveness
read_ast('*.py')              -- Only current directory
read_ast('src/*.*')           -- Only immediate src/ children
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
- **Language extensions**: PHP, Rust, Kotlin support

## Current Status

✅ **Fully Implemented:**
- 12 programming languages with full semantic support
- 8-bit semantic type taxonomy (64 semantic categories)
- Streaming multi-file analysis with glob patterns
- Automatic language detection
- Comprehensive error handling
- Production-ready performance

✅ **Languages Supported:**
- **Full semantic support**: Python, JavaScript, TypeScript, C++, C, Java, Go, Ruby, SQL
- **Basic support**: CSS, HTML, Markdown

---

*This extension transforms traditional AST analysis from complex language-specific parsing into universal semantic understanding, enabling AI agents to analyze code at the conceptual level across all programming languages.*