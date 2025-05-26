# Performance Optimization Roadmap

**Source**: Peer Review Post-PR Analysis
**Priority**: P1 (Critical for scaling)
**Status**: Planning

## Executive Summary

Performance issues with 10K+ nodes require architectural adjustments. Current JSON operations in SQL are the bottleneck.

## Immediate Optimizations

### Option A: Hybrid C++ Implementation (Recommended)
- Keep SQL macros as fallback/reference
- Override with C++ when extension is loaded
- Maintain identical API surface
- Expected improvement: 10-100x faster than SQL macro equivalent

### Option B: Materialized AST Tables
```sql
CREATE TABLE project_ast AS 
SELECT * FROM read_ast('src/**/*.py', 'python');

CREATE INDEX idx_ast_type ON project_ast(type);
CREATE INDEX idx_ast_parent ON project_ast(parent_id);
CREATE INDEX idx_ast_name ON project_ast(name);
```

## Filter Pushdown Implementation

Add pushdown filters during parsing:
- type_filters
- max_depth
- line_range

Early termination during parsing for better performance.

## Three-Tier Performance Strategy

### Tier 1: Immediate Relief (Days)
- C++ implementation of hot-path macros (ast_find_type, ast_filter, ast_extract_names)
- Result size limiting: LIMIT pushdown
- Query complexity analyzer with warnings

### Tier 2: Structural Improvements (Weeks)
- Columnar storage for JSON nodes
- Prepared statement caching for repeated patterns
- Lazy evaluation for method chains

### Tier 3: Architectural Evolution (Months)
- Native graph storage backend
- Distributed parsing for large codebases
- Incremental index maintenance

## Benchmarking Strategy

### Current Performance
- Parsing Phase: ~100-500ms for large files (acceptable)
- Query Phase: JSON operations scale poorly O(n) per operation
- Target: O(log n) with indexing or O(1) with preprocessing

### Performance Tests Needed
```sql
-- Performance regression test
SELECT COUNT(*) FROM read_ast('test/fixtures/large_file_10k_nodes.py', 'python')
WHERE execution_time < 1000;  -- Must complete in < 1 second
```

## Key Questions to Address

1. **Performance Targets**: What's acceptable query latency for AI agents?
2. **Scale Requirements**: Single files, projects, or entire repositories?
3. **Query Patterns**: Most common AI agent query patterns?
4. **Deployment Model**: Local, cloud, or both?
5. **Language Roadmap**: Which languages after Python?