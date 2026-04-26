# Bug #009: Async/modifier keywords not extracted across languages

**Status:** Resolved
**Severity:** Medium
**Component:** Native context extraction
**Reported:** 2026-04-19
**Resolved:** 2026-04-25
**PRs:** #69

## Summary

Async and other keyword modifiers (static, public, suspend, unsafe, etc.) were not being extracted into the `modifiers[]` column for functions in most languages. Only JavaScript/TypeScript had working async extraction via dedicated `async_function_declaration` node types.

## Root Cause

Each language's tree-sitter grammar places modifier keywords in different locations relative to the function node. The extractors assumed a single structure that only matched some grammars:

- **Python**: `ASYNC_FUNCTION` strategy never triggered (tree-sitter emits `function_definition` with `async` child, not `async_function_definition`)
- **Rust**: `function_modifiers` is a child of `function_item`, extractor only checked siblings
- **Kotlin**: `modifiers` node is a direct child of `function_declaration`, extractor only checked parent's children
- **Swift**: `async`/`throws` are standalone children (not inside `modifiers` node); `visibility_modifier` wasn't in the allow-list
- **Dart**: `function_body` (containing async marker) is a sibling of `function_signature`, not a child

## Fix

Each language's `FUNCTION_WITH_PARAMS` extractor was updated to check the correct tree-sitter structure. A shared `EnsureModifier()` helper was added to prevent duplicates across all `ASYNC_FUNCTION` fallback paths.

## Languages fixed

| Language | Modifiers now extracted |
|----------|----------------------|
| Python | async |
| JavaScript | async, static |
| TypeScript | async, static |
| Rust | async, pub, unsafe, extern, const |
| Kotlin | suspend, private, inline |
| Swift | async, throws, public, static, mutating |
| Dart | async, async*, sync*, static |
| C# | (already worked: public, async, static, private, virtual) |
