# Web Languages

JavaScript, TypeScript, HTML, and CSS support in Sitting Duck.

## Language Nuances

### Extraction Quality Summary

| Language | Functions | Classes | Calls | Variables | Body Detection | Overall |
|----------|-----------|---------|-------|-----------|----------------|---------|
| **JavaScript** | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Good |
| **TypeScript** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Very Good |
| **HTML** | N/A | N/A | N/A | N/A | N/A | Structural |
| **CSS** | N/A | N/A | N/A | N/A | N/A | Structural |

### Implementation Notes

- **TypeScript Interfaces**: Interface method signatures (`method_signature`, `property_signature`, `call_signature`, `construct_signature`, `index_signature`) are marked with `IS_DECLARATION_ONLY`.
- **JavaScript Arrow Functions**: Arrow functions are correctly detected as `DEFINITION_FUNCTION` with `Function::LAMBDA` refinement.
- **TypeScript vs JavaScript**: TypeScript inherits JavaScript patterns but adds type-aware extraction.
- **Declaration Keywords**: `const`, `let`, `var` are extracted into the `modifiers` array for variable declarations.

### Known Limitations

- **JSX/TSX**: JSX elements are parsed but component detection relies on naming conventions (capitalized names).
- **JavaScript Types**: No type information available (dynamic typing) - `signature_type` returns `'function'` literal.
- **TypeScript Generics**: Generic type parameters are parsed but not fully extracted.
- **CSS Variables**: Custom properties (`--var-name`) are parsed but not semantically distinguished.

### Body Detection Details

| Language | Declaration Types | Notes |
|----------|-------------------|-------|
| JavaScript | `method_signature`, `property_signature` | Interface-like patterns in `.d.ts` files |
| TypeScript | `method_signature`, `property_signature`, `call_signature`, `construct_signature`, `index_signature` | Full interface support with IS_DECLARATION_ONLY |

### HTML/CSS Notes

HTML and CSS are structural languages without functions or classes:
- **HTML**: Elements, attributes, and text nodes are extracted with semantic types like `ORGANIZATION_SECTION` and `NAME_IDENTIFIER`.
- **CSS**: Rules, selectors, and declarations are extracted. Selectors are identified as `NAME_IDENTIFIER`.

---

## JavaScript

**Extensions:** `.js`, `.jsx`
**Identifier:** `'javascript'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Named functions |
| `arrow_function` | DEFINITION_FUNCTION | Arrow functions |
| `class_declaration` | DEFINITION_CLASS | Class definitions |
| `variable_declaration` | DEFINITION_VARIABLE | var/let/const |
| `call_expression` | COMPUTATION_CALL | Function calls |
| `import_statement` | EXTERNAL_IMPORT | ES6 imports |
| `export_statement` | EXTERNAL_EXPORT | ES6 exports |

### Examples

```sql
-- Find all functions
SELECT name, type, start_line
FROM read_ast('app.js')
WHERE semantic_type = 240;

-- Find React components (functions returning JSX)
SELECT name, peek
FROM read_ast('**/*.jsx')
WHERE type IN ('function_declaration', 'arrow_function')
  AND name LIKE '%Component%';

-- Find imports
SELECT peek
FROM read_ast('app.js')
WHERE type = 'import_statement';
```

---

## TypeScript

**Extensions:** `.ts`, `.tsx`
**Identifier:** `'typescript'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Functions |
| `class_declaration` | DEFINITION_CLASS | Classes |
| `interface_declaration` | DEFINITION_CLASS | Interfaces |
| `type_alias_declaration` | TYPE_COMPOSITE | Type aliases |
| `enum_declaration` | DEFINITION_CLASS | Enums |

### Examples

```sql
-- Find interfaces
SELECT name, start_line
FROM read_ast('**/*.ts')
WHERE type = 'interface_declaration';

-- Find type definitions
SELECT name, peek
FROM read_ast('types.ts')
WHERE type IN ('interface_declaration', 'type_alias_declaration');

-- Find generic functions
SELECT name, peek
FROM read_ast('**/*.ts')
WHERE type = 'function_declaration'
  AND peek LIKE '%<%>%';
```

---

## HTML

**Extensions:** `.html`, `.htm`
**Identifier:** `'html'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `element` | ORGANIZATION_SECTION | HTML elements |
| `tag_name` | NAME_IDENTIFIER | Tag names |
| `attribute` | NAME_ATTRIBUTE | Attributes |
| `text` | LITERAL_STRING | Text content |

### Examples

```sql
-- Find all elements
SELECT type, name, start_line
FROM read_ast('index.html')
WHERE type = 'element';

-- Find all tag names
SELECT DISTINCT name
FROM read_ast('index.html')
WHERE type = 'tag_name'
ORDER BY name;

-- Find elements with specific attributes
SELECT peek
FROM read_ast('index.html')
WHERE type = 'element'
  AND peek LIKE '%class=%';
```

---

## CSS

**Extensions:** `.css`
**Identifier:** `'css'`

### Common Node Types

| Type | Semantic Type | Description |
|------|---------------|-------------|
| `rule_set` | ORGANIZATION_SECTION | CSS rules |
| `class_selector` | NAME_IDENTIFIER | Class selectors |
| `id_selector` | NAME_IDENTIFIER | ID selectors |
| `property_name` | NAME_PROPERTY | Property names |
| `declaration` | DEFINITION_VARIABLE | Property declarations |

### Examples

```sql
-- Find all CSS rules
SELECT peek
FROM read_ast('styles.css')
WHERE type = 'rule_set';

-- Find all class selectors
SELECT name
FROM read_ast('styles.css')
WHERE type = 'class_selector';

-- Find all properties used
SELECT DISTINCT name
FROM read_ast('**/*.css')
WHERE type = 'property_name'
ORDER BY name;

-- Analyze selector complexity
SELECT
    peek,
    children_count as selector_parts
FROM read_ast('styles.css')
WHERE type = 'rule_set'
ORDER BY selector_parts DESC;
```

## Cross-Web Analysis

```sql
-- Find all JavaScript and TypeScript functions
SELECT
    file_path,
    language,
    name,
    type
FROM read_ast(['**/*.js', '**/*.ts'], ignore_errors := true)
WHERE semantic_type = 240
ORDER BY file_path;

-- Compare front-end file sizes
SELECT
    language,
    COUNT(DISTINCT file_path) as files,
    COUNT(*) as total_nodes
FROM read_ast(['**/*.js', '**/*.ts', '**/*.html', '**/*.css'], ignore_errors := true)
GROUP BY language;
```
