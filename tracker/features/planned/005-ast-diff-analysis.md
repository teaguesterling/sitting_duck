# AST Diff and Code Evolution Analysis

**Status**: Planned
**Priority**: Medium
**Estimated Effort**: High

## Description
Add capabilities to compare ASTs between different versions of files, enabling code evolution analysis.

## Proposed Functions
```sql
-- Compare two files
SELECT * FROM ast_diff('file_v1.py', 'file_v2.py', 'python');

-- Analyze git history
SELECT * FROM ast_git_history('file.py', 'python', '2024-01-01', '2024-01-23');

-- Find similar code patterns
SELECT * FROM ast_similar_patterns('pattern_file.py', '*.py', 'python', threshold => 0.8);
```

## Use Cases
- Code review automation
- Refactoring impact analysis  
- Finding duplicated code patterns
- Tracking function/class evolution over time
- Security vulnerability pattern matching

## Technical Challenges
- Efficient AST comparison algorithms
- Handling moved/renamed code
- Git integration
- Performance with large codebases