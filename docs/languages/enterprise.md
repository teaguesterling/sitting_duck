# Enterprise & Mobile Languages

Java, C#, Kotlin, Swift, and Dart support in Sitting Duck.

## Language Nuances

### Extraction Quality Summary

| Language | Functions | Classes | Calls | Variables | Body Detection | Overall |
|----------|-----------|---------|-------|-----------|----------------|---------|
| **Java** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Excellent |
| **C#** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Very Good |
| **Kotlin** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Very Good |
| **Swift** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Good |
| **Dart** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐ | Very Good |

### Implementation Notes

- **Abstract Method Detection**: Java and C# use runtime body detection to automatically identify abstract methods and interface method declarations. These are marked with `IS_DECLARATION_ONLY` flag.
- **Return Type Extraction**: Java provides excellent return type extraction including generics (`List<String>`, `Map<K,V>`).
- **Modifier Extraction**: Java extracts access modifiers (`public`, `private`, `static`, `final`) into the `modifiers` array.
- **Interface Methods**: Interface method declarations (without bodies) are correctly identified as declaration-only.

### Known Limitations

- **C# Properties**: Property getters/setters are extracted but auto-implemented properties may not show full detail.
- **Kotlin Extensions**: Extension functions are parsed but the receiver type may not be fully extracted.
- **Swift Protocols**: Protocol method requirements are detected as declaration-only.
- **Dart Sibling Structure**: Dart's grammar uses sibling structure for function signatures (signature and body are siblings, not parent-child). Function signatures are explicitly marked with `IS_DECLARATION_ONLY`.

---

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

---

## Dart

**Extensions:** `.dart`
**Identifier:** `'dart'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `class_definition` | DEFINITION_CLASS | Classes |
| `function_signature` | DEFINITION_FUNCTION | Function signatures |
| `method_signature` | DEFINITION_FUNCTION | Method signatures |
| `enum_declaration` | DEFINITION_CLASS | Enums |
| `mixin_declaration` | DEFINITION_CLASS | Mixins |

### Examples

```sql
-- Find all classes
SELECT name, start_line
FROM read_ast('**/*.dart')
WHERE type = 'class_definition';

-- Find functions
SELECT name, start_line
FROM read_ast('**/*.dart')
WHERE type IN ('function_signature', 'method_signature');

-- Find async functions
SELECT name, peek
FROM read_ast('**/*.dart')
WHERE type = 'function_signature'
  AND peek LIKE '%async%';

-- Find mixins
SELECT name
FROM read_ast('**/*.dart')
WHERE type = 'mixin_declaration';
```

### Dart-Specific Notes

Dart uses a sibling structure in its grammar where function signatures and bodies are siblings rather than parent-child:

```dart
// In Dart's AST:
// - function_signature: "void myFunc(int x)"
// - function_body: "{ ... }"
// These are siblings, not nested
```

This means:
- Function signatures are explicitly marked with `IS_DECLARATION_ONLY`
- Body detection accuracy is ⭐⭐ (requires explicit marking rather than runtime detection)
- Abstract methods and interface methods work correctly

---

## Cross-Enterprise Analysis

```sql
-- Compare enterprise language patterns
SELECT
    language,
    COUNT(CASE WHEN semantic_type = 240 THEN 1 END) as methods,
    COUNT(CASE WHEN semantic_type = 248 THEN 1 END) as classes,
    COUNT(*) as total_nodes
FROM read_ast(['**/*.java', '**/*.cs', '**/*.kt', '**/*.swift', '**/*.dart'], ignore_errors := true)
GROUP BY language
ORDER BY total_nodes DESC;

-- Find similar class patterns across languages
SELECT
    language,
    name,
    descendant_count as complexity
FROM read_ast(['**/*.java', '**/*.cs', '**/*.kt', '**/*.dart'], ignore_errors := true)
WHERE semantic_type = 248
  AND name LIKE '%Service%'
ORDER BY complexity DESC;
```
