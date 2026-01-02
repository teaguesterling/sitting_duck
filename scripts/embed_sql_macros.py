#!/usr/bin/env python3
"""
Generate a C++ header file with embedded SQL macros.
This script reads SQL files and creates a header with raw string literals.
"""

import os
import sys
from pathlib import Path

def escape_raw_string_delimiter(content):
    """Ensure the content doesn't contain )SQLMACRO" which would break our delimiter."""
    if ')SQLMACRO"' in content:
        raise ValueError("SQL content contains raw string delimiter - please use a different delimiter")
    return content

def generate_chain_methods(sql_files, sql_dir):
    """Generate short name aliases by removing 'ast_' prefix from macro names."""
    chain_methods = ["-- Auto-generated chain methods (short names)", ""]
    
    for sql_file in sql_files:
        filepath = os.path.join(sql_dir, sql_file)
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                content = f.read()
            
            # Find all CREATE OR REPLACE MACRO ast_* statements
            import re
            macro_pattern = r'CREATE OR REPLACE MACRO (ast_\w+)'
            
            for match in re.finditer(macro_pattern, content):
                macro_name = match.group(1)
                short_name = macro_name.replace('ast_', '')
                
                # Extract the parameter list
                start = match.end()
                paren_count = 0
                param_start = content.find('(', start)
                i = param_start
                while i < len(content) and (paren_count > 0 or i == param_start):
                    if content[i] == '(':
                        paren_count += 1
                    elif content[i] == ')':
                        paren_count -= 1
                    i += 1
                param_list = content[param_start:i]
                
                # Create alias
                chain_methods.append(f"CREATE OR REPLACE MACRO {short_name}{param_list} AS {macro_name}{param_list};")
    
    return '\n'.join(chain_methods)

def generate_header(sql_dir, output_file):
    """Generate C++ header with embedded SQL macros."""

    sql_files = [
        'semantic_predicates.sql',
        'file_utilities.sql',
        'tree_navigation.sql',
        'pattern_matching.sql',
    ]
    
    # Chain methods (short names) removed for simplicity
    # chain_methods_content = generate_chain_methods(sql_files, sql_dir)
    
    header_content = """// Auto-generated file - DO NOT EDIT
// Generated from SQL macro files in src/sql_macros/
// Run: python scripts/embed_sql_macros.py

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace duckdb {

// SQL macro definitions embedded at compile time
static const std::vector<std::pair<std::string, std::string>> EMBEDDED_SQL_MACROS = {
"""
    
    for sql_file in sql_files:
        filepath = os.path.join(sql_dir, sql_file)
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                content = f.read()
                
            # Verify content doesn't break our delimiter
            escape_raw_string_delimiter(content)
            
            header_content += f'''    {{"{sql_file}", R"SQLMACRO(
{content}
)SQLMACRO"}},
'''
    
    # Chain methods removed for simplicity
    # escape_raw_string_delimiter(chain_methods_content)
    # header_content += f'''    {{"02b_chain_methods.sql", R"SQLMACRO(
    # {chain_methods_content}
    # )SQLMACRO"}},
    # '''
    
    header_content += """};

} // namespace duckdb
"""
    
    with open(output_file, 'w') as f:
        f.write(header_content)
    
    print(f"Generated {output_file}")

if __name__ == "__main__":
    # Get paths relative to script location
    script_dir = Path(__file__).parent
    project_root = script_dir.parent
    sql_dir = project_root / "src" / "sql_macros"
    output_file = project_root / "src" / "include" / "embedded_sql_macros.hpp"
    
    generate_header(sql_dir, output_file)