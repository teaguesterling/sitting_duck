# Feature: Add Native Extractors for Remaining Languages

## Status: Open

## Priority: Medium

## Summary

Nine languages use the `FUNCTION_CALL` native extraction strategy but lack specialized native extractors. This means:
- `name` column works correctly (via `FIND_CALL_TARGET`)
- `qualified_name` column returns NULL for calls
- `parameters` column returns empty array for calls

## Affected Languages

| Language | Adapter | Notes |
|----------|---------|-------|
| C# | CSharpAdapter | Common enterprise language |
| Lua | LuaAdapter | Game scripting, embedded |
| HCL | HCLAdapter | Terraform configs |
| Zig | ZigAdapter | Systems programming |
| Haskell | (none) | Functional, needs adapter too |
| F# | (none) | Functional, needs adapter too |
| Julia | (none) | Scientific computing, needs adapter too |
| Scala | (none) | JVM functional, needs adapter too |
| GraphQL | GraphQLAdapter | Uses CUSTOM strategy - special case |

## Current Behavior

For these languages, call expressions like `obj.method(arg1, arg2)` return:
```
name: "method"           -- Works (FIND_CALL_TARGET)
qualified_name: NULL     -- Missing (no native extractor)
parameters: []           -- Missing (no native extractor)
```

## Expected Behavior

```
name: "method"
qualified_name: "obj.method"
parameters: ["arg1", "arg2"]
```

## Implementation Steps

For each language:

1. **Create native extractor file** (if not exists):
   - `src/include/{language}_native_extractors.hpp`

2. **Add FUNCTION_CALL specialization**:
   ```cpp
   template<>
   struct {Language}NativeExtractor<NativeExtractionStrategy::FUNCTION_CALL> {
       static NativeContext Extract(TSNode node, const string& content) {
           return UnifiedFunctionCallExtractor<{Language}LanguageTag>::Extract(node, content);
       }
   };
   ```

3. **Add language tag** (in `function_call_extractor.hpp`):
   ```cpp
   struct {Language}LanguageTag {
       static string GetLanguageName() { return "{language}"; }
   };
   ```

4. **Add to LANGUAGE_FUNCTION_CALL_TYPES map** (if needed):
   - Define language-specific node types for call expressions
   - Map argument node types, punctuation, etc.

5. **Add NativeExtractionTraits specialization** (in `native_context_extraction.hpp`):
   ```cpp
   template<>
   struct NativeExtractionTraits<{Language}Adapter> {
       template<NativeExtractionStrategy Strategy>
       using ExtractorType = {Language}NativeExtractor<Strategy>;
   };
   ```

6. **Include the extractor header** at bottom of `native_context_extraction.hpp`

## Effort Estimate

- Per language (with existing adapter): ~30 min
- Per language (needs new adapter): ~1-2 hours
- Total: ~6-10 hours

## Dependencies

- Requires understanding of each language's tree-sitter node types for:
  - Call expressions
  - Member/method access
  - Arguments/argument lists
  - Named parameters (if applicable)

## Related

- Commit `e03ae29`: Updated all language configs to use FIND_CALL_TARGET
- Commit `a8878a6`: Redesign native extraction model for calls

## Notes

- GraphQL is a special case - it uses CUSTOM strategy for fragment_spread which may be intentional
- Haskell, F#, Julia, Scala need both adapters AND extractors (more work)
- Languages with existing adapters (C#, Lua, HCL, Zig) are lower-effort
