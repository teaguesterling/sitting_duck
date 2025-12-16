# Enterprise Languages

Java, C#, Kotlin, and Swift support in Sitting Duck.

## Java

**Extensions:** `.java`
**Identifier:** `'java'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Classes |
| `interface_declaration` | DEFINITION_CLASS | Interfaces |
| `method_declaration` | DEFINITION_FUNCTION | Methods |
| `constructor_declaration` | DEFINITION_FUNCTION | Constructors |
| `enum_declaration` | DEFINITION_CLASS | Enums |
| `import_declaration` | EXTERNAL_IMPORT | Imports |

### Examples

```sql
-- Find all classes
SELECT name, start_line
FROM read_ast('**/*.java')
WHERE type = 'class_declaration';

-- Find interfaces
SELECT name
FROM read_ast('**/*.java')
WHERE type = 'interface_declaration';

-- Find methods with their signatures
SELECT name, native as signature
FROM read_ast('**/*.java', context := 'native')
WHERE type = 'method_declaration';

-- Find Spring annotations
SELECT peek
FROM read_ast('**/*.java')
WHERE type = 'annotation'
  AND peek LIKE '%@%';
```

---

## C#

**Extensions:** `.cs`
**Identifier:** `'csharp'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Classes |
| `interface_declaration` | DEFINITION_CLASS | Interfaces |
| `method_declaration` | DEFINITION_FUNCTION | Methods |
| `property_declaration` | DEFINITION_VARIABLE | Properties |
| `namespace_declaration` | DEFINITION_MODULE | Namespaces |

### Examples

```sql
-- Find classes
SELECT name, start_line
FROM read_ast('**/*.cs')
WHERE type = 'class_declaration';

-- Find properties
SELECT name
FROM read_ast('**/*.cs')
WHERE type = 'property_declaration';

-- Find namespaces
SELECT name
FROM read_ast('**/*.cs')
WHERE type = 'namespace_declaration';

-- Find async methods
SELECT name, peek
FROM read_ast('**/*.cs')
WHERE type = 'method_declaration'
  AND peek LIKE '%async%';
```

---

## Kotlin

**Extensions:** `.kt`, `.kts`
**Identifier:** `'kotlin'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Classes |
| `function_declaration` | DEFINITION_FUNCTION | Functions |
| `property_declaration` | DEFINITION_VARIABLE | Properties |
| `object_declaration` | DEFINITION_CLASS | Objects |
| `interface_declaration` | DEFINITION_CLASS | Interfaces |

### Examples

```sql
-- Find data classes
SELECT name, peek
FROM read_ast('**/*.kt')
WHERE type = 'class_declaration'
  AND peek LIKE '%data class%';

-- Find functions
SELECT name, start_line
FROM read_ast('**/*.kt')
WHERE type = 'function_declaration';

-- Find object declarations (singletons)
SELECT name
FROM read_ast('**/*.kt')
WHERE type = 'object_declaration';

-- Find suspend functions
SELECT name, peek
FROM read_ast('**/*.kt')
WHERE type = 'function_declaration'
  AND peek LIKE '%suspend%';
```

---

## Swift

**Extensions:** `.swift`
**Identifier:** `'swift'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Classes |
| `struct_declaration` | DEFINITION_CLASS | Structs |
| `function_declaration` | DEFINITION_FUNCTION | Functions |
| `protocol_declaration` | DEFINITION_CLASS | Protocols |
| `enum_declaration` | DEFINITION_CLASS | Enums |

### Examples

```sql
-- Find classes and structs
SELECT name, type
FROM read_ast('**/*.swift')
WHERE type IN ('class_declaration', 'struct_declaration');

-- Find protocols
SELECT name
FROM read_ast('**/*.swift')
WHERE type = 'protocol_declaration';

-- Find functions
SELECT name, start_line
FROM read_ast('**/*.swift')
WHERE type = 'function_declaration';

-- Find async functions
SELECT name, peek
FROM read_ast('**/*.swift')
WHERE type = 'function_declaration'
  AND peek LIKE '%async%';
```

## Cross-Enterprise Analysis

```sql
-- Compare enterprise language patterns
SELECT
    language,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as methods,
    COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as classes,
    COUNT(*) as total_nodes
FROM read_ast(['**/*.java', '**/*.cs', '**/*.kt', '**/*.swift'], ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;

-- Find similar class patterns across languages
SELECT
    language,
    name,
    descendant_count as complexity
FROM read_ast(['**/*.java', '**/*.cs', '**/*.kt'], ignore_errors := true)
WHERE semantic_type = 248
  AND name LIKE '%Service%'
ORDER BY complexity DESC;
```
