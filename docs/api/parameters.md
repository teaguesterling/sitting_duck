# Parameters Reference

Complete reference for all `read_ast()` parameters.

## Required Parameters

### File Patterns

**Type:** `VARCHAR` or `LIST(VARCHAR)`

Specify which files to parse.

```sql
-- Single file
SELECT * FROM read_ast('main.py');

-- Glob pattern
SELECT * FROM read_ast('src/**/*.py');

-- File array
SELECT * FROM read_ast(['main.py', 'utils.py']);

-- Mixed array
SELECT * FROM read_ast(['src/**/*.py', 'lib/**/*.js', 'main.cpp']);
```

#### Glob Syntax

| Pattern | Matches |
|---------|---------|
| `*` | Any characters except `/` |
| `**` | Any path (recursive) |
| `?` | Single character |
| `[abc]` | Character set |
| `{a,b}` | Alternatives |

---

## Language Parameter

**Type:** `VARCHAR`
**Default:** `'auto'`

Override automatic language detection.

```sql
-- Auto-detect (default)
SELECT * FROM read_ast('script.py');

-- Explicit language
SELECT * FROM read_ast('script.txt', 'python');

-- For arrays (applies to all files)
SELECT * FROM read_ast(['**/*.txt'], 'javascript');
```

### Supported Languages

| Language | Identifier | Extensions |
|----------|------------|------------|
| Python | `'python'` | `.py` |
| JavaScript | `'javascript'` | `.js`, `.jsx` |
| TypeScript | `'typescript'` | `.ts`, `.tsx` |
| Java | `'java'` | `.java` |
| C | `'c'` | `.c`, `.h` |
| C++ | `'cpp'` | `.cpp`, `.hpp`, `.cc` |
| C# | `'csharp'` | `.cs` |
| Go | `'go'` | `.go` |
| Rust | `'rust'` | `.rs` |
| Ruby | `'ruby'` | `.rb` |
| PHP | `'php'` | `.php` |
| Swift | `'swift'` | `.swift` |
| Kotlin | `'kotlin'` | `.kt`, `.kts` |
| Lua | `'lua'` | `.lua` |
| R | `'r'` | `.r`, `.R` |
| Dart | `'dart'` | `.dart` |
| Zig | `'zig'` | `.zig` |
| SQL | `'sql'` | `.sql` |
| Markdown | `'markdown'` | `.md`, `.markdown` |
| HTML | `'html'` | `.html`, `.htm` |
| CSS | `'css'` | `.css` |
| JSON | `'json'` | `.json` |
| Bash | `'bash'` | `.sh`, `.bash` |
| HCL | `'hcl'` | `.hcl`, `.tf`, `.tfvars` |
| GraphQL | `'graphql'` | `.graphql`, `.gql` |
| TOML | `'toml'` | `.toml` |

---

## Optional Parameters

### `ignore_errors`

**Type:** `BOOLEAN`
**Default:** `false`

Continue processing when files fail to parse.

```sql
-- Stop on first error (default)
SELECT * FROM read_ast('**/*.py');

-- Continue despite errors
SELECT * FROM read_ast('**/*.py', ignore_errors := true);
```

---

### `context`

**Type:** `VARCHAR`
**Default:** `'native'`

Control semantic analysis depth.

| Value | Description | Performance |
|-------|-------------|-------------|
| `'none'` | Raw AST only | Fastest |
| `'node_types_only'` | + Semantic types | Fast |
| `'normalized'` | + Names | Medium |
| `'native'` | Full extraction | Detailed |

```sql
-- Fastest (raw AST)
SELECT * FROM read_ast('file.py', context := 'none');

-- Full analysis (default)
SELECT * FROM read_ast('file.py', context := 'native');
```

---

### `source`

**Type:** `VARCHAR`
**Default:** `'lines'`

Control source text extraction.

| Value | Description |
|-------|-------------|
| `'none'` | No source text |
| `'path'` | File path only |
| `'lines_only'` | Line numbers only |
| `'lines'` | Line-based info |
| `'full'` | Complete source |

```sql
-- No source extraction
SELECT * FROM read_ast('file.py', source := 'none');

-- Full source text
SELECT * FROM read_ast('file.py', source := 'full');
```

---

### `structure`

**Type:** `VARCHAR`
**Default:** `'full'`

Control tree structure extraction.

| Value | Description |
|-------|-------------|
| `'none'` | No structure info |
| `'minimal'` | Basic structure |
| `'full'` | Complete structure |

```sql
-- Minimal structure
SELECT * FROM read_ast('file.py', structure := 'minimal');
```

---

### `peek`

**Type:** `ANY`
**Default:** `'smart'`

Control source code snippet extraction.

| Value | Description |
|-------|-------------|
| `'none'` | No peek |
| `'smart'` | Intelligent truncation |
| `'full'` | Complete source |
| Integer | Character limit |

```sql
-- No peek
SELECT * FROM read_ast('file.py', peek := 'none');

-- Custom size
SELECT * FROM read_ast('file.py', peek := 200);

-- Smart truncation (default)
SELECT * FROM read_ast('file.py', peek := 'smart');
```

---

### `peek_size`

**Type:** `INTEGER`
**Default:** `120`

Custom peek size in characters.

```sql
SELECT * FROM read_ast('file.py', peek_size := 250);
```

---

### `peek_mode`

**Type:** `VARCHAR`
**Default:** `'smart'`

Peek extraction mode.

```sql
SELECT * FROM read_ast('file.py', peek_mode := 'smart');
```

---

### `batch_size`

**Type:** `INTEGER`

Batch size for streaming large file sets.

```sql
SELECT * FROM read_ast(['**/*.py', '**/*.js'], batch_size := 10);
```

---

## Parameter Combinations

### Maximum Performance

```sql
SELECT file_path, type, COUNT(*)
FROM read_ast(
    '**/*.py',
    context := 'none',
    source := 'none',
    structure := 'none',
    peek := 'none',
    ignore_errors := true
)
GROUP BY file_path, type;
```

### Full Analysis

```sql
SELECT *
FROM read_ast(
    'src/**/*.py',
    context := 'native',
    source := 'full',
    structure := 'full',
    peek := 'full'
);
```

### Balanced

```sql
SELECT file_path, type, name, start_line
FROM read_ast(
    'src/**/*.py',
    context := 'normalized',
    source := 'lines',
    peek := 120,
    ignore_errors := true
);
```

## Next Steps

- [Output Schema](output-schema.md) - Column reference
- [Core Functions](core-functions.md) - Function reference
- [Semantic Types](semantic-types.md) - Type system
