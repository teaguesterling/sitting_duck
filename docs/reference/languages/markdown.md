# Markdown Node Types

> Markdown document format node type mappings for AST semantic extraction

## Node Categories

- [Document Structure](#document-structure)
- [Headings](#headings)
- [Lists](#lists)
- [Code Blocks](#code-blocks)
- [Links and References](#links-and-references)
- [Inline Elements](#inline-elements)
- [Tables](#tables)
- [HTML Content](#html-content)
- [Metadata](#metadata)
- [Punctuation and Delimiters](#punctuation-and-delimiters)
- [Markdown-Specific Constructs](#markdown-specific-constructs)
- [Pipe Tables](#pipe-tables)
- [Task Lists](#task-lists)
- [Strikethrough](#strikethrough)
- [Autolinks](#autolinks)
- [Heading Markers](#heading-markers)
- [Parser Error Handling](#parser-error-handling)

## Document Structure

Top-level Markdown structure

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `document` | DEFINITION_MODULE | NONE | Document - root Markdown container |
| `section` | ORGANIZATION_SECTION | CUSTOM | Section - logical document division |
| `paragraph` | LITERAL_STRING | NODE_TEXT | Paragraph - text block |
| `block_quote` | ORGANIZATION_BLOCK | NONE | Block quote - `> quoted text` |

## Headings

Markdown heading styles

Heading types: - ATX: `# H1`, `## H2`, etc. (1-6 levels) - Setext: Underlined with `===` or `---`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `atx_heading` | ORGANIZATION_SECTION | CUSTOM | ATX heading - `# Heading` |
| `setext_heading` | ORGANIZATION_SECTION | CUSTOM | Setext heading - underlined style |

## Lists

Markdown list constructs

List types: - Unordered: `-`, `*`, `+` markers - Ordered: `1.` or `1)` markers - Nested via indentation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `list` | ORGANIZATION_LIST | NONE | Generic list container |
| `list_item` | ORGANIZATION_BLOCK | NONE | List item - single item in list |

## Code Blocks

Markdown code constructs

Code types: - Fenced: ``` with optional language - Indented: 4-space prefix - Inline: `` `code` ``

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `fenced_code_block` | ORGANIZATION_BLOCK | CUSTOM | Fenced code block - ``` block |
| `indented_code_block` | ORGANIZATION_BLOCK | NONE | Indented code block - 4-space indented |
| `info_string` | METADATA_ANNOTATION | NODE_TEXT | Info string - language after ``` |
| `language` | METADATA_ANNOTATION | NODE_TEXT | Language - extracted language name |

## Links and References

Markdown link constructs

Link types: - Inline: `[text](url "title")` - Reference: `[text][ref]` - Definition: `[ref]: url "title"`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `link_title` | METADATA_ANNOTATION | NODE_TEXT | Link title - optional title |
| `link_destination` | LITERAL_STRING | NODE_TEXT | Link destination - URL/path |
| `link_reference_definition` | DEFINITION_VARIABLE | CUSTOM | Link reference definition - `[ref]: url` |
| `link_label` | NAME_IDENTIFIER | NODE_TEXT | Link label - reference identifier |

## Inline Elements

Markdown inline formatting

Inline styles: - Emphasis: `*text*` or `_text_` - Strong: `**text**` or `__text__` - Code: `` `code` ``

## Tables

GitHub Flavored Markdown tables

Table syntax: ``` | Header | Header | |--------|--------| | Cell   | Cell   | ```

## HTML Content

Embedded HTML in Markdown

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `html_block` | EXTERNAL_EMBED | NODE_TEXT | HTML block - standalone HTML |

## Metadata

Document metadata and structure

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `thematic_break` | PARSER_DELIMITER | NONE | Thematic break - `---`, `***`, `___` |

## Punctuation and Delimiters

Markdown syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `/` | PARSER_PUNCTUATION | NONE | Forward slash - `/` |
| `-` | PARSER_PUNCTUATION | NONE | Dash/minus - `-` |
| `_` | PARSER_PUNCTUATION | NONE | Underscore - `_` |
| `.` | PARSER_PUNCTUATION | NONE | Period - `.` |
| `*` | PARSER_PUNCTUATION | NONE | Asterisk - `*` |
| `,` | PARSER_PUNCTUATION | NONE | Comma - `,` |
| ``` | PARSER_DELIMITER | NONE | Backtick - `` ` `` |
| `)` | PARSER_DELIMITER | NONE | Closing parenthesis - `)` |
| `(` | PARSER_DELIMITER | NONE | Opening parenthesis - `(` |
| `:` | PARSER_PUNCTUATION | NONE | Colon - `:` |
| `=` | PARSER_PUNCTUATION | NONE | Equals - `=` |
| `'` | PARSER_DELIMITER | NONE | Single quote - `'` |
| `[` | PARSER_DELIMITER | NONE | Opening bracket - `[` |
| `]` | PARSER_DELIMITER | NONE | Closing bracket - `]` |
| `;` | PARSER_PUNCTUATION | NONE | Semicolon - `;` |
| `#` | PARSER_PUNCTUATION | NONE | Hash - `#` |
| `+` | PARSER_PUNCTUATION | NONE | Plus - `+` |
| `>` | PARSER_PUNCTUATION | NONE | Greater than - `>` |
| `<` | PARSER_PUNCTUATION | NONE | Less than - `<` |
| `\|` | PARSER_DELIMITER | NONE | Pipe - `\|` |
| `!` | PARSER_PUNCTUATION | NONE | Exclamation - `!` |

## Markdown-Specific Constructs

Markdown structural elements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block_continuation` | ORGANIZATION_BLOCK | NODE_TEXT | Block continuation - continued block |
| `inline` | LITERAL_STRING | NODE_TEXT | Inline content - inline text run |
| `list_marker_minus` | PARSER_DELIMITER | NODE_TEXT | List marker minus - `-` |
| `fenced_code_block_delimiter` | PARSER_DELIMITER | NODE_TEXT | Fenced code block delimiter - ``` or ~~~ |
| `list_marker_plus` | PARSER_DELIMITER | NODE_TEXT | List marker plus - `+` |
| `list_marker_star` | PARSER_DELIMITER | NODE_TEXT | List marker star - `*` |
| `list_marker_dot` | PARSER_DELIMITER | NODE_TEXT | List marker dot - ordered list dot |
| `list_marker_parenthesis` | PARSER_DELIMITER | NODE_TEXT | List marker parenthesis - ordered list paren |
| `code_fence_content` | LITERAL_STRING | NONE | Code fence content - code inside fence |
| `backslash_escape` | LITERAL_STRING | NONE | Backslash escape - `\*` |
| `entity_reference` | LITERAL_STRING | NODE_TEXT | Entity reference - `&nbsp;` |
| `numeric_character_reference` | LITERAL_STRING | NODE_TEXT | Numeric character reference - `&#160;` |

## Pipe Tables

GFM pipe table constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `pipe_table_header` | ORGANIZATION_SECTION | NONE | Pipe table header row |
| `pipe_table_row` | ORGANIZATION_LIST | NONE | Pipe table data row |
| `pipe_table_delimiter_row` | PARSER_DELIMITER | NONE | Pipe table delimiter row |
| `pipe_table_cell` | LITERAL_STRING | NODE_TEXT | Pipe table cell content |

## Task Lists

GitHub Flavored Markdown task lists

Task list syntax: - `- [ ]` unchecked - `- [x]` checked

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `task_list_marker_checked` | PARSER_DELIMITER | NODE_TEXT | Task list marker checked - `[x]` |
| `task_list_marker_unchecked` | PARSER_DELIMITER | NODE_TEXT | Task list marker unchecked - `[ ]` |

## Strikethrough

GFM strikethrough syntax

## Autolinks

Automatic link detection

## Heading Markers

ATX and Setext heading markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `atx_h3_marker` | ORGANIZATION_SECTION | NODE_TEXT | ATX H3 marker - `###` |
| `atx_h2_marker` | ORGANIZATION_SECTION | NODE_TEXT | ATX H2 marker - `##` |
| `{` | PARSER_DELIMITER | NONE | Opening brace - `{` |
| `}` | PARSER_DELIMITER | NONE | Closing brace - `}` |
| `&` | PARSER_PUNCTUATION | NONE | Ampersand - `&` |
| `%` | PARSER_PUNCTUATION | NONE | Percent - `%` |
| `$` | PARSER_PUNCTUATION | NONE | Dollar - `$` |
| `atx_h1_marker` | ORGANIZATION_SECTION | NODE_TEXT | ATX H1 marker - `#` |
| `?` | PARSER_PUNCTUATION | NONE | Question mark - `?` |
| `@` | PARSER_PUNCTUATION | NONE | At sign - `@` |
| `pipe_table_delimiter_cell` | PARSER_DELIMITER | NONE | Pipe table delimiter cell - `---` |
| `atx_h4_marker` | ORGANIZATION_SECTION | NODE_TEXT | ATX H4 marker - `####` |
| `\\` | PARSER_PUNCTUATION | NONE | Backslash - `\` |
| `~` | PARSER_PUNCTUATION | NONE | Tilde - `~` |
| `pipe_table` | ORGANIZATION_CONTAINER | NONE | Pipe table container |
| `^` | PARSER_PUNCTUATION | NONE | Caret - `^` |
| `pipe_table_align_left` | PARSER_DELIMITER | NONE | Pipe table align left - `:---` |
| `block_quote_marker` | PARSER_DELIMITER | NODE_TEXT | Block quote marker - `>` |
| `-->` | PARSER_DELIMITER | NONE | HTML comment end - `-->` |
| `pipe_table_align_right` | PARSER_DELIMITER | NONE | Pipe table align right - `---:` |
| `minus_metadata` | METADATA_ANNOTATION | NODE_TEXT | Minus metadata - frontmatter delimiter |
| `setext_h1_underline` | ORGANIZATION_SECTION | NODE_TEXT | Setext H1 underline - `===` |
| `atx_h5_marker` | ORGANIZATION_SECTION | NODE_TEXT | ATX H5 marker - `#####` |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

## Other Node Types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `atx_h6_marker` | PARSER_CONSTRUCT | NODE_TEXT |  |
| `plus_metadata` | PARSER_CONSTRUCT | NODE_TEXT |  |
| `setext_h2_underline` | PARSER_CONSTRUCT | NODE_TEXT |  |

---

*Generated from `markdown_types.def`*
