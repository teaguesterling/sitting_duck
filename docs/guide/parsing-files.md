# Parsing Files

Learn how to parse source code files with Sitting Duck.

## Single File Parsing

### Basic Parsing

```sql
-- Parse a single file (language auto-detected)
SELECT * FROM read_ast('src/main.py');

-- Explicit language specification
SELECT * FROM read_ast('script.txt', 'python');
```

### Language Detection

Sitting Duck automatically detects languages from file extensions:

| Extension | Language |
|-----------|----------|
| `.py` | Python |
| `.js`, `.jsx` | JavaScript |
| `.ts`, `.tsx` | TypeScript |
| `.java` | Java |
| `.go` | Go |
| `.rs` | Rust |
| `.c`, `.h` | C |
| `.cpp`, `.hpp`, `.cc` | C++ |
| `.cs` | C# |
| `.rb` | Ruby |
| `.php` | PHP |
| `.swift` | Swift |
| `.kt`, `.kts` | Kotlin |
| `.lua` | Lua |
| `.r`, `.R` | R |
| `.dart` | Dart |
| `.zig` | Zig |
| `.sql` | SQL |
| `.md`, `.markdown` | Markdown |
| `.html`, `.htm` | HTML |
| `.css` | CSS |
| `.json` | JSON |
| `.sh`, `.bash` | Bash |
| `.tf`, `.hcl` | HCL |
| `.graphql`, `.gql` | GraphQL |
| `.toml` | TOML |

### Override Language Detection

```sql
-- Force Python parsing for a file with non-standard extension
SELECT * FROM read_ast('Makefile', 'bash');

-- Parse a TypeScript file as JavaScript
SELECT * FROM read_ast('script.ts', 'javascript');
```

## Glob Patterns

### Basic Patterns

```sql
-- All Python files in current directory
SELECT * FROM read_ast('*.py');

-- All files in src directory
SELECT * FROM read_ast('src/*');

-- Recursive - all Python files in all subdirectories
SELECT * FROM read_ast('**/*.py');
```

### Examples

```sql
-- All .py and .js files
SELECT * FROM read_ast(['**/*.py', '**/*.js'], ignore_errors := true);

-- Files starting with 'test_'
SELECT * FROM read_ast('**/test_*.py');
```

## Output Columns

The `read_ast()` function returns 20 columns by default (22 with `source := 'full'`):

| Column | Type | Description |
|--------|------|-------------|
| `node_id` | BIGINT | Unique node identifier |
| `type` | VARCHAR | Tree-sitter AST node type |
| `semantic_type` | SEMANTIC_TYPE | Universal semantic category |
| `flags` | UTINYINT | Node property flags |
| `name` | VARCHAR | Extracted identifier name |
| `signature_type` | VARCHAR | Type/return type information |
| `parameters` | STRUCT[] | Function parameters (name and type) |
| `modifiers` | VARCHAR[] | Access modifiers and keywords |
| `annotations` | VARCHAR | Decorator/annotation text |
| `qualified_name` | VARCHAR | Fully qualified name |
| `file_path` | VARCHAR | Source file path |
| `language` | VARCHAR | Detected language |
| `start_line` | UINTEGER | Starting line (1-based) |
| `end_line` | UINTEGER | Ending line (1-based) |
| `start_column` | UINTEGER | Starting column (**only with `source := 'full'`**) |
| `end_column` | UINTEGER | Ending column (**only with `source := 'full'`**) |
| `parent_id` | BIGINT | Parent node ID |
| `depth` | UINTEGER | Tree depth (0 for root) |
| `sibling_index` | UINTEGER | Position among siblings |
| `children_count` | UINTEGER | Direct children count |
| `descendant_count` | UINTEGER | Total descendants |
| `peek` | VARCHAR | Source code snippet |

See [Output Schema](../api/output-schema.md) for detailed column documentation.

## Parameters

### Core Parameters

```sql
-- File pattern (required)
SELECT * FROM read_ast('src/**/*.py');

-- Language override (optional, auto-detected by default)
SELECT * FROM read_ast('script.txt', 'python');

-- Ignore errors (continue if files fail to parse)
SELECT * FROM read_ast('**/*.py', ignore_errors := true);
```

### Context Extraction

```sql
-- Native context (full semantic analysis, default)
SELECT * FROM read_ast('file.py', context := 'native');

-- Normalized (cross-language semantic types)
SELECT * FROM read_ast('file.py', context := 'normalized');

-- Node types only (faster, less detail)
SELECT * FROM read_ast('file.py', context := 'node_types_only');

-- None (fastest, raw AST only)
SELECT * FROM read_ast('file.py', context := 'none');
```

### Peek Configuration

```sql
-- Custom peek size (characters)
SELECT * FROM read_ast('file.py', peek := 200);

-- Peek modes
SELECT * FROM read_ast('file.py', peek := 'smart');   -- Intelligent truncation
SELECT * FROM read_ast('file.py', peek := 'full');    -- Full source text
SELECT * FROM read_ast('file.py', peek := 'none');    -- No source text
```

### Source and Structure

```sql
-- Control source extraction
SELECT * FROM read_ast('file.py', source := 'full');     -- Full source
SELECT * FROM read_ast('file.py', source := 'lines');    -- Line-based
SELECT * FROM read_ast('file.py', source := 'none');     -- No source

-- Control structure extraction
SELECT * FROM read_ast('file.py', structure := 'full');    -- Full tree info
SELECT * FROM read_ast('file.py', structure := 'minimal'); -- Basic only
SELECT * FROM read_ast('file.py', structure := 'none');    -- No structure
```

## Error Handling

### Graceful Error Handling

```sql
-- Continue processing even if some files fail
SELECT file_path, COUNT(*) as nodes
FROM read_ast('**/*.*', ignore_errors := true)
GROUP BY file_path;
```

### Finding Parse Errors

```sql
-- Identify files with syntax errors
SELECT DISTINCT file_path, type, peek
FROM read_ast('**/*.py', ignore_errors := true)
WHERE type = 'ERROR';
```

## Performance Tips

1. **Use specific patterns**: `src/**/*.py` is faster than `**/*.*`
2. **Limit scope**: Parse only what you need
3. **Use `ignore_errors`**: Prevents failures from blocking processing
4. **Minimize context**: Use `context := 'none'` for speed when semantic info isn't needed

```sql
-- Fast: specific pattern, minimal context
SELECT file_path, COUNT(*)
FROM read_ast('src/**/*.py', context := 'none')
GROUP BY file_path;

-- Slower: broad pattern, full context
SELECT file_path, COUNT(*)
FROM read_ast('**/*.*', context := 'native', ignore_errors := true)
GROUP BY file_path;
```

## Next Steps

- [Multi-File Processing](multi-file.md) - Arrays and batch processing
- [Semantic Types](semantic-types.md) - Cross-language analysis
- [Context Extraction](context-extraction.md) - Detailed semantic information
