# Context Extraction

Control the depth of semantic analysis with context extraction levels.

## Context Levels

The `context` parameter controls how much semantic information is extracted:

| Level | Speed | Detail | Use Case |
|-------|-------|--------|----------|
| `'none'` | Fastest | Raw AST only | Simple node counting |
| `'node_types_only'` | Fast | Semantic types | Basic classification |
| `'normalized'` | Medium | + Names | Cross-language analysis |
| `'native'` | Slower | Full extraction | Detailed analysis |

## Using Context Levels

### None - Raw AST Only

Fastest processing, minimal information:

```sql
SELECT type, COUNT(*)
FROM read_ast('src/**/*.py', context := 'none')
GROUP BY type
ORDER BY COUNT(*) DESC;
```

Output includes:
- `type` - Raw AST node type
- `file_path`, `start_line`, `end_line`, etc.
- No `semantic_type`, `name`, or `native`

### Node Types Only

Adds semantic type classification:

```sql
SELECT
    type,
    semantic_type,
    COUNT(*)
FROM read_ast('src/**/*.py', context := 'node_types_only')
WHERE semantic_type IS NOT NULL
GROUP BY type, semantic_type
ORDER BY COUNT(*) DESC;
```

Output includes:
- Everything from `'none'`
- `semantic_type` - Universal semantic category

### Normalized

Adds name extraction:

```sql
SELECT type, name, semantic_type
FROM read_ast('src/**/*.py', context := 'normalized')
WHERE type = 'function_definition'
  AND name IS NOT NULL;
```

Output includes:
- Everything from `'node_types_only'`
- `name` - Extracted identifier names

### Native (Default)

Full semantic analysis with language-specific context:

```sql
SELECT type, name, semantic_type, native
FROM read_ast('src/**/*.py', context := 'native')
WHERE type = 'function_definition';
```

Output includes:
- Everything from `'normalized'`
- `native` - Rich structured data (signatures, parameters, etc.)

## Native Context Details

### Function Extraction

For function definitions, native context extracts:

```sql
-- Python function with parameters and return type
SELECT name, native, semantic_type
FROM read_ast('example.py', context := 'native')
WHERE type = 'function_definition';
```

Native format: `function_name(param1: type1, param2: type2) -> return_type`

### Class Extraction

For class definitions:

```sql
SELECT name, native, semantic_type
FROM read_ast('Example.java', context := 'native')
WHERE type = 'class_declaration';
```

### Method Signatures

```sql
-- Java method signatures
SELECT name, native
FROM read_ast('MyClass.java', context := 'native')
WHERE type = 'method_declaration';
```

## Choosing the Right Level

### Use `'none'` When:

- Counting nodes quickly
- File-level statistics only
- Maximum performance needed

```sql
-- Fast file size comparison
SELECT
    file_path,
    COUNT(*) as node_count
FROM read_ast('**/*.py', context := 'none', ignore_errors := true)
GROUP BY file_path
ORDER BY node_count DESC;
```

### Use `'node_types_only'` When:

- Cross-language type analysis
- Semantic categorization without names
- Moderate performance needs

```sql
-- Semantic type distribution
SELECT
    semantic_type_to_string(semantic_type) as type_name,
    language,
    COUNT(*)
FROM read_ast(['**/*.py', '**/*.js'], context := 'node_types_only', ignore_errors := true)
GROUP BY semantic_type, language
ORDER BY COUNT(*) DESC;
```

### Use `'normalized'` When:

- Name-based analysis
- Symbol extraction
- No need for detailed signatures

```sql
-- Find all named definitions
SELECT name, type, file_path
FROM read_ast('**/*.py', context := 'normalized')
WHERE is_definition(semantic_type)
  AND name IS NOT NULL;
```

### Use `'native'` (Default) When:

- Full semantic analysis
- API documentation
- Detailed code understanding

```sql
-- Complete function inventory
SELECT
    name,
    native as signature,
    file_path,
    start_line
FROM read_ast('src/**/*.py', context := 'native')
WHERE semantic_type = 240  -- DEFINITION_FUNCTION
ORDER BY file_path, start_line;
```

## Performance Comparison

```sql
-- Benchmark different context levels
-- (Run each separately and compare times)

-- Fastest
SELECT COUNT(*) FROM read_ast('**/*.py', context := 'none', ignore_errors := true);

-- Fast
SELECT COUNT(*) FROM read_ast('**/*.py', context := 'node_types_only', ignore_errors := true);

-- Medium
SELECT COUNT(*) FROM read_ast('**/*.py', context := 'normalized', ignore_errors := true);

-- Detailed (default)
SELECT COUNT(*) FROM read_ast('**/*.py', context := 'native', ignore_errors := true);
```

## Combining with Other Parameters

### With Source Control

```sql
-- Full context, full source text
SELECT type, name, native, peek
FROM read_ast('example.py', context := 'native', source := 'full');
```

### With Structure Control

```sql
-- Minimal structure, full context
SELECT type, name, semantic_type
FROM read_ast('example.py', context := 'native', structure := 'minimal');
```

### Optimized for Large Codebases

```sql
-- Balance performance and information
SELECT
    file_path,
    name,
    semantic_type_to_string(semantic_type) as type_name,
    start_line
FROM read_ast(
    '**/*.py',
    context := 'normalized',  -- Faster than 'native'
    source := 'none',         -- Skip source extraction
    structure := 'minimal',   -- Minimal tree info
    ignore_errors := true
)
WHERE is_definition(semantic_type);
```

## Next Steps

- [Cross-Language Analysis](cross-language.md) - Comparing across languages
- [Semantic Types](semantic-types.md) - Type system details
- [API Reference](../api/parameters.md) - All parameters
