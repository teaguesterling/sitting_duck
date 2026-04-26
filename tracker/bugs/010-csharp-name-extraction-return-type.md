# Bug #010: C# name extraction picks up return type instead of method name

**Status:** Fixed
**Severity:** Medium
**Component:** C# language adapter / FIND_IDENTIFIER strategy
**Reported:** 2026-04-25
**Fixed:** 2026-04-26

## Summary

For C# methods with `async Task` or `async Task<T>` return types, the `name` column extracts `Task` (the return type) instead of the actual method name. This is because `FIND_IDENTIFIER` picks the first `identifier` node in the tree-sitter AST, and `Task` appears before the method name.

## Reproduction

```sql
LOAD sitting_duck;

SELECT name, modifiers FROM read_ast('test/data/modifier_audit/async_functions.cs', context := 'native')
WHERE is_function_definition(semantic_type) AND name != ''
ORDER BY start_line;
```

**Expected:** `PrivateAsync` with `modifiers = [private, async]`
**Actual:** `Task` with `modifiers = [private, async]`

## Root Cause

C# tree-sitter grammar for `private async Task PrivateAsync()` produces:
```
method_declaration
  modifier("private")
  modifier("async")
  identifier("Task")      <-- return type, but parsed as identifier
  identifier("PrivateAsync")  <-- actual method name
```

The `FIND_IDENTIFIER` strategy takes the first `identifier` child, which is the return type `Task` rather than the method name `PrivateAsync`.

## Possible Fixes

1. Use a named field lookup (`ts_node_child_by_field_name(node, "name", ...)`) instead of first-identifier scan
2. Skip identifiers that appear before the parameter list
3. Add C#-specific name extraction that skips type identifiers

## Impact

- Affects any C# method with a non-predefined return type (Task, List, etc.)
- Methods returning `void`, `int`, `string` etc. are unaffected (those are `predefined_type` nodes, not `identifier`)

## Fix

Used `ts_node_child_by_field_name(node, "name")` in the C# language adapter instead of the generic `FIND_IDENTIFIER` strategy (which takes the first `identifier` child). The `method_declaration` grammar node exposes a `name` field that points directly to the method name, bypassing the return type `identifier`. The fix was implemented in the C# adapter's name extraction path.
