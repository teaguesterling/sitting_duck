#!/usr/bin/env python3
"""Fix the corrupted unified_ast_backend.cpp file."""

# Read the original file
with open('/mnt/aux-data/teague/Projects/duckdb_ast/src/unified_ast_backend.cpp', 'r') as f:
    content = f.read()

# Find where the ParseToASTResult function ends (at line 113)
lines = content.split('\n')
good_lines = lines[:113]  # Keep up to the closing brace of ParseToASTResult

# Find where the good functions start (PopulateSemanticFields and after)
start_idx = None
for i, line in enumerate(lines):
    if 'void UnifiedASTBackend::PopulateSemanticFields(ASTNode& node, const LanguageAdapter* adapter' in line:
        start_idx = i
        break

if start_idx:
    # Add a newline after ParseToASTResult and then the rest of the functions
    good_lines.append('')
    good_lines.extend(lines[start_idx:])
else:
    print("ERROR: Could not find PopulateSemanticFields function")

# Write the fixed content
with open('/mnt/aux-data/teague/Projects/duckdb_ast/src/unified_ast_backend.cpp', 'w') as f:
    f.write('\n'.join(good_lines))

print("Fixed unified_ast_backend.cpp")