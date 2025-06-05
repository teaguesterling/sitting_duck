# AI Agent Guide to DuckDB AST Extension

## Overview

The DuckDB AST Extension provides a **universal semantic taxonomy** for understanding code across programming languages. Instead of learning language-specific node types, AI agents can work with standardized semantic categories that remain consistent whether analyzing Python, JavaScript, C++, or any supported language.

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

### Cross-Language Function Finding
```sql
-- Find all functions regardless of language
SELECT * FROM parse_ast('code.py') 
WHERE semantic_type = 115; -- DEFINITION_FUNCTION

-- Same query works for JavaScript, C++, etc.
SELECT * FROM parse_ast('code.js')
WHERE semantic_type = 115;
```

### Semantic Pattern Matching
```sql
-- Find all arithmetic operations
SELECT * FROM parse_ast('code.py')
WHERE (semantic_type & 0xF0) = 64 -- OPERATOR kind
  AND (semantic_type & 0x0C) = 0;  -- ARITHMETIC super type

-- Find all conditionals across languages  
SELECT * FROM parse_ast('code.cpp')
WHERE semantic_type = 136; -- FLOW_CONDITIONAL
```

### Cross-Language Refactoring
```sql
-- Find assignment patterns to refactor
SELECT file_path, type, name, semantic_type 
FROM parse_ast('*.py') 
WHERE semantic_type = 140; -- EXECUTION_MUTATION
```

## Advanced Features (Future)

### Semantic Descriptions (Planned)
Instead of working with numeric codes, AI agents will be able to use normalized semantic names:

```sql
-- Future syntax (not yet implemented)
SELECT * FROM parse_ast('code.py')
WHERE semantic_description = '+';

-- Both Python '+' and any other language's addition operator
```

**Planned semantic normalization approach:**
- **Operators**: Use familiar symbols: `"+"`, `"&&"`, `"=="`, `"="`
- **Language constructs**: Use simple normalized terms: `"function_definition"`, `"if_statement"`
- **Cross-language mapping**: Python `"and"` → semantic `"&&"`, JS `"function_declaration"` → semantic `"function_definition"`

**Example mappings:**
```
Python "+"     → semantic "+"
C++ "+"        → semantic "+"
Python "and"   → semantic "&&"  
JS "&&"        → semantic "&&"
Python "True"  → semantic "true"
JS "true"      → semantic "true"
```

**Note**: The choice of canonical representation (e.g., `"&&"` vs `"and"` for logical AND) should be evaluated empirically with different AI models to determine what's most naturally interpretable. This may require A/B testing to optimize for AI understanding.

### Synonym System (Planned)
AI agents will be able to use natural language synonyms:

```sql
-- Future syntax (not yet implemented)
SELECT * FROM parse_ast('code.py')
WHERE semantic_synonym IN ('function', 'func', 'defn', 'definition', 'method');

-- All resolve to DEFINITION_FUNCTION
```

**Planned synonym groups:**
- Functions: `function`, `func`, `defn`, `def`, `method`, `procedure`
- Variables: `variable`, `var`, `identifier`, `name`, `binding`
- Conditionals: `if`, `conditional`, `branch`, `case`, `when`
- Loops: `loop`, `iteration`, `repeat`, `while`, `for`

## Best Practices for AI Agents

### 1. Use Semantic Types for Cross-Language Analysis
Instead of hardcoding language-specific node types, use semantic categories:

```python
# Bad - language specific
if node_type == "function_definition":  # Only works for Python

# Good - semantic
if semantic_type == DEFINITION_FUNCTION:  # Works across all languages
```

### 2. Leverage Bit Patterns for Efficient Filtering
```sql
-- Find all LITERAL types (any super type)
WHERE (semantic_type & 0xF0) = 0;

-- Find all OPERATOR types  
WHERE (semantic_type & 0xF0) = 64;

-- Find all ARITHMETIC operators specifically
WHERE semantic_type & 0xFC = 64;  -- OPERATOR + ARITHMETIC
```

### 3. Use Taxonomy for Code Understanding
The taxonomy enables semantic code analysis:
- **Code similarity**: Compare semantic patterns across languages
- **Refactoring**: Find equivalent constructs to transform
- **Documentation**: Generate language-agnostic explanations
- **Translation**: Map concepts between programming languages

## Integration with AI Workflows

### Code Analysis Pipeline
1. **Parse**: Use `parse_ast()` to get semantic types
2. **Filter**: Query by semantic categories, not raw types  
3. **Analyze**: Work with universal semantic vocabulary
4. **Generate**: Output language-agnostic insights

### Cross-Language Understanding
The taxonomy enables AI agents to:
- Explain code concepts universally ("This is a function definition")
- Find equivalent patterns across codebases
- Perform semantic diff analysis
- Build language-agnostic refactoring tools

## Current Status

✅ **Implemented:**
- 8-bit semantic type encoding
- 64 semantic constants (16 kinds × 4 super types)
- Cross-language mappings for Python, JavaScript, C++
- Universal SQL queries using semantic types

🚧 **Planned:**
- Human-readable semantic descriptions
- Synonym system for natural language queries
- Additional language support
- Semantic-aware AST manipulation functions

---

*This taxonomy transforms language-specific AST analysis into universal semantic code understanding, enabling AI agents to work with code at a conceptual level rather than syntax level.*