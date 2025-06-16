#!/usr/bin/env python3
"""Clean up the unified_ast_backend.cpp file to remove duplicate implementation."""

# Read the file
with open('/mnt/aux-data/teague/Projects/duckdb_ast/src/unified_ast_backend.cpp', 'r') as f:
    lines = f.readlines()

# Find where to cut - after the closing brace of ParseToASTResult
output_lines = []
in_old_implementation = False
brace_count = 0
found_end = False

for i, line in enumerate(lines):
    if not found_end:
        output_lines.append(line)
        
        # Check if we're at the end of ParseToASTResult
        if i > 0 and lines[i-1].strip() == '}' and i >= 113:
            # Check if the next non-empty line starts a new function
            j = i
            while j < len(lines) and lines[j].strip() == '':
                j += 1
            if j < len(lines) and ('void UnifiedASTBackend::' in lines[j] or 
                                  'vector<' in lines[j] or 
                                  'LogicalType UnifiedASTBackend::' in lines[j] or
                                  'Value UnifiedASTBackend::' in lines[j] or
                                  'ASTResultCollection UnifiedASTBackend::' in lines[j] or
                                  'unique_ptr<ASTResult> UnifiedASTBackend::' in lines[j]):
                found_end = True
                # Remove the duplicate old implementation lines
                while output_lines and output_lines[-1].strip() in ['', '}']:
                    if output_lines[-1].strip() == '}' and len(output_lines) > 113:
                        break
                    output_lines.pop()
                output_lines.append('}\n')
                output_lines.append('\n')
    else:
        # After we found the end, check if this line starts a valid function
        if ('void UnifiedASTBackend::' in line or 
            'vector<' in line or 
            'LogicalType UnifiedASTBackend::' in line or
            'Value UnifiedASTBackend::' in line or
            'ASTResultCollection UnifiedASTBackend::' in line or
            'unique_ptr<ASTResult> UnifiedASTBackend::' in line):
            output_lines.append(line)
            in_old_implementation = False
        elif not in_old_implementation:
            output_lines.append(line)

# Write the cleaned file
with open('/mnt/aux-data/teague/Projects/duckdb_ast/src/unified_ast_backend.cpp', 'w') as f:
    f.writelines(output_lines)

print("Cleaned up unified_ast_backend.cpp")