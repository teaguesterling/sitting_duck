# Sitting Duck

**Transform your source code into a sitting duck for SQL-based analysis.**

Sitting Duck is a DuckDB extension that makes Abstract Syntax Trees (ASTs) from source code files quack like data - enabling powerful SQL-based analysis across **27 programming languages**.

## Why Sitting Duck?

Traditional code analysis tools force you to learn their APIs and query languages. Sitting Duck lets you use the most powerful data analysis language ever created - **SQL** - to explore your codebase.

```sql
-- Find all functions with their complexity scores
SELECT name, file_path, descendant_count as complexity
FROM read_ast('src/**/*.py')
WHERE type = 'function_definition'
ORDER BY complexity DESC
LIMIT 10;
```

## Key Features

- **27 Languages Supported** - Web, systems, scripting, enterprise, and infrastructure languages
- **Semantic Type System** - Cross-language analysis with universal semantic categories
- **Multi-File Processing** - Glob patterns, file arrays, and batch processing
- **Native Context Extraction** - Language-specific semantic analysis with type information
- **Streaming Design** - Efficient processing of large codebases
- **DuckDB Integration** - Follows DuckDB conventions for a native feel

## Supported Languages

| Category | Languages |
|----------|-----------|
| **Web** | JavaScript, TypeScript, HTML, CSS |
| **Systems** | C, C++, Go, Rust, Zig |
| **Scripting** | Python, Ruby, PHP, Lua, R, Bash |
| **Enterprise** | Java, C#, Kotlin, Swift |
| **Mobile** | Dart |
| **Infrastructure** | HCL (Terraform), JSON, TOML, GraphQL |
| **Documentation** | SQL, Markdown |

## Quick Start

```sql
-- Load the extension
LOAD sitting_duck;

-- Parse a Python file
SELECT type, name, start_line
FROM read_ast('example.py')
WHERE type = 'function_definition';

-- Analyze an entire codebase
SELECT file_path, COUNT(*) as nodes, language
FROM read_ast('src/**/*.*', ignore_errors := true)
GROUP BY file_path, language
ORDER BY nodes DESC;

-- Cross-language function analysis
SELECT language, COUNT(*) as function_count
FROM read_ast(['**/*.py', '**/*.js', '**/*.java'], ignore_errors := true)
WHERE semantic_type = 240  -- DEFINITION_FUNCTION
GROUP BY language;
```

## Documentation

- **[Getting Started](getting-started/installation.md)** - Installation and basic setup
- **[User Guide](guide/parsing-files.md)** - Detailed usage instructions
- **[API Reference](api/core-functions.md)** - Complete function reference
- **[Languages](languages/overview.md)** - Language-specific details
- **[AI Agent Guide](ai-agent-guide.md)** - Using Sitting Duck with AI agents

## Why "Sitting Duck"?

The name reflects the project's philosophy and technology stack:

- **Sitting**: A nod to Tree-sitter, our parsing engine - your code sits in trees waiting for analysis
- **Duck**: Everything quacks like data in DuckDB - including your source code!
- **Target**: Your codebase becomes a sitting duck for powerful SQL-based analysis

---

*Code is data. Data wants to be queried. DuckDB makes querying a joy.*
