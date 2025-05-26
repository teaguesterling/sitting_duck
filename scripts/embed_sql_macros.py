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

def generate_header(sql_dir, output_file):
    """Generate C++ header with embedded SQL macros."""
    
    sql_files = [
        'core_macros.sql',
        'entrypoint_macros.sql',
        'structure_macros.sql', 
        'extract_macros.sql',
        'source_macros.sql',
        'ai_macros.sql',
        'utility_macros.sql'
    ]
    
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