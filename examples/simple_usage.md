# Simple AST Usage Examples

## Bash Usage

```bash
# Create an index (one-time setup)
./ast index cpp "src/**/*.cpp"
./ast index py "**/*.py"

# List functions in a file
./ast funcs src/unified_ast_backend.cpp

# Find a function
./ast find ParseToASTResult

# Get function source code
./ast src ParseToASTResult

# Search for names
./ast search parse

# Find complex functions
./ast complex 200

# Show statistics
./ast stats
```

## Python Usage

```python
from ast import AST

ast = AST()

# Create index
ast.index('cpp', 'src/**/*.cpp')

# List functions in a file
funcs = ast.funcs('src/main.cpp')
for f in funcs:
    print(f"{f['name']} at lines {f['start']}-{f['end']}")

# Find a function
locations = ast.find('ParseToASTResult')
for loc in locations:
    print(f"Found in {loc['file']} at lines {loc['start']}-{loc['end']}")

# Get source code
code = ast.src('ParseToASTResult')
print(code)

# Search for names
results = ast.search('parse')
for r in results:
    print(f"{r['name']} in {r['file']}:{r['line']}")

# Find complex functions
complex_funcs = ast.complex(threshold=150)
for f in complex_funcs:
    print(f"{f['name']} has complexity {f['complexity']}")

# Get statistics
stats = ast.stats()
for s in stats:
    print(f"{s['lang']}: {s['files']} files, {s['functions']} functions")
```

## Interactive Python Usage

```python
>>> from ast import AST
>>> ast = AST()

# Quick function lookup
>>> ast.find('main')
[{'file': 'src/main.cpp', 'start': 10, 'end': 50, 'complexity': 245}]

# Get source directly
>>> print(ast.src('main'))
int main(int argc, char** argv) {
    // ... function body ...
}

# Search for patterns
>>> ast.search('parse')[:5]  # First 5 results
[
    {'file': 'src/parser.cpp', 'name': 'parse_expression', 'type': 'function_declarator', 'line': 45},
    {'file': 'src/parser.cpp', 'name': 'parse_statement', 'type': 'function_declarator', 'line': 123},
    ...
]
```

## One-Liners

```bash
# Count total functions
./ast stats | awk '{sum += $7} END {print "Total functions:", sum}'

# Find largest function
./ast complex 1 | head -1

# Extract all function names
./ast funcs src/main.cpp | awk '{print $1}'

# Find all files with complex functions
./ast complex 200 | awk '{print $2}' | sort -u
```

## Integration Examples

### Vim Integration
```vim
" Add to .vimrc
command! -nargs=1 ASTFind :!./ast find <args>
command! -nargs=0 ASTFuncs :!./ast funcs %

" Usage:
" :ASTFind ParseToASTResult
" :ASTFuncs
```

### Shell Function
```bash
# Add to .bashrc/.zshrc
astgrep() {
    ./ast search "$1" | fzf --preview 'echo {} | cut -d: -f1 | xargs ./ast funcs'
}

# Usage: astgrep parse
```

### Git Pre-commit Hook
```bash
#!/bin/bash
# Check for overly complex functions
complex=$(./ast complex 300 | wc -l)
if [ $complex -gt 0 ]; then
    echo "Warning: $complex functions exceed complexity threshold of 300"
    ./ast complex 300
fi
```