# Toml Node Types

> TOML configuration format node type mappings for AST semantic extraction

## Node Categories

- [Document Structure](#document-structure)
- [Table Definitions](#table-definitions)
- [Key-Value Pairs](#key-value-pairs)
- [Key Types](#key-types)
- [Collections](#collections)
- [Literal Values](#literal-values)
- [Date/Time Types](#date-time-types)
- [String Content](#string-content)
- [Comments](#comments)
- [Punctuation](#punctuation)
- [Parser Error Handling](#parser-error-handling)

## Document Structure

Top-level TOML document

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `document` | DEFINITION_MODULE | NONE | Document - root TOML container |

## Table Definitions

TOML section groupings

Table types: - Standard tables: `[section]` - Dotted tables: `[parent.child]` - Array tables: `[[array]]` for repeated sections

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `table` | DEFINITION_CLASS | CUSTOM | Table - `[section]` |
| `table_array_element` | DEFINITION_CLASS | CUSTOM | Array table element - `[[array]]` |

## Key-Value Pairs

TOML assignment syntax

Key-value format: - Basic: `key = value` - Quoted: `"key" = value` - Dotted: `parent.child = value`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `pair` | DEFINITION_VARIABLE | CUSTOM | Key-value pair - `key = value` |

## Key Types

TOML key formats

Key formats: - Bare keys: `key` (alphanumeric, `_`, `-`) - Quoted keys: `"key with spaces"` - Dotted keys: `parent.child.grandchild`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `bare_key` | NAME_IDENTIFIER | NODE_TEXT | Bare key - unquoted identifier |
| `quoted_key` | NAME_IDENTIFIER | NODE_TEXT | Quoted key - `"key"` or `'key'` |
| `dotted_key` | NAME_IDENTIFIER | CUSTOM | Dotted key - `parent.child` |

## Collections

TOML inline structures

Collection types: - Inline tables: `{key = value}` - Arrays: `[value1, value2]`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `inline_table` | LITERAL_STRUCTURED | NONE | Inline table - `{key = value, ...}` |
| `array` | LITERAL_STRUCTURED | NONE | Array - `[value, ...]` |

## Literal Values

TOML primitive types

TOML value types: - Strings: basic `"..."`, literal `'...'` - Integers: decimal, hex `0x`, octal `0o`, binary `0b` - Floats: `3.14`, `inf`, `nan` - Booleans: `true`, `false`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string` | LITERAL_STRING | NODE_TEXT | String literal - `"value"` or `'value'` |
| `integer` | LITERAL_NUMBER | NODE_TEXT | Integer literal - `42`, `0xFF` |
| `float` | LITERAL_NUMBER | NODE_TEXT | Float literal - `3.14`, `inf` |
| `boolean` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true`, `false` |

## Date/Time Types

TOML temporal values (RFC 3339)

Date/time formats: - Local date: `2024-01-15` - Local time: `14:30:00` - Local datetime: `2024-01-15T14:30:00` - Offset datetime: `2024-01-15T14:30:00-05:00`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `local_date` | LITERAL_ATOMIC | NODE_TEXT | Local date - `2024-01-15` |
| `local_time` | LITERAL_ATOMIC | NODE_TEXT | Local time - `14:30:00` |
| `local_date_time` | LITERAL_ATOMIC | NODE_TEXT | Local datetime - `2024-01-15T14:30:00` |
| `offset_date_time` | LITERAL_ATOMIC | NODE_TEXT | Offset datetime - `2024-01-15T14:30:00-05:00` |

## String Content

String internals

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `escape_sequence` | LITERAL_STRING | NODE_TEXT | Escape sequence - `\n`, `\t`, etc. |

## Comments

TOML comment syntax

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment - `# comment` |

## Punctuation

TOML syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `{` | PARSER_DELIMITER | NODE_TEXT | Opening brace - inline table `{` |
| `}` | PARSER_DELIMITER | NODE_TEXT | Closing brace - inline table `}` |
| `[` | PARSER_DELIMITER | NODE_TEXT | Opening bracket - array or table `[` |
| `]` | PARSER_DELIMITER | NODE_TEXT | Closing bracket - array or table `]` |
| `[[` | PARSER_DELIMITER | NODE_TEXT | Double opening bracket - array table `[[` |
| `]]` | PARSER_DELIMITER | NODE_TEXT | Double closing bracket - array table `]]` |
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Equals - assignment `=` |
| `,` | PARSER_PUNCTUATION | NODE_TEXT | Comma - element separator `,` |
| `.` | PARSER_PUNCTUATION | NODE_TEXT | Period - dotted key separator `.` |
| `'` | PARSER_DELIMITER | NODE_TEXT | Single quote - literal string delimiter `'` |
| `'''` | PARSER_DELIMITER | NODE_TEXT | Triple single quote - multi-line literal `'''` |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `toml_types.def`*
