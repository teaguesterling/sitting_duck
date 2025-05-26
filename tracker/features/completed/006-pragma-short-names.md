# Feature: Pragma for Short Function Names

**Status:** Completed
**Priority:** High
**Target:** v0.3.0

## Description
Implement a pragma or macro to register short (unprefixed) function names as aliases for the ast_* prefixed functions.

## Motivation
- Power users and AI agents in focused contexts can opt into shorter names
- Maintains discoverability with prefixes by default
- Avoids namespace pollution for general users

## Design
```sql
-- Option 1: Via PRAGMA
PRAGMA duckdb_ast_register_short_names = true;

-- Option 2: Via Macro  
SELECT duckdb_ast_register_short_names();
```

## Implementation
Create aliases for all main functions:
```sql
CREATE OR REPLACE MACRO get_type(nodes, types) AS ast_get_type(nodes, types);
CREATE OR REPLACE MACRO get_names(nodes, node_type) AS ast_get_names(nodes, node_type);
-- etc...
```

## Benefits
- Cleaner queries for power users
- Backward compatibility
- Opt-in safety