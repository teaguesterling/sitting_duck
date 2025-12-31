# Yaml Node Types

> YAML data format node type mappings for AST semantic extraction

## Node Categories

- [Document Structure](#document-structure)
- [Block Collections](#block-collections)
- [Flow Collections](#flow-collections)
- [Scalar Values](#scalar-values)
- [Typed Values](#typed-values)
- [Tags and Anchors](#tags-and-anchors)
- [Directives](#directives)
- [Comments](#comments)
- [String Content](#string-content)
- [Punctuation and Structure](#punctuation-and-structure)
- [Block Scalar Indicators](#block-scalar-indicators)
- [Parser Error Handling](#parser-error-handling)

## Document Structure

Top-level YAML structure

YAML document organization: - Stream contains one or more documents - Documents separated by `---` - Documents terminated by `...`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `stream` | DEFINITION_MODULE | NONE | Stream - root node containing all documents |
| `document` | DEFINITION_MODULE | NONE | Document - single YAML document within stream |

## Block Collections

Indentation-based collections

Block style uses indentation for structure: - Mappings: key-value pairs - Sequences: ordered lists with `-` markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block_mapping` | LITERAL_STRUCTURED | NONE | Block mapping - indentation-based key-value pairs |
| `block_mapping_pair` | PATTERN_DESTRUCTURE | FIND_PROPERTY | Block mapping pair - `key: value` on separate lines |
| `block_sequence` | LITERAL_STRUCTURED | NONE | Block sequence - indentation-based list |
| `block_sequence_item` | ORGANIZATION_LIST | NONE | Block sequence item - `- value` |

## Flow Collections

JSON-like inline collections

Flow style uses explicit delimiters: - `{key: value}` for mappings - `[item1, item2]` for sequences

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `flow_mapping` | LITERAL_STRUCTURED | NONE | Flow mapping - `{key: value, ...}` |
| `flow_pair` | PATTERN_DESTRUCTURE | FIND_PROPERTY | Flow pair - `key: value` in flow context |
| `flow_sequence` | LITERAL_STRUCTURED | NONE | Flow sequence - `[value, ...]` |

## Scalar Values

YAML scalar types

Scalar styles: - Plain: unquoted `value` - Single-quoted: `'value'` - Double-quoted: `"value"` with escapes - Literal: `|` preserves newlines - Folded: `>` folds newlines to spaces

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `plain_scalar` | LITERAL_STRING | NODE_TEXT | Plain scalar - unquoted value |
| `single_quote_scalar` | LITERAL_STRING | NODE_TEXT | Single-quoted scalar - `'value'` |
| `double_quote_scalar` | LITERAL_STRING | NODE_TEXT | Double-quoted scalar - `"value"` |
| `literal_scalar` | LITERAL_STRING | NODE_TEXT | Literal scalar - `|` block (preserves newlines) |
| `folded_scalar` | LITERAL_STRING | NODE_TEXT | Folded scalar - `>` block (folds newlines) |

## Typed Values

YAML values with implicit types

YAML auto-detects types for unquoted values: - Booleans: `true`, `false`, `yes`, `no` - Null: `null`, `~` - Numbers: integers, floats

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `boolean_scalar` | LITERAL_ATOMIC | NODE_TEXT | Boolean scalar - `true`, `false`, `yes`, `no` |
| `null_scalar` | LITERAL_ATOMIC | NODE_TEXT | Null scalar - `null`, `~` |
| `integer_scalar` | LITERAL_NUMBER | NODE_TEXT | Integer scalar - `42`, `0x2A`, `0o52` |
| `float_scalar` | LITERAL_NUMBER | NODE_TEXT | Float scalar - `3.14`, `.inf`, `.nan` |

## Tags and Anchors

YAML metadata constructs

YAML references: - Tags: `!!str`, `!custom` type specification - Anchors: `&name` define reusable node - Aliases: `*name` reference anchored node

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `tag` | METADATA_ANNOTATION | NODE_TEXT | Tag - `!tag` or `!!type` type specifier |
| `anchor` | METADATA_ANNOTATION | NODE_TEXT | Anchor - `&name` reusable reference definition |
| `alias` | COMPUTATION_ACCESS | NODE_TEXT | Alias - `*name` reference to anchored node |

## Directives

YAML document directives

Directive types: - `%YAML 1.2` - version specification - `%TAG !prefix! uri` - tag shorthand

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `directive` | METADATA_DIRECTIVE | NODE_TEXT | Generic directive - `%DIRECTIVE` |
| `yaml_directive` | METADATA_DIRECTIVE | NODE_TEXT | YAML directive - `%YAML 1.2` |
| `tag_directive` | METADATA_DIRECTIVE | NODE_TEXT | Tag directive - `%TAG !prefix! uri` |
| `reserved_directive` | METADATA_DIRECTIVE | NODE_TEXT | Reserved directive - future use |

## Comments

YAML comment syntax

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment - `# comment` |

## String Content

String internals

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string_scalar` | LITERAL_STRING | NODE_TEXT | String scalar - generic string content |
| `escape_sequence` | LITERAL_STRING | NODE_TEXT | Escape sequence - `\n`, `\x00`, etc. |

## Punctuation and Structure

YAML syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `:` | PARSER_PUNCTUATION | NODE_TEXT | Colon - key-value separator `:` |
| `,` | PARSER_PUNCTUATION | NODE_TEXT | Comma - flow element separator `,` |
| `-` | PARSER_PUNCTUATION | NODE_TEXT | Dash - sequence item marker `-` |
| `[` | PARSER_DELIMITER | NODE_TEXT | Opening bracket - flow sequence `[` |
| `]` | PARSER_DELIMITER | NODE_TEXT | Closing bracket - flow sequence `]` |
| `{` | PARSER_DELIMITER | NODE_TEXT | Opening brace - flow mapping `{` |
| `}` | PARSER_DELIMITER | NODE_TEXT | Closing brace - flow mapping `}` |
| `|` | PARSER_PUNCTUATION | NODE_TEXT | Pipe - literal scalar indicator `|` |
| `>` | PARSER_PUNCTUATION | NODE_TEXT | Greater than - folded scalar indicator `>` |
| `---` | PARSER_DELIMITER | NODE_TEXT | Document start marker - `---` |
| `...` | PARSER_DELIMITER | NODE_TEXT | Document end marker - `...` |

## Block Scalar Indicators

Literal/folded scalar modifiers

Block scalar modifiers: - Chomping: `-` strip, `+` keep trailing newlines - Indentation: explicit indent level

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block_scalar_header` | ORGANIZATION_SCOPE | NONE | Block scalar header - `|2-` modifiers |
| `block_chomping_indicator` | METADATA_ANNOTATION | NODE_TEXT | Chomping indicator - `-` or `+` |
| `block_indentation_indicator` | METADATA_ANNOTATION | NODE_TEXT | Indentation indicator - explicit indent level |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `yaml_types.def`*
