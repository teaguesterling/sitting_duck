# Next Steps Plan - DuckDB AST Extension

## Current Status ✅

### Completed
- ✅ Language handler architecture implemented
- ✅ JSON escaping bug fixed  
- ✅ JavaScript method name extraction working
- ✅ C++ language support added (3 languages total: Python, JavaScript, C++)
- ✅ All 494 test assertions passing

### Languages Supported
- **Python**: function_definition, class_definition, normalized types
- **JavaScript**: function_declaration vs method_definition, property_identifier support
- **C++**: Fixed return type bug, handles function_declarator correctly

## Immediate Next Steps (Priority Order)

### 1. Add SQL Language Support 🎯
- **Goal**: Implement basic SQL parsing with current interface
- **Approach**: Phase 1 from design docs - accept limitations, document what doesn't work
- **Key Tasks**:
  - Add tree-sitter-sql submodule
  - Create SQLLanguageHandler class
  - Map SQL concepts to existing normalized types where possible
  - Create comprehensive SQL test file with DDL/DML/DQL
  - Document limitations for interface evolution

### 2. Interface Evolution Planning 🔧
- **Goal**: Design context-aware normalization
- **Tasks**:
  - Update LanguageHandler interface for NodeContext
  - Implement method vs function differentiation
  - Add multi-part name support (schema.table.column)
  - Plan backward compatibility strategy

### 3. Method vs Function Differentiation 🎨
- **Goal**: Use METHOD_DECLARATION properly  
- **Approach**: Add parent context to GetNormalizedType
- **Languages**: Python and C++ need this (JavaScript already works)

### 4. Self-Analysis & Validation 🔍
- **Goal**: Use our extension on our own C++ codebase
- **Focus**: Find patterns, validate our approach, discover edge cases
- **Areas**: Class hierarchies, function complexity, naming patterns

## Long-term Roadmap

### Phase 2: Interface Maturation
- Context-aware normalization
- Multi-part names (ExtractedName struct)
- Relationship hints between nodes

### Phase 3: Advanced Language Support
- Rust, Go, TypeScript consideration
- Language-specific normalized types
- Cross-language analysis capabilities

### Phase 4: Performance & Polish
- C++ hot-path implementation
- Streaming parser for large files
- Memory optimization

## Design Documents Created
- `workspace/design/language_handler_evolution_plan.md` - Interface evolution strategy
- `workspace/design/sql_language_design.md` - SQL-specific implementation plan
- `workspace/design/method_vs_function_normalization.md` - Context-aware normalization

## Key Architectural Insights Learned
1. **Static type mappings insufficient** - Need context for method vs function
2. **Each language reveals new patterns** - JavaScript property_identifier, C++ function_declarator
3. **Name extraction is complex** - Return types, operators, qualified names
4. **SQL will be fundamentally different** - Tables not classes, references not variables

## Commitment Strategy
- **Small, focused commits** for each language addition
- **Test-driven development** - ensure 100% test pass rate
- **Document limitations** - be honest about what doesn't work yet
- **Iterative refinement** - learn from usage before major interface changes

## Success Metrics
- All tests passing
- Clean separation of language-specific logic
- Easy to add new languages
- Useful for real-world AST analysis

Ready to tackle SQL language support next! 🚀