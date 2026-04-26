# ast_find_references() - Find All Uses of a Symbol

**Source**: Peer Review Feedback
**Priority**: P1 (High - Core navigation feature)
**Status**: Complete

## Overview

Find all references to a named symbol within a file, using scope-chain resolution to correctly associate references with their definitions.

## Implemented API

```sql
-- Find all definitions, references, and call sites for 'process'
SELECT ref_kind, node_type, start_line, scope_name, peek
FROM ast_find_references('src/main.py', 'process')
ORDER BY start_line;

-- Filter to just call sites
SELECT start_line, scope_name, peek
FROM ast_find_references('src/**/*.rs', 'helper')
WHERE ref_kind = 'call';

-- Count references per kind
SELECT ref_kind, COUNT(*)
FROM ast_find_references('src/app.js', 'Service')
GROUP BY ref_kind;
```

## Output Columns

| Column | Type | Description |
|--------|------|-------------|
| file_path | VARCHAR | Source file path |
| name | VARCHAR | Symbol name |
| ref_kind | VARCHAR | `definition`, `reference`, or `call` |
| node_type | VARCHAR | Tree-sitter node type (e.g., `function_item`, `identifier`) |
| start_line | UINTEGER | Line number |
| peek | VARCHAR | Source context |
| scope_name | VARCHAR | Containing scope name (or `<module>`) |
| def_node_id | BIGINT | Node ID of the resolved definition |

## Implementation

- **Location**: `src/sql_macros/scope_resolution.sql`
- **Signature**: `ast_find_references(source, target_name, language := NULL)`
- **Approach**: Inlines the `ast_resolve` pattern filtered to a target name
  - Finds all `is_name_definition` nodes matching the target
  - Finds all `is_name_reference` nodes matching the target (excluding `binds_name` identifiers)
  - Walks scope chains via `scope.stack` to resolve each reference to its innermost definition
  - Promotes references whose parent is a call node to `ref_kind = 'call'`
  - UNIONs definition sites with resolved reference sites

## Test Coverage

- **Test file**: `test/sql/scope_resolution/find_references.test`
- **Assertions**: 200 across Python, JavaScript, Rust, Go
- **Fixtures**: `test/data/find_references/refs.{py,js,rs,go}`
- **Scenarios**: Function refs, class/struct refs, variable shadowing, method refs, constructor calls, filtering patterns, empty results, def_node_id disambiguation
