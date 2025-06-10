# Grammar/Adapter Architecture Refactor

## Problem Statement
Currently we have redundant functionality split between `grammars.cpp` and `language_adapter.cpp`:

### Duplication Issues
1. **Duplicate TSLanguage declarations** in both files
2. **Split language mapping**: `GetLanguage()` vs adapter `InitializeParser()`  
3. **Inconsistent registry usage**: `GetSupportedLanguages()` uses adapters, `GetLanguage()` bypasses them
4. **Maintenance burden**: Adding new language requires changes in multiple files

## Current Architecture
```
grammars.cpp:
├── extern "C" TSLanguage declarations
├── GetLanguage(string) → TSLanguage*  
└── GetSupportedLanguages() → delegates to LanguageAdapterRegistry

language_adapter.cpp:
├── extern "C" TSLanguage declarations (duplicate!)
├── Each Adapter.InitializeParser() calls tree_sitter_*()
└── LanguageAdapterRegistry manages all adapters
```

## Proposed Architecture

### Phase 1: Consolidation
```
language_adapter.cpp (unified):
├── extern "C" TSLanguage declarations (single source)
├── LanguageAdapterRegistry::GetTSLanguage(string) → TSLanguage*
├── LanguageAdapterRegistry::GetSupportedLanguages() → vector<string>
└── Remove grammars.cpp entirely
```

### Phase 2: Clean Interface
```cpp
// Single point of access
auto& registry = LanguageAdapterRegistry::GetInstance();

// All language operations through same interface
const TSLanguage* lang = registry.GetTSLanguage("python");
const LanguageAdapter* adapter = registry.GetAdapter("python"); 
vector<string> supported = registry.GetSupportedLanguages();
```

## Implementation Plan

### Step 1: Move TSLanguage Access to Registry
```cpp
class LanguageAdapterRegistry {
public:
    const TSLanguage* GetTSLanguage(const string &language) const;
    // ... existing methods
};
```

### Step 2: Update Callers
- Find all `GetLanguage()` calls → replace with `registry.GetTSLanguage()`
- Update any direct tree_sitter_*() calls → use registry

### Step 3: Remove grammars.cpp
- Delete `src/grammars.cpp`
- Remove from `CMakeLists.txt`
- Update includes

### Step 4: Single TSLanguage Declaration File
- Create `src/include/tree_sitter_languages.hpp` with all declarations
- Include only where needed

## Benefits

### For Developers
- **Single responsibility**: LanguageAdapter owns everything about a language
- **Easier maintenance**: Add new language by just creating adapter class
- **Consistent API**: All language operations through same pathway

### For Architecture  
- **Reduced duplication**: No more split declarations
- **Clear ownership**: Registry is single source of truth
- **Better encapsulation**: TSLanguage details hidden in adapters

## Migration Strategy

1. **Backward compatibility**: Keep old API during transition
2. **Incremental**: Update callers one by one
3. **Test-driven**: Validate each step doesn't break functionality
4. **Clean removal**: Remove old code only after full migration

## Files to Modify
- `src/grammars.cpp` → delete
- `src/include/grammars.hpp` → delete  
- `src/language_adapter.cpp` → add GetTSLanguage()
- `src/include/language_adapter.hpp` → add GetTSLanguage()
- `CMakeLists.txt` → remove grammars.cpp
- Any callers of `GetLanguage()` → update to use registry

## Testing Plan
- Verify all 5 languages still work after refactor
- Test both explicit language parsing and auto-detection
- Validate performance hasn't regressed

## Success Criteria
- Single source of truth for all language operations
- No duplicate TSLanguage declarations  
- Same functionality with cleaner architecture
- Adding new language requires only adapter creation