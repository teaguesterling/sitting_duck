# Bug #008: Segfault when filtering class types with name IS NOT NULL

## Summary
DuckDB AST extension crashes with "INTERNAL Error: Attempted to dereference unique_ptr that is NULL!" when combining specific filter conditions.

## Reproducer
```sql
LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- This crashes:
SELECT name FROM read_parquet('.index-cpp.parquet') 
WHERE type LIKE '%class%' AND name IS NOT NULL LIMIT 1;
```

## Working Cases
```sql
-- These all work fine:
SELECT COUNT(*) FROM read_parquet('.index-cpp.parquet') WHERE type LIKE '%class%';
SELECT file_path FROM read_parquet('.index-cpp.parquet') WHERE type LIKE '%class%' LIMIT 1;
SELECT type FROM read_parquet('.index-cpp.parquet') WHERE type LIKE '%class%' LIMIT 1;  
SELECT name FROM read_parquet('.index-cpp.parquet') WHERE type LIKE '%class%' LIMIT 1;
```

## Analysis
The crash occurs specifically when:
1. Filtering by `type LIKE '%class%'` 
2. **AND** filtering by `name IS NOT NULL`
3. **AND** selecting the `name` column

**Scale-dependency confirmed**: 
- Small datasets (100-200 nodes) do NOT trigger the crash
- Large production dataset (.index-cpp.parquet with 24M+ nodes) consistently crashes
- Suggests memory pressure or data-specific issues in the large dataset

This suggests a memory management bug in the AST extension when certain filter combinations are applied to the name column at scale.

## Stack Trace
```
INTERNAL Error: Attempted to dereference unique_ptr that is NULL!
./build/release/duckdb(+0x5e941a)
./build/release/duckdb(+0x5e9508) 
./build/release/duckdb(+0x5ecdf5)
./build/release/duckdb(+0x1977e67)
[... full stack trace in test file]
```

## Priority
**CRITICAL** - This is a segfault in our extension that could crash user processes.

## Workaround
Avoid combining `type LIKE '%class%'` with `name IS NOT NULL` filters when selecting the name column. Use separate queries or different filter patterns.

## Impact
- Affects any query trying to find named class-related AST nodes
- Breaks the `ast classes` command and similar functionality
- Could cause crashes in production usage

## Next Steps
1. Debug the AST extension C++ code for memory management issues
2. Look specifically at how `name` column values are handled with filtering
3. Check for null pointer dereferences in the parquet reading code
4. Add unit tests to prevent regression