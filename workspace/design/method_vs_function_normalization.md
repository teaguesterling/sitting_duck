# Method vs Function Normalization Design

## Problem
Python and C++ use the same AST node type (`function_definition`) for both free functions and class methods, making it impossible to differentiate them using static type mappings alone.

## Current State
- **JavaScript**: ✅ Already differentiates (`function_declaration` vs `method_definition`)
- **Python**: ❌ Both use `function_definition`
- **C++**: ❌ Both use `function_definition`

## Proposed Solution

### Option 1: Context-Aware GetNormalizedType (Recommended)
Update the LanguageHandler interface to pass parent context:

```cpp
// Current
virtual string GetNormalizedType(const string &node_type) const = 0;

// Proposed
virtual string GetNormalizedType(const string &node_type, 
                                 const ASTNode *parent = nullptr) const = 0;
```

Then in language handlers:
```cpp
string PythonLanguageHandler::GetNormalizedType(const string &node_type, 
                                                const ASTNode *parent) const {
    if (node_type == "function_definition") {
        if (parent && parent->type == "class_definition") {
            return NormalizedTypes::METHOD_DECLARATION;
        }
        return NormalizedTypes::FUNCTION_DECLARATION;
    }
    // ... rest of mappings
}
```

### Option 2: Post-Process Normalization
Keep current interface but add a post-processing step in read_ast_function.cpp that updates normalized types based on parent-child relationships.

### Option 3: Language-Specific Node Walking
Each language handler could provide its own node visitor that handles context during the initial parse.

## Recommendation
Option 1 is cleanest - it keeps the logic in the language handlers where it belongs, with minimal changes to the interface.