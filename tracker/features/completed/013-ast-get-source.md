# ast_get_source() - Extract Source Code with Context

**Source**: Peer Review Feedback
**Priority**: P1 (High - Critical for practical use)
**Status**: Implemented

## What's Implemented

### Core source extraction macros (in `file_utilities.sql`)
- `ast_get_source(file_path, start_line, end_line)` — Extract lines as a single string
- `ast_get_source_numbered(file_path, start_line, end_line)` — Extract with line numbers prefixed
- `ast_get_source_line(file_path, line_num)` — Get a single line

### High-level lookup macro
- `ast_source_of(file_patterns, target_name, language := NULL, kind := NULL)` — Find a definition by name and return numbered source

```sql
-- Simple usage
SELECT * FROM ast_source_of('src/**/*.py', 'process_payment');

-- With kind filter
SELECT * FROM ast_source_of('src/**/*.py', 'MyClass', kind := 'class');

-- With explicit language
SELECT * FROM ast_source_of('src/**/*.py', 'handler', language := 'python', kind := 'function');
```

Returns: `file_path`, `name`, `definition_kind`, `start_line`, `end_line`, `source`

### Test coverage
- `test/sql/file_utilities.test` — covers all macros including multi-file glob patterns

## Remaining Gaps

- **Context lines parameter**: No `context_lines` option to show N lines before/after a node
- **Node-level integration**: Can't pass a `node_id` directly — requires `file_path, start_line, end_line`
- **Lateral join limitation**: `read_text()` doesn't support lateral joins, so `ast_get_source()` can't be called with column references from a query (must use literal path). `ast_source_of()` works around this by joining file contents differently.
- **Method chaining**: `.get_source()` chain syntax not yet available (depends on #023)

## Implementation History

- Initial `ast_get_source/numbered/line` macros — added as scalar macros
- `ast_source_of` table macro — added 2026-03-04 (commit ef823da)
- NULL language fallback in `read_ast` two-arg form — same commit
