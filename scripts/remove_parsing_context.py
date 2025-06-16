#!/usr/bin/env python3
"""Remove GetParsingContext() method declarations from all language adapter classes."""

import re

# Read the header file
with open('/mnt/aux-data/teague/Projects/duckdb_ast/src/include/language_adapter.hpp', 'r') as f:
    content = f.read()

# Remove all GetParsingContext declarations
pattern = r'^\s*virtual ParsingContext GetParsingContext\(\) const override;\s*\n'
content = re.sub(pattern, '', content, flags=re.MULTILINE)

# Write the updated content back
with open('/mnt/aux-data/teague/Projects/duckdb_ast/src/include/language_adapter.hpp', 'w') as f:
    f.write(content)

print("Removed GetParsingContext declarations from header file")

# Also remove implementations from all .cpp files
import os
import glob

cpp_files = glob.glob('/mnt/aux-data/teague/Projects/duckdb_ast/src/language_adapters/*.cpp')

for cpp_file in cpp_files:
    with open(cpp_file, 'r') as f:
        content = f.read()
    
    # Find and remove GetParsingContext implementation
    pattern = r'LanguageAdapter::ParsingContext \w+::GetParsingContext\(\) const \{[^}]*?\n\}\n+'
    if re.search(pattern, content, re.DOTALL):
        content = re.sub(pattern, '', content, re.DOTALL)
        with open(cpp_file, 'w') as f:
            f.write(content)
        print(f"Removed GetParsingContext implementation from {os.path.basename(cpp_file)}")

print("Done!")