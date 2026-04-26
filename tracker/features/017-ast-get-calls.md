# ast_get_calls() / ast_call_graph() - Call Extraction

**Source**: Peer Review Feedback
**Priority**: P1 (High - Essential for dependency analysis)
**Status**: Complete

## Overview

Extract function/method calls from source files with call-type classification and scope-aware caller attribution. Build caller→callee call graphs.

## Implemented API

```sql
-- All calls in a file with caller attribution and call type
SELECT caller_name, called_name, call_type, call_expression, start_line
FROM ast_get_calls('src/main.py');

-- Only method calls
SELECT * FROM ast_get_calls('src/**/*.rs')
WHERE call_type = 'method';

-- Aggregated call graph
SELECT caller, callee, call_type, call_count
FROM ast_call_graph('src/service.go');
```

## ast_get_calls Output Columns

| Column | Type | Description |
|--------|------|-------------|
| file_path | VARCHAR | Source file path |
| caller_name | VARCHAR | Containing function name (or `<module>`) |
| called_name | VARCHAR | Name of the called function/method |
| call_expression | VARCHAR | Full call expression (peek) |
| call_type | VARCHAR | `function`, `method`, `constructor`, or `macro` |
| language | VARCHAR | Source language |
| start_line | UINTEGER | Line number |
| node_id | BIGINT | Call node ID |
| caller_node_id | BIGINT | Caller function node ID |

## ast_call_graph Output Columns

| Column | Type | Description |
|--------|------|-------------|
| file_path | VARCHAR | Source file path |
| caller | VARCHAR | Caller function name |
| callee | VARCHAR | Called function name |
| call_type | VARCHAR | Call type classification |
| call_count | BIGINT | Number of calls |

## Call Type Classification

| Type | Detection Method |
|------|-----------------|
| `constructor` | Call node type is `new_expression`, `object_creation_expression`, etc. |
| `macro` | Call node type is `macro_invocation` |
| `method` | First child of call node is `attribute`, `member_expression`, `field_expression`, `selector_expression`, etc. |
| `function` | Default — first child is `identifier` |

## Implementation

- **Location**: `src/sql_macros/tree_navigation.sql`
- **Signatures**: `ast_get_calls(source, language := NULL)`, `ast_call_graph(source, language := NULL)`
- **Caller attribution**: O(1) via `scope.function` hash join (not range-join)
- **Method detection**: AST structural check on first child node type (no LIKE patterns)
- **Related**: `ast_callees`/`ast_callers` in `scope_resolution.sql` provide simpler caller/callee pairs without call-type classification

## Test Coverage

- **Test file**: `test/sql/tree_navigation/call_extraction.test`
- **Assertions**: 392 across Python, JavaScript, Rust, Go
- **Fixtures**: `test/data/call_extraction/calls.{py,js,rs,go}`
- **Scenarios**: Function calls, method calls, constructor calls, chained calls, macro calls, nested calls, module-level calls, call graph aggregation
