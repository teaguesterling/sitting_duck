# FIND_IDENTIFIER Missing variable_name and word Child Types

**Status:** Resolved
**Priority:** Medium
**GitHub Issue:** #28

## Problem

The `FIND_IDENTIFIER` extraction strategy in `ExtractByStrategy()` searches a fixed list of child node types to extract names from AST definition nodes. However, two tree-sitter node types were missing from this list:

- **`variable_name`**: Used by Bash (variable assignments) and PHP (parameters)
- **`word`**: Used by Bash (function names)

## Affected Node Types

| Language | Node Type | Fixed | Count | Example |
|----------|-----------|-------|-------|---------|
| Bash | `function_definition` | Yes | 5 | `function show_usage() {}` → `show_usage` |
| Bash | `variable_assignment` | 27/28 | 27 | `SCRIPT_NAME="simple.sh"` → `SCRIPT_NAME` |
| PHP | `simple_parameter` | Yes | 9 | `string $name` → `$name` |
| PHP | `property_declaration` | No | 3 | Needs deeper fix (see Known Remaining) |

## Known Remaining

- **Bash subscript assignments** (1 instance): `CONFIG_MAP["$basename"]="$file_size"` — the `variable_name` is nested inside a `subscript` node, not a direct child of `variable_assignment`. `FindChildByType` only searches direct children.
- **PHP `property_declaration`** (3 instances): `private string $name;` — the `variable_name` is nested inside a `property_element` child, not a direct child of `property_declaration`. Fixing this requires either a recursive search or a custom extraction strategy.

## Fix

Added two `FindChildByType` lookups to the `FIND_IDENTIFIER` case in `src/language_adapter.cpp`, after the existing identifier type checks:

```cpp
if (result.empty()) {
    result = FindChildByType(node, content, "variable_name");
}
if (result.empty()) {
    result = FindChildByType(node, content, "word");
}
```

No changes to `*_types.def` or `*_adapter.cpp` files were needed since these node types were already configured to use `FIND_IDENTIFIER`.

## Test Coverage

- `test/sql/name_extraction/bash_php_find_identifier.test`: Verifies name extraction for Bash function_definition, variable_assignment, and PHP simple_parameter

## Discovery

Identified by the name extraction audit (master-report.md, Issue 1 / Phase 1 Quick Win).
