# Release Notes: v1.3.0

## Pattern Matching for AST Queries

This release introduces **pattern-by-example matching** - a powerful new way to search code using familiar syntax instead of manual AST queries.

### Headline Feature: `ast_match`

Write code patterns with wildcards to find matching structures:

```sql
-- Find all eval() calls and capture their arguments
SELECT * FROM ast_match('code', 'eval(__X__)', 'python');

-- Find functions with return statements (variadic body)
SELECT * FROM ast_match('code',
    'def __F__(__):
    %__<BODY*>__%
    return __Y__', 'python');

-- Cross-language: Python pattern matches JavaScript
SELECT * FROM ast_match('js_code', '__F__(__X__)', 'python',
    match_by := 'semantic_type');
```

### Wildcard Syntax

| Pattern | Description |
|---------|-------------|
| `__X__` | Named wildcard - captures matched node as 'X' |
| `__` | Anonymous wildcard - matches any node |
| `%__<BODY*>__%` | Variadic: matches 0+ siblings (HTML syntax) |
| `%__<X+ type=identifier>__%` | With constraints (Phase 1 parsing) |

### New SQL Macros

- `ast_match(table, pattern, lang)` - Find AST structures matching a pattern
- `ast_pattern(pattern, lang)` - Inspect how patterns are parsed
- `clean_pattern(str)` - Convert extended wildcards to simple form
- `parse_html_wildcard(str)` - Parse HTML-style wildcard attributes
- `is_pattern_wildcard(name)` - Check if name is a wildcard
- `wildcard_capture_name(name)` - Extract capture name from wildcard

### Parameters

- `match_syntax := false` - Include punctuation in matching
- `match_by := 'type'` - Match on `'type'` or `'semantic_type'` (cross-language)
- `depth_fuzz := 0` - Allow depth flexibility for cross-language patterns

---

## Other Improvements

### Structural Analysis Macros

New macros for code analysis (added in commits leading to v1.3.0):

- `ast_function_metrics(table)` - Cyclomatic complexity, return counts, nesting depth
- `ast_security_audit(table)` - Detect security anti-patterns (eval, exec, pickle, etc.)
- `ast_dead_code(table)` - Find potentially unused functions/classes
- `ast_nesting_analysis(table)` - Identify deeply nested code
- `ast_functions_containing(table, type)` - Find functions with specific node types
- `ast_function_scope(table, node_id)` - Get function body excluding nested functions
- `ast_class_members(table, node_id)` - Get direct class members

### Tree Navigation Macros

- `ast_children(table, node_id)` - Get immediate children
- `ast_descendants(table, node_id)` - Get entire subtree (O(1) using descendant_count)
- `ast_ancestors(table, node_id)` - Path from node to root
- `ast_siblings(table, node_id)` - Sibling nodes
- `ast_containing_line(table, line)` - Nodes containing a line
- `ast_definitions(table)` - All named definitions with categories
- `ast_call_arguments(table, call_id)` - Arguments of a function call

### File Utilities

- `read_lines(path)` - Read file as rows with line numbers
- `read_lines_range(path, start, end)` - Read specific line range
- `read_lines_context(path, line, context)` - Lines around a specific line
- `get_lines_text(path, start, end)` - Get lines as single string

### String Utilities

- `string_contains_any(str, patterns)` - Check if string contains any pattern
- `ast_peek_contains_any(table, patterns)` - Filter AST by peek content

### Semantic Predicate Helpers

Convenience macros for filtering by semantic type:
- `is_function_definition(st)`, `is_class_definition(st)`, `is_variable_definition(st)`
- `is_function_call(st)`, `is_member_access(st)`
- `is_string_literal(st)`, `is_number_literal(st)`, `is_boolean_literal(st)`
- `is_conditional(st)`, `is_loop(st)`, `is_jump(st)`
- `is_assignment(st)`, `is_comparison(st)`, `is_arithmetic(st)`, `is_logical(st)`
- `is_import(st)`, `is_export(st)`, `is_comment(st)`, `is_annotation(st)`

### Bug Fixes

- Fix multi-file pattern matching with proper file_path scoping
- Fix SQL statement splitter to handle `--` line comments
- Fix name extraction for Go methods, JS anonymous functions, C pointer returns
- Fix body detection for PHP, Ruby, Dart, R
- Fix Lua method name extraction for colon syntax
- Fix Ruby method name extraction for special cases

### Documentation

- New pattern matching guide: `docs/guide/pattern-matching.md`
- Comprehensive Doxygen documentation for all language type definitions
- Auto-generated language reference documentation
- Expanded cookbook with security and structural analysis examples

---

## Upgrade Notes

Pattern matching macros are now embedded in the extension - no need to `.read` external SQL files. Just `LOAD sitting_duck;` and use `ast_match()` directly.

## What's Next (Roadmap)

- **Phase 2**: Nested pattern constraints (`<contains>`, `<not-contains>`)
- **Phase 3**: C++ implementation for performance at scale
- Constraint application for parsed attributes (`type=`, `max-descendants=`, etc.)

---

**Full Changelog**: https://github.com/teaguesterling/sitting_duck/compare/v1.2.0...v1.3.0
