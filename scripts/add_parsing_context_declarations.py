#!/usr/bin/env python3
"""Add GetParsingContext() method declarations to all language adapter classes in the header file."""

import re

# Read the header file
with open('/mnt/aux-data/teague/Projects/duckdb_ast/src/include/language_adapter.hpp', 'r') as f:
    content = f.read()

# List of all language adapters that need the method declaration
adapters = [
    'PythonAdapter', 'JavaScriptAdapter', 'CPPAdapter', 'TypeScriptAdapter', 
    'SQLAdapter', 'GoAdapter', 'RubyAdapter', 'MarkdownAdapter', 'JavaAdapter',
    'HTMLAdapter', 'CSSAdapter', 'CAdapter'
]

# For each adapter, find its class definition and add the method declaration
for adapter in adapters:
    # Find the class definition
    class_pattern = rf'class {adapter} : public LanguageAdapter \{{[^{{}}]*?public:(.*?)(?=protected:|private:|\}};)'
    match = re.search(class_pattern, content, re.DOTALL)
    
    if match:
        public_section = match.group(1)
        
        # Check if GetParsingContext is already declared
        if 'GetParsingContext' not in public_section:
            # Find the last override method in the public section
            last_override_pattern = r'(const override;\s*)\n'
            last_override_match = None
            for m in re.finditer(last_override_pattern, public_section):
                last_override_match = m
            
            if last_override_match:
                # Insert the new method declaration after the last override
                insertion_point = match.start(1) + last_override_match.end()
                new_declaration = "    virtual ParsingContext GetParsingContext() const override;\n"
                
                # Find the actual position in the full content
                before = content[:insertion_point]
                after = content[insertion_point:]
                content = before + new_declaration + after
                print(f"Added GetParsingContext() declaration to {adapter}")

# Write the updated content back
with open('/mnt/aux-data/teague/Projects/duckdb_ast/src/include/language_adapter.hpp', 'w') as f:
    f.write(content)

print("Done!")