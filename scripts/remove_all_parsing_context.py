#!/usr/bin/env python3
"""Remove all GetParsingContext implementations from language adapter cpp files."""

import re
import glob

# Get all cpp files
cpp_files = glob.glob('/mnt/aux-data/teague/Projects/duckdb_ast/src/language_adapters/*.cpp')

for cpp_file in cpp_files:
    with open(cpp_file, 'r') as f:
        content = f.read()
    
    # Find and remove GetParsingContext implementation
    # Look for the pattern from the function start to the closing brace
    pattern = r'\n\n\nLanguageAdapter::ParsingContext \w+::GetParsingContext\(\) const \{[^}]*?\n\}\n'
    
    original_len = len(content)
    content = re.sub(pattern, '\n', content, flags=re.DOTALL)
    
    if len(content) != original_len:
        with open(cpp_file, 'w') as f:
            f.write(content)
        print(f"Removed GetParsingContext from {cpp_file}")

print("Done!")