# Sitting Ducks 🦆

**Sitting Ducks** is a DuckDB extension that makes Abstract Syntax Trees (ASTs) from source code files quack like data - enabling powerful SQL-based analysis across multiple programming languages.

## Why "Sitting Ducks"?

The name reflects the project's philosophy:
- **Sitting**: ASTs are stationary, structured data waiting to be analyzed
- **Ducks**: Everything quacks like data in DuckDB - including your source code!
- **Target-rich environment**: Code analysis becomes as easy as shooting fish in a barrel

## What Makes It Special

Traditional code analysis tools force you to learn their APIs and query languages. Sitting Ducks lets you use the most powerful data analysis language ever created - **SQL** - to explore your codebase.

```sql
-- Find all complex functions across your entire codebase
SELECT file_path, name, descendant_count as complexity
FROM read_ast('**/*.py') 
WHERE type LIKE '%function%' AND descendant_count > 100
ORDER BY complexity DESC;

-- Analyze import patterns
SELECT 
    regexp_extract(peek, 'from (\w+)', 1) as module,
    count(*) as usage_count
FROM read_ast('**/*.py')
WHERE type = 'import_from_statement'
GROUP BY module
ORDER BY usage_count DESC;
```

## Architecture

Sitting Ducks transforms your source code into queriable data structures:

1. **Tree-sitter parsing** - Robust, error-recovering parsers for 13+ languages
2. **Semantic classification** - Universal type system for cross-language analysis  
3. **SQL interface** - Rich table functions and analysis macros
4. **Monadic design** - Composable operations that maintain tree structure

## Supported Languages

- Python, JavaScript, TypeScript
- C, C++, Rust, Go
- Java, C#, PHP, Ruby
- HTML, CSS, Markdown, SQL

## Use Cases

- **Code quality analysis** - Find complexity hotspots and code smells
- **Dependency analysis** - Understand import/include relationships
- **Refactoring assistance** - Identify patterns and duplicated code
- **Documentation generation** - Extract API signatures and comments
- **Security auditing** - Find dangerous patterns across languages
- **Learning and exploration** - Understand unfamiliar codebases quickly

## Quick Start

```bash
# Install the extension
make && make install

# Analyze a Python project
./tools/ast-cli/ast index py "**/*.py"
./tools/ast-cli/ast funcs "**/*.py" 

# Or use SQL directly
duckdb -c "LOAD sitting_ducks; SELECT * FROM read_ast('main.py') LIMIT 10;"
```

## Philosophy

Code is data. Data wants to be queried. DuckDB makes querying a joy.

Therefore: Analyzing code should be joyful. 🦆

---

*Previous name: DuckDB AST Extension*
*Renamed to Sitting Ducks for better branding and memorability*