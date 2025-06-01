# Smart Pointer Refactor for Tree-sitter Objects

## Overview
Replace raw pointer management with smart pointers to prevent memory leaks and double-delete bugs.

## Motivation
- Current code uses raw pointers with manual `ts_parser_delete()` calls
- Led to segfault due to double-delete after parser ownership refactor
- Smart pointers would make ownership explicit and automatic

## Implementation Plan

### 1. Custom RAII Wrappers
Create type-safe wrappers for tree-sitter C objects:

```cpp
// TSParserPtr with custom deleter
using TSParserPtr = std::unique_ptr<TSParser, decltype(&ts_parser_delete)>;

// Or full RAII wrapper
class TSParserWrapper {
    TSParser* parser;
public:
    TSParserWrapper() : parser(ts_parser_new()) {}
    ~TSParserWrapper() { if (parser) ts_parser_delete(parser); }
    TSParser* get() const { return parser; }
    operator TSParser*() const { return parser; }
    
    // Delete copy constructor and assignment
    TSParserWrapper(const TSParserWrapper&) = delete;
    TSParserWrapper& operator=(const TSParserWrapper&) = delete;
    
    // Implement move semantics
    TSParserWrapper(TSParserWrapper&& other) noexcept 
        : parser(other.parser) { other.parser = nullptr; }
};
```

### 2. Update LanguageHandler
```cpp
class LanguageHandler {
protected:
    // Old: mutable TSParser* parser = nullptr;
    mutable std::unique_ptr<TSParserWrapper> parser;
    
    virtual void InitializeParser() const override {
        parser = std::make_unique<TSParserWrapper>();
        ts_parser_set_language(parser->get(), tree_sitter_python());
    }
};
```

### 3. Similar wrappers needed for:
- `TSTree` → `TSTreeWrapper` (with `ts_tree_delete`)
- `TSQuery` → `TSQueryWrapper` (with `ts_query_delete`) 
- `TSQueryCursor` → `TSQueryCursorWrapper` (with `ts_query_cursor_delete`)

### 4. Benefits
- Automatic cleanup on scope exit
- No manual delete calls needed
- Exception safety
- Clear ownership semantics
- Prevents double-delete bugs

## Priority
Medium - This is a code quality improvement that would prevent future bugs but isn't blocking functionality.

## Dependencies
- Should be done after current parser ownership refactor is stable
- Will touch most files that interact with tree-sitter

## Notes
- Consider making this part of a broader C++14/17 modernization pass
- Could use `std::shared_ptr` if parsers need to be shared across threads