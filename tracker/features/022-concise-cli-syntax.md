# Concise CLI Syntax for AST Queries

## Vision
Create a streamlined CLI that makes AST analysis as easy as `grep` or `find`, but with semantic understanding.

## Proposed Syntax
```bash
# Basic pattern
duckdb_ast [options] <file_pattern> <ast_expression>

# Examples
duckdb_ast src/*.cpp "ast.get_functions().to_locations(with_peek:=true)"
duckdb_ast src/**/*.py "ast.find_name('authenticate').to_signatures()"
duckdb_ast . "ast.get_classes().filter(n -> n.is_public)"
```

## Translation Layer
The CLI would translate concise expressions to full SQL:

### Input
```bash
duckdb_ast src/*.cpp "ast.get_functions().to_locations(with_peek:=true)"
```

### Generated SQL
```sql
SELECT ast.get_functions().to_locations(with_peek:=true) 
FROM read_ast_objects('src/*.cpp')
```

### More Complex Example
```bash
duckdb_ast --format=markdown src/**/*.py "ast.find_type('function_definition').filter(n -> n.descendant_count > 50).to_complexity_metrics()"
```

Becomes:
```sql
SELECT ast.find_type('function_definition')
         .filter(n -> n.descendant_count > 50)
         .to_complexity_metrics()
FROM read_ast_objects('src/**/*.py')
FORMAT markdown;
```

## CLI Options
```bash
duckdb_ast [options] <pattern> <expression>

Options:
  -f, --format=FORMAT     Output format: table, json, markdown, csv
  -l, --language=LANG     Force language detection (python, cpp, javascript)
  -o, --output=FILE       Write to file instead of stdout
  -v, --verbose           Show generated SQL
  --include-types=TYPES   Filter to specific node types
  --exclude=PATTERN       Exclude files matching pattern
  --recursive             Search recursively (default for **)
  --parallel=N            Number of parallel workers
```

## Developer Workflow Integration
```bash
# Quick function inventory
duckdb_ast . "ast.get_functions().to_names()" --format=json

# Find technical debt
duckdb_ast src/ "ast.find_comments().filter(c -> c.text LIKE '%TODO%')" 

# Complexity analysis
duckdb_ast --format=markdown "ast.to_complexity_metrics().order_by('complexity_score DESC')"

# Cross-language function search
duckdb_ast src/ "ast.find_name('authenticate')" --format=table

# Architecture validation
duckdb_ast src/ "ast.get_classes().filter(c -> !c.has_method('virtual'))" 
```

## Implementation Strategy

### Phase 1: Basic Translation
- Parse `file_pattern` → `read_ast_objects('pattern')`
- Parse `ast_expression` → validate it's a valid AST method chain
- Generate SQL and execute

### Phase 2: Expression Parser
- Handle method chaining: `ast.get_X().filter().to_Y()`
- Handle lambda expressions: `filter(n -> condition)`
- Handle parameters: `to_locations(with_peek:=true)`

### Phase 3: Smart Defaults
- Auto-detect output format based on expression
- Intelligent file pattern expansion
- Caching for repeated queries

### Phase 4: Interactive Mode
```bash
duckdb_ast --interactive
> ast.get_functions()     # Shows available functions
> .filter(n -> n.name LIKE '%auth%')  # Chain from previous
> .to_signatures()        # Get final output
```

## Benefits for Dogfooding
1. **Rapid iteration** - No SQL boilerplate for quick analyses
2. **Discoverable** - Method chaining shows what's possible
3. **Memorable** - Consistent with modern CLI tools
4. **Pipeable** - Integrates with shell workflows

## Example Usage During Development
```bash
# Before making changes
duckdb_ast src/ "ast.get_functions().count()" > before.txt

# After refactor
duckdb_ast src/ "ast.get_functions().count()" > after.txt
diff before.txt after.txt

# Find what changed
duckdb_ast src/ "ast.get_functions().filter(f -> f.modified_recently)"
```

## Implementation Notes
- Could be a Python script that generates SQL for DuckDB
- Or a lightweight C++ tool that uses the extension directly
- Should validate expressions before execution
- Need good error messages for invalid method chains

## Success Metrics
- Developers reach for `duckdb_ast` instead of `grep` for code analysis
- Common queries become one-liners
- New team members can explore codebase structure quickly
- Integration into CI/CD pipelines for architectural checks