# Standardized Semantic Extraction API Design

## Vision Statement
Create universal SQL functions that extract high-level semantic information (functions, classes, parameters, types) from any supported programming language, enabling cross-language code analysis through a unified interface.

## Core Design Principles

### 1. Language Agnostic Interface
```sql
-- Same API works for any language
SELECT * FROM extract_functions('src/my_file.py');   -- Python
SELECT * FROM extract_functions('src/my_file.ts');   -- TypeScript  
SELECT * FROM extract_functions('src/my_file.sql');  -- SQL
```

### 2. Consistent Schema
All extraction functions return the same column structure regardless of source language:
```sql
-- extract_functions() schema
file_path VARCHAR, language VARCHAR, function_name VARCHAR, 
start_line INT, end_line INT, parameters VARCHAR[], return_type VARCHAR,
visibility VARCHAR, is_async BOOLEAN, is_static BOOLEAN
```

### 3. Hierarchical Extraction
```sql
-- Top level: Files and modules
SELECT * FROM extract_modules('src/**/*');

-- Mid level: Classes and interfaces  
SELECT * FROM extract_classes('src/**/*');

-- Detail level: Functions and methods
SELECT * FROM extract_functions('src/**/*');
SELECT * FROM extract_function_calls('src/**/*');
```

## API Functions Design

### Core Extraction Functions
```sql
-- Universal function extraction
extract_functions(path_pattern) → TABLE(
    file_path VARCHAR, language VARCHAR, function_name VARCHAR,
    start_line INT, end_line INT, parameters VARCHAR[], 
    return_type VARCHAR, visibility VARCHAR, 
    is_async BOOLEAN, is_static BOOLEAN, is_method BOOLEAN,
    parent_class VARCHAR, docstring VARCHAR
)

-- Class/interface extraction  
extract_classes(path_pattern) → TABLE(
    file_path VARCHAR, language VARCHAR, class_name VARCHAR,
    start_line INT, end_line INT, base_classes VARCHAR[],
    is_abstract BOOLEAN, visibility VARCHAR,
    method_count INT, property_count INT
)

-- Variable/constant extraction
extract_variables(path_pattern) → TABLE(
    file_path VARCHAR, language VARCHAR, variable_name VARCHAR,
    start_line INT, variable_type VARCHAR, initial_value VARCHAR,
    scope VARCHAR, is_constant BOOLEAN, visibility VARCHAR
)

-- Function call analysis
extract_function_calls(path_pattern) → TABLE(
    file_path VARCHAR, language VARCHAR, caller_function VARCHAR,
    called_function VARCHAR, call_line INT, call_type VARCHAR,
    is_external BOOLEAN, module_source VARCHAR
)
```

### Advanced Analysis Functions
```sql
-- Cross-language dependency mapping
analyze_dependencies(path_pattern) → TABLE(
    source_file VARCHAR, target_file VARCHAR, dependency_type VARCHAR,
    import_line INT, symbols VARCHAR[]
)

-- Type usage analysis
extract_type_usage(path_pattern) → TABLE(
    file_path VARCHAR, language VARCHAR, type_name VARCHAR,
    usage_context VARCHAR, usage_line INT, is_definition BOOLEAN
)

-- Complexity metrics
analyze_complexity(path_pattern) → TABLE(
    file_path VARCHAR, function_name VARCHAR, cyclomatic_complexity INT,
    parameter_count INT, line_count INT, nesting_depth INT
)
```

## Implementation Strategy

### Phase 1: Foundation (MVP)
**Goal**: Prove the concept with basic function extraction
- `extract_functions()` working across all 5 languages
- Consistent schema with core fields (name, location, basic metadata)
- File pattern support with language auto-detection

### Phase 2: Enhancement
**Goal**: Rich metadata and additional extraction types
- Add parameter details, return types, visibility
- Implement `extract_classes()` and `extract_variables()`  
- Add cross-language consistency validation

### Phase 3: Advanced Analysis
**Goal**: Relationship analysis and metrics
- `extract_function_calls()` for dependency analysis
- `analyze_dependencies()` for import/module mapping
- Performance optimization for large codebases

