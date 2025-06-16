#!/usr/bin/env python3
"""Add GetParsingFunction implementation to all language adapters."""

import os
import re

# Map of adapter class names to their file names
adapters = {
    'JavaScriptAdapter': 'javascript_adapter.cpp',
    'CPPAdapter': 'cpp_adapter.cpp',
    'TypeScriptAdapter': 'typescript_adapter.cpp',
    'SQLAdapter': 'sql_adapter.cpp',
    'GoAdapter': 'go_adapter.cpp',
    'RubyAdapter': 'ruby_adapter.cpp',
    'MarkdownAdapter': 'markdown_adapter.cpp',
    'JavaAdapter': 'java_adapter.cpp',
    'HTMLAdapter': 'html_adapter.cpp',
    'CSSAdapter': 'css_adapter.cpp',
    'CAdapter': 'c_adapter.cpp'
}

base_dir = '/mnt/aux-data/teague/Projects/duckdb_ast/src/language_adapters/'

for class_name, file_name in adapters.items():
    file_path = os.path.join(base_dir, file_name)
    
    # Read the file
    with open(file_path, 'r') as f:
        content = f.read()
    
    # Check if GetParsingFunction already exists
    if 'GetParsingFunction' in content:
        print(f"Skipping {file_name} - GetParsingFunction already exists")
        continue
    
    # First, add the include for unified_ast_backend_impl.hpp if not present
    if 'unified_ast_backend_impl.hpp' not in content:
        content = content.replace(
            '#include "language_adapter.hpp"',
            '#include "language_adapter.hpp"\n#include "unified_ast_backend_impl.hpp"'
        )
    
    # Find the end of the GetNodeConfig method and add GetParsingFunction
    pattern = r'(const NodeConfig\* ' + class_name + r'::GetNodeConfig\(const string &node_type\) const \{[^}]+\})'
    match = re.search(pattern, content, re.DOTALL)
    
    if match:
        # Insert GetParsingFunction implementation after GetNodeConfig
        insertion_point = match.end()
        
        parsing_function = f'''

ParsingFunction {class_name}::GetParsingFunction() const {{
    // Return a lambda that captures the templated parsing function
    return [](const void* adapter, const string& content, const string& language, 
              const string& file_path, int32_t peek_size, const string& peek_mode) -> ASTResult {{
        auto typed_adapter = static_cast<const {class_name}*>(adapter);
        return UnifiedASTBackend::ParseToASTResultTemplated(typed_adapter, content, language, file_path, peek_size, peek_mode);
    }};
}}'''
        
        content = content[:insertion_point] + parsing_function + content[insertion_point:]
        
        # Write the updated content
        with open(file_path, 'w') as f:
            f.write(content)
        
        print(f"Updated {file_name}")
    else:
        print(f"WARNING: Could not find GetNodeConfig in {file_name}")

print("Done!")