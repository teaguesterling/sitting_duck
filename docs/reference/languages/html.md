# Html Node Types

> HTML language node type mappings for AST semantic extraction

## Language Characteristics

- **Markup language**: Structure and content of web documents
- **Elements**: Nested tag-based structure `<tag>content</tag>`
- **Attributes**: Key-value pairs on elements `id="value"`
- **Self-closing tags**: `<img />`, `<br />`
- **DOCTYPE**: Document type declaration
- **Comments**: `<!-- comment -->`
- **Embedded content**: `<script>` and `<style>` elements
- **HTML5**: Semantic elements like `<article>`, `<section>`, `<nav>`
- **Entities**: Character references like `&nbsp;`, `&#x00A0;`

## Semantic Type Encoding

Semantic types use an 8-bit encoding:
- Bits 7-2: Base semantic category (e.g., DEFINITION_CLASS = 0x08)
- Bits 1-0: Refinement within category (e.g., Class::REGULAR = 0x00)

## DEF_TYPE Macro Parameters

```cpp
DEF_TYPE(raw_type, semantic_type, name_extraction, native_extraction, flags)
```

| Parameter | Description |
|-----------|-------------|
| raw_type | Tree-sitter node type string |
| semantic_type | Semantic category with optional refinement |
| name_extraction | Strategy for extracting node name |
| native_extraction | Strategy for rich context extraction |
| flags | Behavioral flags (IS_CONSTRUCT, IS_KEYWORD, IS_EMBODIED, etc.) |

## Node Categories

- [Document Structure](#document-structure)
- [Elements](#elements)
- [Attributes](#attributes)
- [Text and Content](#text-and-content)
- [Error Nodes](#error-nodes)
- [DOCTYPE Components](#doctype-components)
- [Punctuation and Delimiters](#punctuation-and-delimiters)
- [Common HTML Tag Names](#common-html-tag-names)
- [Common HTML Attributes](#common-html-attributes)
- [Parser Error Handling](#parser-error-handling)

## Document Structure

Top-level HTML structure

HTML document structure: - `<!DOCTYPE html>` - document type declaration - `<html>` - root element - `<head>` - metadata section - `<body>` - content section

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `document` | DEFINITION_MODULE | NONE | Document root - complete HTML document |
| `doctype` | METADATA_DIRECTIVE | NODE_TEXT | DOCTYPE declaration - `<!DOCTYPE html>` |

## Elements

HTML element constructs

HTML elements: - `<tag>content</tag>` - paired tags - `<tag />` - self-closing tags - Start tag contains attributes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `element` | ORGANIZATION_BLOCK | CUSTOM | Element - `<tag>content</tag>` |
| `script_element` | EXTERNAL_EMBED | CUSTOM | Script element - `<script>code</script>` |
| `style_element` | EXTERNAL_EMBED | CUSTOM | Style element - `<style>css</style>` |
| `start_tag` | PARSER_DELIMITER | CUSTOM | Start tag - `<tag attr="value">` |
| `end_tag` | PARSER_DELIMITER | CUSTOM | End tag - `</tag>` |
| `self_closing_tag` | PARSER_DELIMITER | CUSTOM | Self-closing tag - `<tag />` |
| `tag_name` | NAME_IDENTIFIER | NODE_TEXT | Tag name - element name like `div`, `span` |

## Attributes

HTML element attributes

HTML attributes: - `name="value"` - quoted value - `name=value` - unquoted value - `name` - boolean attribute

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `attribute` | DEFINITION_VARIABLE | CUSTOM | Attribute - `name="value"` |
| `attribute_name` | NAME_IDENTIFIER | NODE_TEXT | Attribute name - the attribute identifier |
| `attribute_value` | LITERAL_STRING | NODE_TEXT | Attribute value - the value part |
| `quoted_attribute_value` | LITERAL_STRING | NODE_TEXT | Quoted attribute value - `"value"` |

## Text and Content

Text content and special constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `text` | LITERAL_STRING | NODE_TEXT | Text - plain text content |
| `raw_text` | LITERAL_STRING | NODE_TEXT | Raw text - text in script/style elements |
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment - `<!-- comment -->` |
| `entity` | LITERAL_STRING | NODE_TEXT | Entity - character reference like `&nbsp;` |

## Error Nodes

Parser error constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `erroneous_end_tag` | PARSER_SYNTAX | NODE_TEXT | Erroneous end tag - mismatched closing tag |
| `erroneous_end_tag_name` | PARSER_SYNTAX | NODE_TEXT | Erroneous end tag name - name of mismatched tag |

## DOCTYPE Components

DOCTYPE declaration parts

## Punctuation and Delimiters

HTML syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `>` | PARSER_DELIMITER | NONE | Greater than - `>` |
| `<` | PARSER_DELIMITER | NONE | Less than - `<` |
| `=` | OPERATOR_ASSIGNMENT | NONE | Equals - `=` |
| `</` | PARSER_DELIMITER | NONE | End tag open - `</` |
| `/>` | PARSER_DELIMITER | NONE | Self-closing - `/>` |
| `<!` | PARSER_DELIMITER | NONE | DOCTYPE open - `<!` |

## Common HTML Tag Names

Frequently used HTML element names

These tag names are handled by the generic `tag_name` type, but are included for explicit recognition and IS_KEYWORD flagging.

## Common HTML Attributes

Frequently used attribute names

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `html_types.def`*