### Phase 4: Ecosystem Integration
**Goal**: Real-world tooling integration
- Export to standard formats (JSON, GraphQL schema)
- Integration with IDEs and analysis tools
- Documentation generation capabilities

## Technical Architecture

### 1. Abstract Semantic Extractor
```cpp
class SemanticExtractor {
public:
    virtual vector<FunctionInfo> ExtractFunctions(const ASTNodes& nodes) = 0;
    virtual vector<ClassInfo> ExtractClasses(const ASTNodes& nodes) = 0;
    virtual vector<VariableInfo> ExtractVariables(const ASTNodes& nodes) = 0;
    virtual vector<CallInfo> ExtractFunctionCalls(const ASTNodes& nodes) = 0;
};
```

### 2. Language-Specific Implementations
```cpp
class PythonSemanticExtractor : public SemanticExtractor {
    // Python-specific extraction logic
    // Handle def, async def, class, lambda, etc.
};

class TypeScriptSemanticExtractor : public SemanticExtractor {
    // TypeScript-specific extraction logic  
    // Handle function, arrow functions, interfaces, types, etc.
};
```

### 3. Unified Table Functions
```cpp
// DuckDB table function that coordinates everything
struct ExtractFunctionsData : public TableFunctionData {
    string path_pattern;
    vector<FunctionInfo> all_functions;  // Aggregated from all files
};
```

## Data Models

### FunctionInfo Structure
```cpp
struct FunctionInfo {
    string file_path;
    string language;
    string function_name;
    int start_line, end_line;
    vector<ParameterInfo> parameters;
    string return_type;
    string visibility;        // public, private, protected, internal
    bool is_async;
    bool is_static;
    bool is_method;
    string parent_class;      // if method
    string docstring;
};

struct ParameterInfo {
    string name;
    string type;
    string default_value;
    bool is_optional;
    bool is_variadic;        // *args, **kwargs, ...rest
};
```

## Language-Specific Extraction Strategies

### Python
- **Functions**: `function_definition`, `async_function_definition`
- **Classes**: `class_definition` 
- **Parameters**: Extract from `parameters` node, handle type hints
- **Returns**: Parse type annotations, docstring analysis

### TypeScript
- **Functions**: `function_declaration`, `arrow_function`, `method_definition`
- **Classes**: `class_declaration`, `interface_declaration`
- **Parameters**: Full type information from TS type system
- **Returns**: Explicit return type annotations

### SQL  
- **Functions**: `create_function`, stored procedures
- **Tables**: `create_table` as "classes"
- **Columns**: As "properties"
- **Queries**: `select_statement` as "methods"

### C++
- **Functions**: `function_definition` with complex declarator parsing
- **Classes**: `class_specifier`, `struct_specifier`
- **Templates**: Generic type handling
- **Namespaces**: Scope context

### JavaScript
- **Functions**: All function forms, handle hoisting
- **Classes**: `class_declaration`, prototype patterns
- **Modules**: Import/export analysis
- **Closures**: Scope chain analysis

## Success Metrics

### Technical
- ✅ Extract 90%+ of functions across all test codebases
- ✅ Sub-second performance on 1000+ file codebases  
- ✅ Cross-language consistency in schema
- ✅ Accurate parameter and return type detection

### User Experience
- ✅ Intuitive SQL API that "just works"
- ✅ Comprehensive documentation with examples
- ✅ Integration examples for common workflows
- ✅ Error handling with helpful messages

## Future Extensions

### Language Additions
- **Go**: Strong typing, interfaces, goroutines
- **Rust**: Ownership, traits, async/await
- **Java**: Classes, annotations, generics
- **PHP**: Dynamic typing, frameworks

### Analysis Capabilities
- **Security Analysis**: Detect potential vulnerabilities
- **Performance Analysis**: Identify bottlenecks
- **Architectural Analysis**: Detect patterns, anti-patterns
- **Documentation Generation**: Auto-generate API docs

This API would revolutionize how developers analyze and understand codebases, providing unprecedented cross-language insights through familiar SQL interfaces.