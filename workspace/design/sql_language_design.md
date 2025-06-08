# SQL Language Support Design

## Overview

SQL is fundamentally different from procedural languages. Instead of functions and classes, we have tables, queries, and DDL statements. This requires rethinking our normalization strategy.

## SQL AST Challenges

1. **Statement-Oriented** rather than expression-oriented
2. **Schema Objects** (tables, views, indexes) instead of classes
3. **References** are contextual (CREATE vs SELECT)
4. **Multi-part names** everywhere (schema.table.column)

## Proposed SQL Normalizations

### Using Existing Types Where Possible

| SQL Node Type | Normalized Type | Notes |
|--------------|-----------------|-------|
| create_function | function_declaration | Stored procedures/functions |
| function_call | function_call | Already exists |
| line_comment | comment | -- comments |
| block_comment | comment | /* comments */ |

### New SQL-Specific Types Needed

| SQL Concept | Proposed Normalized Type | Examples |
|------------|-------------------------|----------|
| CREATE TABLE | table_declaration | CREATE TABLE users (...) |
| CREATE VIEW | view_declaration | CREATE VIEW v_active AS ... |
| CREATE INDEX | index_declaration | CREATE INDEX idx_name ON ... |
| Column definitions | column_definition | id INTEGER PRIMARY KEY |
| Table in FROM | table_reference | FROM users |
| Column in SELECT | column_reference | SELECT user_id |
| WITH clause | cte_declaration | WITH temp AS (...) |
| SELECT/INSERT/etc | query_statement | Top-level queries |
| Subquery | subquery_expression | (SELECT ... FROM ...) |

## Name Extraction Patterns

### CREATE Statements
```sql
CREATE TABLE schema.table_name -- Extract: table_name, qualifier: schema
CREATE VIEW view_name AS ...    -- Extract: view_name
CREATE INDEX idx ON table(col)  -- Extract: idx
```

### References
```sql
SELECT u.id FROM users u        -- Table: users (alias: u), Column: id
JOIN orders o ON u.id = o.uid   -- Table: orders (alias: o)
```

### Complex Cases
```sql
SELECT 
    u.name AS user_name,        -- Column: name, alias: user_name
    COUNT(*) AS total           -- Function: COUNT, alias: total
FROM db.schema.users u          -- Qualified: db.schema.users
```

## Implementation Approach

### Phase 1: Basic SQL Support (Current Interface)

```cpp
class SQLLanguageHandler : public LanguageHandler {
    // Map what we can to existing types
    {"create_table", "table_declaration"},  // Reuse pattern
    {"create_function", "function_declaration"},
    {"function_call", "function_call"},
    
    // For now, map SQL-specific to generic
    {"select_statement", "query_statement"},
    {"table_reference", "variable_reference"},  // Compromise
};
```

**Limitations**:
- Can't differentiate table references from column references
- No way to represent qualified names
- Loses SQL-specific semantics

### Phase 2: With Interface Updates

```cpp
string GetNormalizedType(const NodeContext &context) const override {
    if (context.node_type == "identifier") {
        // Look at parent to determine context
        if (context.parent_type == "create_table") {
            return "table_declaration";
        } else if (context.parent_type == "from_clause") {
            return "table_reference";
        } else if (context.parent_type == "select_clause") {
            return "column_reference";
        }
    }
    // ...
}
```

## Test Cases for SQL

### Basic DDL
```sql
-- Test: Table creation with columns
CREATE TABLE users (
    id INTEGER PRIMARY KEY,
    email VARCHAR(255) NOT NULL,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Test: View creation
CREATE VIEW active_users AS
SELECT * FROM users WHERE status = 'active';

-- Test: Index creation
CREATE INDEX idx_email ON users(email);
```

### Complex Queries
```sql
-- Test: CTEs, joins, aliases
WITH recent_orders AS (
    SELECT * FROM orders WHERE date > '2024-01-01'
)
SELECT 
    u.name AS customer_name,
    COUNT(o.id) AS order_count
FROM users u
JOIN recent_orders o ON u.id = o.user_id
GROUP BY u.id, u.name
HAVING COUNT(o.id) > 5;
```

### Name Extraction Tests
- Simple names: `users`, `email`, `idx_email`
- Qualified names: `public.users`, `users.email`
- Aliased names: `users AS u`, `COUNT(*) AS total`
- Reserved words: `"user"`, `[order]` (dialect-specific)

## Questions to Resolve

1. **Should we add SQL-specific normalized types now or try to reuse existing ones?**
   - Pro reuse: Simpler, works with current interface
   - Pro new types: More accurate, better for SQL-specific queries

2. **How to handle SQL dialects?**
   - PostgreSQL: "quoted identifiers"
   - MySQL: `backtick identifiers`
   - SQL Server: [bracket identifiers]

3. **What about DDL vs DML vs DQL?**
   - Should CREATE/ALTER/DROP be different from SELECT/INSERT?
   - Does it matter for normalization?

## Recommendation

1. **Start simple**: Implement with current interface, document limitations
2. **Learn from usage**: See what queries people actually want to run
3. **Iterate**: Add SQL-specific types after interface updates
4. **Consider a companion extension**: `duckdb_sql_ast` for SQL-specific analysis?