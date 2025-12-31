# Json Node Types

> JSON data format node type mappings for AST semantic extraction

## Node Categories

- [Document Structure](#document-structure)
- [Object Structure](#object-structure)
- [Array Structure](#array-structure)
- [Literals and Values](#literals-and-values)
- [String Content](#string-content)
- [Punctuation](#punctuation)
- [Parser Error Handling](#parser-error-handling)

## Document Structure

Top-level JSON document

JSON document is the root container for all content. Valid JSON must have exactly one root value.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `document` | DEFINITION_MODULE | NONE | Document - root JSON container |

## Object Structure

JSON object constructs

JSON objects: - Unordered key-value collections - Keys must be strings - `{ "key": value, "key2": value2 }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `object` | LITERAL_STRUCTURED | NONE | Object - `{ key: value, ... }` |
| `pair` | PATTERN_DESTRUCTURE | FIND_PROPERTY | Key-value pair - `"key": value` |

## Array Structure

JSON array constructs

JSON arrays: - Ordered collections of values - Mixed types allowed - `[ value1, value2, value3 ]`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `array` | LITERAL_STRUCTURED | NONE | Array - `[ value, ... ]` |

## Literals and Values

JSON primitive values

JSON value types: - Strings: `"text"` (double quotes required) - Numbers: `42`, `3.14`, `-1e10` - Booleans: `true`, `false` - Null: `null`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string` | LITERAL_STRING | NODE_TEXT | String literal - `"text"` |
| `number` | LITERAL_NUMBER | NODE_TEXT | Number literal - `42`, `3.14`, `-1e10` |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean true - `true` |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean false - `false` |
| `null` | LITERAL_ATOMIC | NODE_TEXT | Null value - `null` |

## String Content

String internals

String components: - Content between quotes - Escape sequences: `\n`, `\t`, `\"`, `\\`, `\uXXXX`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string_content` | LITERAL_STRING | NODE_TEXT | String content - text inside quotes |
| `escape_sequence` | LITERAL_STRING | NODE_TEXT | Escape sequence - `\n`, `\uXXXX`, etc. |

## Punctuation

JSON syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `{` | PARSER_DELIMITER | NODE_TEXT | Opening brace - `{` |
| `}` | PARSER_DELIMITER | NODE_TEXT | Closing brace - `}` |
| `[` | PARSER_DELIMITER | NODE_TEXT | Opening bracket - `[` |
| `]` | PARSER_DELIMITER | NODE_TEXT | Closing bracket - `]` |
| `:` | PARSER_PUNCTUATION | NODE_TEXT | Colon - key-value separator `:` |
| `,` | PARSER_PUNCTUATION | NODE_TEXT | Comma - element separator `,` |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `json_types.def`*
