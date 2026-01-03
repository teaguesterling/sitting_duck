# Git-Aware AST Database Architecture

## Overview

This document outlines the design for a next-generation AST analysis system that replaces the current write-once Parquet files with a persistent DuckDB database capable of tracking code evolution through git commits.

## Current Limitations

### Parquet-Based Approach Issues
- **Write-once**: Cannot incrementally update indexes
- **No history**: Cannot track code changes over time  
- **No context**: Missing authorship, commit messages, branch info
- **Fragile**: Failed indexing loses all progress
- **Language separation**: HPP/CPP treated as different when they're the same

## Proposed Architecture

### Core Database Schema

```sql
-- Git commit metadata
CREATE TABLE git_commits (
    commit_hash VARCHAR PRIMARY KEY,
    commit_date TIMESTAMP,
    author_name VARCHAR,
    author_email VARCHAR,
    commit_message TEXT,
    branch VARCHAR,
    parent_commits VARCHAR[], -- Array of parent commit hashes
    is_merge BOOLEAN
);

-- File-level tracking
CREATE TABLE ast_files (
    file_path VARCHAR,
    commit_hash VARCHAR,
    file_size INTEGER,
    line_count INTEGER,
    last_modified TIMESTAMP,
    file_hash VARCHAR, -- SHA of file contents
    language VARCHAR, -- cpp, python, javascript, sql
    is_deleted BOOLEAN DEFAULT FALSE,
    PRIMARY KEY (file_path, commit_hash),
    FOREIGN KEY (commit_hash) REFERENCES git_commits(commit_hash)
);

-- AST node data
CREATE TABLE ast_nodes (
    file_path VARCHAR,
    commit_hash VARCHAR,
    node_id BIGINT,
    parent_id BIGINT,
    type VARCHAR,
    name VARCHAR,
    start_line INTEGER,
    end_line INTEGER,
    start_column INTEGER,
    end_column INTEGER,
    depth INTEGER,
    sibling_index INTEGER,
    children_count INTEGER,
    descendant_count INTEGER,
    semantic_type TINYINT,
    universal_flags TINYINT,
    arity_bin TINYINT,
    peek TEXT, -- Source code snippet
    PRIMARY KEY (file_path, commit_hash, node_id),
    FOREIGN KEY (file_path, commit_hash) REFERENCES ast_files(file_path, commit_hash)
);

-- Function-specific analysis (derived view)
CREATE VIEW ast_functions AS
SELECT 
    n.*,
    f.author_name,
    f.commit_date,
    f.commit_message
FROM ast_nodes n
JOIN ast_files af USING (file_path, commit_hash)
JOIN git_commits f ON af.commit_hash = f.commit_hash
WHERE n.type IN ('function_declarator', 'function_definition')
  AND (n.semantic_type = 112 OR n.semantic_type IS NULL);
```

### Indexing Strategy

```sql
-- Performance indexes
CREATE INDEX idx_ast_nodes_name ON ast_nodes(name) WHERE name IS NOT NULL;
CREATE INDEX idx_ast_nodes_type ON ast_nodes(type);
CREATE INDEX idx_ast_nodes_complexity ON ast_nodes(descendant_count);
CREATE INDEX idx_ast_files_language ON ast_files(language);
CREATE INDEX idx_git_commits_date ON git_commits(commit_date);
CREATE INDEX idx_git_commits_author ON git_commits(author_name);

-- Composite indexes for common queries
CREATE INDEX idx_function_evolution ON ast_nodes(name, file_path, commit_hash) 
WHERE type IN ('function_declarator', 'function_definition');
```

## Enhanced Command Interface

### Incremental Indexing
```bash
# Index from current git state
ast index-git cpp "src/**/*.cpp" "src/**/*.hpp"

# Index specific commit
ast index-commit abc123def cpp "src/**/*.cpp"

# Index commit range  
ast index-range main..feature-branch cpp "src/**/*.cpp"

# Update index with latest changes
ast update-index cpp
```

### Time Series Analysis
```bash
# Function evolution
ast history ParseExpression --since="2024-01-01"
ast history ParseExpression --commits=10

# Complexity trends
ast complexity-trend src/parser.cpp --function=ParseExpression
ast complexity-trend --author="john.doe@company.com"

# File evolution
ast file-history src/parser.cpp --metrics
```

### Advanced Git-Aware Queries
```bash
# Blame-style complexity analysis
ast blame-complexity src/parser.cpp --threshold=200

# Find functions that grew complex recently
ast complexity-growth --since="2024-01-01" --threshold=50

# Author analysis
ast author-stats john.doe@company.com
ast author-functions john.doe@company.com --complex=100

# Commit impact analysis  
ast commit-impact abc123def
ast commit-complexity abc123def

# Branch comparison
ast branch-diff main feature-branch --complexity
```

### Unified C++ Language Support
```bash
# Treat cpp/hpp as unified language
ast index-git cpp "**/*.cpp" "**/*.hpp" "**/*.h"
ast unified-cpp-stats
ast cpp-cross-references MyClass
```

## Data Collection Workflow

### 1. Git Integration
```bash
# Extract git metadata
git log --format='%H|%ai|%an|%ae|%s' > commits.csv

# Get file changes per commit
git diff-tree --no-commit-id --name-only -r $commit

# Get file content at specific commit
git show $commit:$file_path
```

