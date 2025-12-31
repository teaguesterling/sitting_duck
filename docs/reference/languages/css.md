# Css Node Types

> CSS language node type mappings for AST semantic extraction

## Language Characteristics

- **Stylesheet language**: Visual presentation of web documents
- **Selectors**: Target HTML elements with specificity rules
- **Cascade**: Inheritance and specificity determine applied styles
- **At-rules**: `@media`, `@keyframes`, `@import`, `@supports`
- **Custom properties**: CSS variables with `--name` and `var()`
- **Functions**: `calc()`, `rgb()`, `url()`, `clamp()`, `min()`, `max()`
- **Units**: Absolute (`px`, `cm`) and relative (`em`, `rem`, `vh`, `vw`, `%`)
- **Media queries**: Responsive design breakpoints
- **Animations**: `@keyframes` with percentage or `from`/`to` blocks
- **Modern CSS**: Grid, Flexbox, container queries, nesting

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
- [At-Rules](#at-rules)
- [Selectors](#selectors)
- [Declarations and Properties](#declarations-and-properties)
- [Values and Literals](#values-and-literals)
- [Functions and Calculations](#functions-and-calculations)
- [Comments](#comments)
- [Media Queries](#media-queries)
- [Keyframes](#keyframes)
- [CSS Custom Properties](#css-custom-properties)
- [Layout Values](#layout-values)
- [Special Selectors](#special-selectors)
- [Punctuation and Delimiters](#punctuation-and-delimiters)
- [Selector Names](#selector-names)
- [Operators and Combinators](#operators-and-combinators)
- [At-Rule Keywords](#at-rule-keywords)
- [Units](#units)
- [Special Values](#special-values)
- [CSS Function Names](#css-function-names)
- [String and Escape Sequences](#string-and-escape-sequences)
- [Attribute Selector Components](#attribute-selector-components)
- [Media Query Operators](#media-query-operators)
- [Additional Constructs](#additional-constructs)
- [Parser Error Handling](#parser-error-handling)

## Document Structure

Top-level CSS structure

CSS document organization: - Stylesheet is the root containing all rules - Rule sets contain selectors and declaration blocks - At-rules provide special directives

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `stylesheet` | DEFINITION_MODULE | NONE | Stylesheet - root node containing all CSS rules |
| `rule_set` | DEFINITION_CLASS | NONE | Rule set - selector(s) + declaration block `selector { declarations }` |

## At-Rules

CSS at-rule directives

CSS at-rules: - `@media` - responsive breakpoints - `@import` - external stylesheet import - `@keyframes` - animation definition - `@supports` - feature detection - `@charset` - character encoding - `@namespace` - XML namespace

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `at_rule` | DEFINITION_CLASS | CUSTOM | Generic at-rule - `@rule { }` |
| `media_statement` | DEFINITION_CLASS | NODE_TEXT | Media statement - `@media screen and (min-width: 768px)` |
| `import_statement` | EXTERNAL_IMPORT | CUSTOM | Import statement - `@import url("file.css")` |
| `namespace_statement` | DEFINITION_MODULE | FIND_IDENTIFIER | Namespace statement - `@namespace svg url(...)` |
| `keyframes_statement` | DEFINITION_CLASS | FIND_IDENTIFIER | Keyframes statement - `@keyframes animation-name` |
| `supports_statement` | DEFINITION_CLASS | NODE_TEXT | Supports statement - `@supports (display: grid)` |
| `charset_statement` | METADATA_DIRECTIVE | CUSTOM | Charset statement - `@charset "UTF-8"` |

## Selectors

CSS selector constructs

CSS selector types: - `.class` - class selector - `#id` - ID selector - `element` - type/tag selector - `*` - universal selector - `[attr]` - attribute selector - `:pseudo` - pseudo-class selector - `::pseudo` - pseudo-element selector - Combinators: descendant, child (`>`), sibling (`~`), adjacent (`+`)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `selectors` | ORGANIZATION_LIST | NODE_TEXT | Selectors list - comma-separated selectors |
| `class_selector` | NAME_IDENTIFIER | NODE_TEXT | Class selector - `.classname` |
| `id_selector` | NAME_IDENTIFIER | NODE_TEXT | ID selector - `#identifier` |
| `tag_name` | NAME_IDENTIFIER | NODE_TEXT | Tag/type selector - `div`, `p`, `span` |
| `universal_selector` | NAME_IDENTIFIER | NODE_TEXT | Universal selector - `*` |
| `attribute_selector` | NAME_QUALIFIED | NODE_TEXT | Attribute selector - `[attr="value"]` |
| `pseudo_class_selector` | NAME_IDENTIFIER | NODE_TEXT | Pseudo-class selector - `:hover`, `:nth-child()` |
| `pseudo_element_selector` | NAME_IDENTIFIER | NODE_TEXT | Pseudo-element selector - `::before`, `::after` |
| `descendant_selector` | OPERATOR_LOGICAL | NODE_TEXT | Descendant selector - `ancestor descendant` |
| `child_selector` | OPERATOR_LOGICAL | NODE_TEXT | Child selector - `parent > child` |
| `sibling_selector` | OPERATOR_LOGICAL | NODE_TEXT | General sibling selector - `element ~ sibling` |
| `adjacent_sibling_selector` | OPERATOR_LOGICAL | NODE_TEXT | Adjacent sibling selector - `element + sibling` |

## Declarations and Properties

CSS property declarations

CSS declaration structure: - Declaration: `property: value;` - Block: `{ declarations }` - Important: `!important` modifier

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | ORGANIZATION_BLOCK | NONE | Declaration block - `{ property: value; ... }` |
| `declaration` | DEFINITION_VARIABLE | CUSTOM | Declaration - `property: value` |
| `property_name` | NAME_IDENTIFIER | NODE_TEXT | Property name - `color`, `margin`, `display` |
| `important` | METADATA_ANNOTATION | NODE_TEXT | Important marker - `!important` |

## Values and Literals

CSS value types

CSS value types: - Numbers: `42`, `3.14` - Dimensions: `16px`, `1.5em`, `100%` - Colors: `#fff`, `rgb()`, `hsl()` - Strings: `"content"`, `'font-family'` - Keywords: `auto`, `inherit`, `none`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `integer_value` | LITERAL_NUMBER | NODE_TEXT | Integer value - `42` |
| `float_value` | LITERAL_NUMBER | NODE_TEXT | Float value - `3.14` |
| `unit` | METADATA_ANNOTATION | NODE_TEXT | Unit - `px`, `em`, `%`, etc. |
| `string_value` | LITERAL_STRING | NODE_TEXT | String value - `"text"` |
| `color_value` | LITERAL_ATOMIC | NODE_TEXT | Color value - `#rgb`, `#rrggbb` |
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - generic name token |
| `plain_value` | LITERAL_ATOMIC | NODE_TEXT | Plain value - unquoted keyword value |
| `binary_expression` | OPERATOR_ARITHMETIC | NONE | Binary expression - `calc(100% - 20px)` |
| `parenthesized_value` | ORGANIZATION_CONTAINER | NONE | Parenthesized value - `(value)` |

## Functions and Calculations

CSS function calls

CSS functions: - `calc()` - mathematical calculations - `var()` - custom property reference - `url()` - resource reference - `rgb()`, `rgba()`, `hsl()`, `hsla()` - color functions - `min()`, `max()`, `clamp()` - comparison functions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | CUSTOM | Function call - `function(args)` |
| `function_name` | NAME_IDENTIFIER | NODE_TEXT | Function name - `calc`, `rgb`, `var` |
| `arguments` | ORGANIZATION_LIST | NONE | Function arguments - argument list |

## Comments

CSS comment blocks

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment - `/* comment */` |

## Media Queries

Media query components

Media query features: - `min-width`, `max-width` - viewport dimensions - `orientation` - portrait/landscape - `prefers-color-scheme` - light/dark mode

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `media_feature_name` | NAME_IDENTIFIER | NODE_TEXT | Media feature name - `min-width`, `orientation` |
| `feature_name` | NAME_IDENTIFIER | NODE_TEXT | Feature name - generic feature identifier |

## Keyframes

Animation keyframe constructs

Keyframe syntax: - `@keyframes name { 0% { } 100% { } }` - `from` = 0%, `to` = 100%

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `keyframe_block_list` | ORGANIZATION_LIST | NONE | Keyframe block list - contains keyframe blocks |
| `keyframe_block` | ORGANIZATION_BLOCK | CUSTOM | Keyframe block - single animation frame `50% { }` |

## CSS Custom Properties

CSS variables and custom properties

Custom property syntax: - Definition: `--custom-property: value;` - Usage: `var(--custom-property)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_name` | NAME_IDENTIFIER | NODE_TEXT | Variable name - `--custom-property` |
| `postcss_statement` | METADATA_DIRECTIVE | NODE_TEXT | PostCSS statement - preprocessor directive |

## Layout Values

Grid and flexbox specific values

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `grid_value` | LITERAL_STRUCTURED | NODE_TEXT | Grid value - grid-specific syntax |

## Special Selectors

Modern CSS selector features

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `nesting_selector` | NAME_IDENTIFIER | NODE_TEXT | Nesting selector - `&` in nested CSS |
| `namespace_name` | NAME_IDENTIFIER | NODE_TEXT | Namespace name - XML namespace prefix |

## Punctuation and Delimiters

CSS syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `:` | PARSER_PUNCTUATION | NONE | Colon - property separator `:` |
| `;` | PARSER_PUNCTUATION | NONE | Semicolon - declaration terminator `;` |
| `.` | PARSER_PUNCTUATION | NONE | Period - class selector prefix `.` |
| `,` | PARSER_PUNCTUATION | NONE | Comma - selector/value separator `,` |
| `{` | PARSER_DELIMITER | NONE | Opening brace - block start `{` |
| `}` | PARSER_DELIMITER | NONE | Closing brace - block end `}` |
| `(` | PARSER_DELIMITER | NONE | Opening parenthesis - function call `(` |
| `)` | PARSER_DELIMITER | NONE | Closing parenthesis - function call `)` |
| `#` | PARSER_PUNCTUATION | NONE | Hash - ID selector prefix `#` |
| `>` | OPERATOR_LOGICAL | NONE | Greater than - child combinator `>` |
| `::` | PARSER_PUNCTUATION | NONE | Double colon - pseudo-element prefix `::` |
| `'` | PARSER_DELIMITER | NONE | Single quote - string delimiter `'` |

## Selector Names

CSS selector name tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_name` | NAME_IDENTIFIER | NODE_TEXT | Class name - name portion of `.classname` |
| `string_content` | LITERAL_STRING | NONE | String content - text inside quotes |

## Operators and Combinators

CSS operator tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_LOGICAL | NONE | Plus - adjacent sibling combinator `+` |
| `~` | OPERATOR_LOGICAL | NONE | Tilde - general sibling combinator `~` |
| `*` | OPERATOR_LOGICAL | NONE | Asterisk - universal selector `*` |

## At-Rule Keywords

CSS at-rule keyword tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `@media` | METADATA_DIRECTIVE | NODE_TEXT | @media keyword |
| `@import` | METADATA_DIRECTIVE | NODE_TEXT | @import keyword |
| `@keyframes` | METADATA_DIRECTIVE | NODE_TEXT | @keyframes keyword |
| `@supports` | METADATA_DIRECTIVE | NODE_TEXT | @supports keyword |
| `@charset` | METADATA_DIRECTIVE | NODE_TEXT | @charset keyword |
| `@namespace` | METADATA_DIRECTIVE | NODE_TEXT | @namespace keyword |

## Units

CSS measurement unit tokens

Unit categories: - Absolute: `px`, `cm`, `mm`, `in`, `pt`, `pc` - Relative: `em`, `rem`, `%` - Viewport: `vh`, `vw`, `vmin`, `vmax`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `px` | METADATA_ANNOTATION | NODE_TEXT | Pixels unit - `px` |
| `rem` | METADATA_ANNOTATION | NODE_TEXT | Root em unit - `rem` |
| `em` | METADATA_ANNOTATION | NODE_TEXT | Em unit - `em` |
| `%` | METADATA_ANNOTATION | NODE_TEXT | Percentage unit - `%` |
| `vh` | METADATA_ANNOTATION | NODE_TEXT | Viewport height unit - `vh` |
| `vw` | METADATA_ANNOTATION | NODE_TEXT | Viewport width unit - `vw` |

## Special Values

CSS keyword values

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `!important` | METADATA_ANNOTATION | NODE_TEXT | Important declaration - `!important` |
| `inherit` | LITERAL_ATOMIC | NODE_TEXT | Inherit value - inherit from parent |
| `initial` | LITERAL_ATOMIC | NODE_TEXT | Initial value - use initial value |
| `unset` | LITERAL_ATOMIC | NODE_TEXT | Unset value - reset to natural value |
| `none` | LITERAL_ATOMIC | NODE_TEXT | None value - disable property |
| `auto` | LITERAL_ATOMIC | NODE_TEXT | Auto value - automatic calculation |

## CSS Function Names

Built-in CSS function keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `var` | COMPUTATION_CALL | NODE_TEXT | var() function - custom property reference |
| `calc` | COMPUTATION_CALL | NODE_TEXT | calc() function - mathematical calculation |
| `url` | COMPUTATION_CALL | NODE_TEXT | url() function - resource reference |
| `rgb` | COMPUTATION_CALL | NODE_TEXT | rgb() function - RGB color |
| `rgba` | COMPUTATION_CALL | NODE_TEXT | rgba() function - RGB color with alpha |
| `hsl` | COMPUTATION_CALL | NODE_TEXT | hsl() function - HSL color |
| `hsla` | COMPUTATION_CALL | NODE_TEXT | hsla() function - HSL color with alpha |

## String and Escape Sequences

CSS string-related tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `escape_sequence` | LITERAL_STRING | NONE | Escape sequence - `\n`, `\u0041` |

## Attribute Selector Components

Attribute selector parts

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `attribute_name` | NAME_IDENTIFIER | NODE_TEXT | Attribute name - name in `[attr]` |
| `[` | PARSER_DELIMITER | NONE | Opening bracket - `[` |
| `]` | PARSER_DELIMITER | NONE | Closing bracket - `]` |
| `feature_query` | ORGANIZATION_BLOCK | NONE | Feature query - `@supports` condition |
| `=` | OPERATOR_COMPARISON | NONE | Equals operator - `=` |
| `id_name` | NAME_IDENTIFIER | NODE_TEXT | ID name - name portion of `#idname` |

## Media Query Operators

Media query logical operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `and` | OPERATOR_LOGICAL | NONE | And operator - `and` in media queries |
| `binary_query` | OPERATOR_LOGICAL | NONE | Binary query - combined media conditions |

## Additional Constructs

Remaining CSS constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `keyword_query` | METADATA_DIRECTIVE | NODE_TEXT | Keyword query - media type query |
| `unary_query` | OPERATOR_LOGICAL | NONE | Unary query - negated media query |
| `only` | OPERATOR_LOGICAL | NODE_TEXT | Only keyword - media query modifier |
| `keyframes_name` | NAME_IDENTIFIER | NODE_TEXT | Keyframes name - animation identifier |
| `*=` | OPERATOR_ASSIGNMENT | NONE | Substring match operator - `*=` |
| `at_keyword` | METADATA_DIRECTIVE | NODE_TEXT | At keyword - generic at-rule identifier |
| `-` | OPERATOR_ARITHMETIC | NONE | Minus operator - subtraction `-` |
| `/` | OPERATOR_ARITHMETIC | NONE | Division operator - `/` |
| `to` | LITERAL_ATOMIC | NODE_TEXT | To keyword - keyframe end `to` |
| `from` | LITERAL_ATOMIC | NODE_TEXT | From keyword - keyframe start `from` |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `css_types.def`*
