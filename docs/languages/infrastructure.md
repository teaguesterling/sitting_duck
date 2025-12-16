# Infrastructure Languages

HCL (Terraform), JSON, TOML, and GraphQL support in Sitting Duck.

## HCL (Terraform)

**Extensions:** `.hcl`, `.tf`, `.tfvars`
**Identifier:** `'hcl'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `block` | ORGANIZATION_SECTION | HCL blocks |
| `attribute` | DEFINITION_VARIABLE | Attributes |
| `identifier` | NAME_IDENTIFIER | Identifiers |
| `string_lit` | LITERAL_STRING | Strings |

### Examples

```sql
-- Find all blocks
SELECT type, name, peek
FROM read_ast('**/*.tf')
WHERE type = 'block';

-- Find resource definitions
SELECT peek
FROM read_ast('**/*.tf')
WHERE type = 'block'
  AND peek LIKE 'resource %';

-- Find variables
SELECT peek
FROM read_ast('**/*.tf')
WHERE type = 'block'
  AND peek LIKE 'variable %';

-- Find module references
SELECT peek
FROM read_ast('**/*.tf')
WHERE type = 'block'
  AND peek LIKE 'module %';
```

---

## JSON

**Extensions:** `.json`
**Identifier:** `'json'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `object` | LITERAL_STRUCTURED | Objects |
| `array` | LITERAL_STRUCTURED | Arrays |
| `pair` | DEFINITION_VARIABLE | Key-value pairs |
| `string` | LITERAL_STRING | Strings |
| `number` | LITERAL_NUMBER | Numbers |

### Examples

```sql
-- Find all objects
SELECT depth, children_count
FROM read_ast('config.json')
WHERE type = 'object';

-- Find all key-value pairs
SELECT name, peek
FROM read_ast('config.json')
WHERE type = 'pair';

-- Analyze JSON structure depth
SELECT
    file_path,
    MAX(depth) as max_depth,
    COUNT(*) as total_nodes
FROM read_ast('**/*.json')
GROUP BY file_path
ORDER BY max_depth DESC;
```

---

## TOML

**Extensions:** `.toml`
**Identifier:** `'toml'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `table` | ORGANIZATION_SECTION | Tables |
| `pair` | DEFINITION_VARIABLE | Key-value pairs |
| `string` | LITERAL_STRING | Strings |
| `integer` | LITERAL_NUMBER | Integers |
| `array` | LITERAL_STRUCTURED | Arrays |

### Examples

```sql
-- Find all tables
SELECT name, peek
FROM read_ast('**/*.toml')
WHERE type = 'table';

-- Find all key-value pairs
SELECT peek
FROM read_ast('Cargo.toml')
WHERE type = 'pair';

-- Analyze pyproject.toml
SELECT type, name
FROM read_ast('pyproject.toml')
WHERE type IN ('table', 'pair');
```

---

## GraphQL

**Extensions:** `.graphql`, `.gql`
**Identifier:** `'graphql'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `type_definition` | DEFINITION_CLASS | Type definitions |
| `field_definition` | DEFINITION_VARIABLE | Field definitions |
| `operation_definition` | DEFINITION_FUNCTION | Operations |
| `input_object_type_definition` | DEFINITION_CLASS | Input types |
| `enum_type_definition` | DEFINITION_CLASS | Enums |

### Examples

```sql
-- Find type definitions
SELECT name, peek
FROM read_ast('**/*.graphql')
WHERE type = 'type_definition';

-- Find queries and mutations
SELECT name, type
FROM read_ast('**/*.graphql')
WHERE type = 'operation_definition';

-- Find field definitions
SELECT name
FROM read_ast('schema.graphql')
WHERE type = 'field_definition';

-- Find input types
SELECT name
FROM read_ast('**/*.graphql')
WHERE type = 'input_object_type_definition';
```

## Cross-Infrastructure Analysis

```sql
-- Analyze infrastructure files
SELECT
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as total_nodes
FROM read_ast(['**/*.tf', '**/*.json', '**/*.toml', '**/*.graphql'], ignore_errors := true)
GROUP BY language
ORDER BY files DESC;

-- Find all configuration keys
SELECT
    file_path,
    language,
    COUNT(CASE WHEN semantic_type = 244 THEN 1 END) as key_count
FROM read_ast(['**/*.tf', '**/*.json', '**/*.toml'], ignore_errors := true)
GROUP BY file_path, language
ORDER BY key_count DESC;
```

## Infrastructure as Code Patterns

```sql
-- Analyze Terraform resources
SELECT
    peek,
    descendant_count as complexity
FROM read_ast('**/*.tf')
WHERE type = 'block'
  AND peek LIKE 'resource %'
ORDER BY complexity DESC;

-- Find package dependencies (package.json)
SELECT peek
FROM read_ast('package.json')
WHERE type = 'pair'
  AND (peek LIKE '%dependencies%' OR peek LIKE '%devDependencies%');
```
