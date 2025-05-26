#!/usr/bin/env python3
"""
Update test files to use new function names
"""

import os
import re
from pathlib import Path

# Mapping of old function names to new ones
FUNCTION_MAPPINGS = {
    # Core functions that changed names
    'ast_find_type': 'ast_get_type',
    
    # Functions that became parameterized calls to ast_get_names
    'ast_function_names': "ast_get_names(nodes, node_type:='function_definition')",
    'ast_class_names': "ast_get_names(nodes, node_type:='class_definition')", 
    'ast_identifiers': "ast_get_names(nodes, node_type:='identifier')",
    
    # Functions that became calls to other new functions
    'ast_at_depth': 'ast_get_depth',
    'ast_children_of': 'ast_nav_children',
}

def update_file(file_path):
    """Update a single test file with new function names"""
    try:
        with open(file_path, 'r') as f:
            content = f.read()
        
        original_content = content
        
        # Apply replacements
        for old_name, new_name in FUNCTION_MAPPINGS.items():
            # Handle cases with parameters - need to be more careful
            if old_name in ['ast_function_names', 'ast_class_names', 'ast_identifiers']:
                if '(' in new_name:
                    # Handle both parameterized and parameterless calls
                    # Replace function_name(nodes) with new parameterized version
                    pattern_with_param = rf'\b{re.escape(old_name)}\s*\(\s*([^)]+)\s*\)'
                    content = re.sub(pattern_with_param, lambda m: new_name.replace('nodes', m.group(1)), content)
                    
                    # Replace parameterless calls function_name() with new version
                    pattern_no_param = rf'\b{re.escape(old_name)}\s*\(\s*\)'
                    content = re.sub(pattern_no_param, new_name, content)
                else:
                    content = content.replace(old_name, new_name)
            else:
                # Simple replacement
                content = content.replace(old_name, new_name)
        
        # Write back if changed
        if content != original_content:
            with open(file_path, 'w') as f:
                f.write(content)
            print(f"Updated: {file_path}")
            return True
        else:
            return False
            
    except Exception as e:
        print(f"Error updating {file_path}: {e}")
        return False

def main():
    """Update all test files in test/sql/"""
    test_dir = Path(__file__).parent.parent / "test" / "sql"
    
    if not test_dir.exists():
        print(f"Test directory not found: {test_dir}")
        return
    
    updated_count = 0
    total_files = 0
    
    # Find all .test files
    for test_file in test_dir.rglob("*.test"):
        total_files += 1
        if update_file(test_file):
            updated_count += 1
    
    print(f"\nUpdated {updated_count} out of {total_files} test files")

if __name__ == "__main__":
    main()