### 2. Incremental Processing
```sql
-- Check what commits we already have
WITH missing_commits AS (
    SELECT commit_hash 
    FROM git_log_input 
    WHERE commit_hash NOT IN (SELECT commit_hash FROM git_commits)
)
-- Process only new commits
SELECT * FROM missing_commits ORDER BY commit_date;
```

### 3. Smart Change Detection
```sql
-- Only reparse files that actually changed
WITH changed_files AS (
    SELECT file_path, commit_hash, file_hash
    FROM git_file_changes 
    WHERE file_hash != (
        SELECT file_hash 
        FROM ast_files af 
        WHERE af.file_path = git_file_changes.file_path 
        ORDER BY commit_date DESC 
        LIMIT 1
    )
)
```

## Advanced Analytics Capabilities

### Code Evolution Metrics
```sql
-- Function complexity over time
SELECT 
    name,
    commit_date,
    descendant_count,
    LAG(descendant_count) OVER (PARTITION BY name, file_path ORDER BY commit_date) as prev_complexity,
    descendant_count - LAG(descendant_count) OVER (PARTITION BY name, file_path ORDER BY commit_date) as complexity_delta
FROM ast_functions 
WHERE name = 'ParseExpression'
ORDER BY commit_date;

-- Technical debt accumulation
SELECT 
    DATE_TRUNC('month', commit_date) as month,
    AVG(descendant_count) as avg_complexity,
    COUNT(*) FILTER (WHERE descendant_count > 200) as complex_functions,
    COUNT(*) as total_functions
FROM ast_functions
GROUP BY month
ORDER BY month;

-- Refactoring impact analysis
WITH function_changes AS (
    SELECT 
        name, file_path, commit_hash, commit_date,
        descendant_count,
        LAG(descendant_count) OVER (PARTITION BY name, file_path ORDER BY commit_date) as prev_complexity
    FROM ast_functions
)
SELECT 
    commit_hash,
    commit_message,
    author_name,
    COUNT(*) as functions_changed,
    SUM(descendant_count - prev_complexity) as total_complexity_change,
    AVG(descendant_count - prev_complexity) as avg_complexity_change
FROM function_changes fc
JOIN git_commits gc USING (commit_hash)
WHERE prev_complexity IS NOT NULL
GROUP BY commit_hash, commit_message, author_name
HAVING SUM(descendant_count - prev_complexity) < -100  -- Significant complexity reduction
ORDER BY total_complexity_change;
```

### Developer Insights
```sql
-- Developer complexity patterns
SELECT 
    author_name,
    COUNT(*) as commits,
    AVG(descendant_count) as avg_function_complexity,
    COUNT(*) FILTER (WHERE descendant_count > 200) as complex_functions_created,
    MAX(descendant_count) as max_complexity_created
FROM ast_functions af
JOIN git_commits gc USING (commit_hash)
GROUP BY author_name
ORDER BY avg_function_complexity DESC;

-- Code ownership analysis
WITH function_ownership AS (
    SELECT 
        name, file_path,
        author_name,
        COUNT(*) as modifications,
        MAX(commit_date) as last_modified
    FROM ast_functions af
    JOIN git_commits gc USING (commit_hash)
    GROUP BY name, file_path, author_name
),
primary_owners AS (
    SELECT 
        name, file_path,
        author_name,
        modifications,
        ROW_NUMBER() OVER (PARTITION BY name, file_path ORDER BY modifications DESC) as owner_rank
    FROM function_ownership
)
SELECT name, file_path, author_name as primary_owner, modifications
FROM primary_owners
WHERE owner_rank = 1
  AND modifications >= 3;  -- Only functions with significant ownership
```

## Implementation Phases

### Phase 1: Database Foundation
1. Create DuckDB schema
2. Implement git metadata extraction
3. Basic incremental indexing
4. Migrate existing parquet data

### Phase 2: Git Integration  
1. Commit-aware indexing
2. Change detection optimization
3. Branch/merge handling
4. Historical data backfill

### Phase 3: Advanced Analytics
1. Time series analysis commands
2. Developer insight queries
3. Refactoring impact analysis
4. Technical debt tracking

### Phase 4: Performance & Scale
1. Partitioning by date/project
2. Incremental view materialization
3. Query optimization
4. Data archival strategies

## Benefits

### For Code Analysis
- **Historical Context**: Understand how code evolved
- **Change Attribution**: Know who changed what and why
- **Impact Assessment**: Measure refactoring effectiveness
- **Technical Debt**: Track complexity accumulation over time

### For Team Management
- **Developer Patterns**: Identify coding style differences
- **Code Ownership**: Clear responsibility mapping
- **Review Insights**: Focus reviews on complexity growth
- **Training Needs**: Identify areas for developer education

### For Project Health
- **Complexity Trends**: Early warning for technical debt
- **Refactoring ROI**: Measure improvement impact
- **Architecture Evolution**: Track structural changes
- **Quality Metrics**: Long-term code health indicators

## Migration Strategy

1. **Parallel Operation**: Run both systems simultaneously
2. **Data Validation**: Ensure database matches parquet results
3. **Performance Testing**: Verify query performance
4. **Gradual Cutover**: Replace commands one by one
5. **Archive**: Keep parquet files as backup during transition

This architecture transforms AST analysis from static snapshots into dynamic code evolution tracking, enabling unprecedented insights into software development patterns and code quality trends.