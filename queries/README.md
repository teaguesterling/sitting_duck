# Queries Directory

This directory contains SQL query libraries for AST analysis.

## Current Libraries

### `ast_queries.sql` - **Master Query Library**
- **Use this for new projects** - comprehensive, simplified API
- Unified search, analysis, and navigation functions
- Designed for SQL-first workflow with Python orchestration
- Simplified name resolution (no complex qualified name parsing)

### Legacy Libraries (`legacy/`)

#### `ast-navigator.sql` - **Complex Function Extraction**
- Advanced C++ function extraction with tree-walking
- Handles complex qualified names and namespaces
- More sophisticated than current master library
- **Use for**: Complex C++ codebases requiring precise function detection

#### `ast-nav-parquet.sql` - **Parquet Indexing System**  
- Efficient parquet-based indexing for large codebases
- Incremental update capabilities
- High-performance queries on cached indexes
- **Use for**: Large-scale analysis where performance is critical

## Migration Path

1. **Start with**: `ast_queries.sql` (master library)
2. **For complex C++ projects**: Also load `legacy/ast-navigator.sql`
3. **For large-scale analysis**: Use `legacy/ast-nav-parquet.sql` indexing

## Loading Libraries

```sql
-- Load master library (recommended)
.read queries/ast_queries.sql

-- Additional libraries if needed
.read queries/legacy/ast-navigator.sql
.read queries/legacy/ast-nav-parquet.sql
```