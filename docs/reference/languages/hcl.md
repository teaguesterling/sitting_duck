# Hcl Node Types

> HCL (HashiCorp Configuration Language) node type mappings for AST semantic extraction

## Language Characteristics

- **Infrastructure as Code**: Declarative configuration language
- **Blocks**: Named blocks with labels `resource "type" "name" { }`
- **Attributes**: Key-value assignments `name = value`
- **Expressions**: Rich expression syntax with functions
- **Variables**: `var.name` references
- **String templates**: `${expression}` interpolation
- **For expressions**: List/map comprehensions
- **Splat expressions**: `[*]` and `.*` syntax
- **Conditionals**: `condition ? true : false`
- **Heredocs**: Multi-line strings with `<<EOF`

Used by:
- Terraform (infrastructure)
- Vault (secrets)
- Nomad (orchestration)
- Waypoint (deployment)

## Semantic Type Encoding

Semantic types use an 8-bit encoding:
- Bits 7-2: Base semantic category (e.g., DEFINITION_CLASS = 0x08)
- Bits 1-0: Refinement within category

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
- [Block Definitions](#block-definitions)
- [Attributes](#attributes)
- [Expressions](#expressions)
- [Variable and Function References](#variable-and-function-references)
- [Identifiers](#identifiers)
- [Literals](#literals)
- [Collection Types](#collection-types)
- [For Expressions](#for-expressions)
- [Splat Expressions](#splat-expressions)
- [Template Expressions](#template-expressions)
- [Template Directives](#template-directives)
- [Heredocs](#heredocs)
- [Comments](#comments)
- [Arithmetic Operators](#arithmetic-operators)
- [Comparison Operators](#comparison-operators)
- [Logical Operators](#logical-operators)
- [Assignment Operators](#assignment-operators)
- [Punctuation](#punctuation)
- [Keywords](#keywords)
- [Parser Error Handling](#parser-error-handling)

## Document Structure

Top-level HCL structure

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `config_file` | DEFINITION_MODULE | NONE | Config file - root HCL container |
| `body` | ORGANIZATION_BLOCK | NONE | Body - content container within blocks |

## Block Definitions

HCL block constructs

Block syntax: - `resource "type" "name" { body }` - `variable "name" { }` - `output "name" { }` - `provider "name" { }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | DEFINITION_CLASS | CUSTOM | Block - named block `type "label" { }` |
| `block_start` | PARSER_DELIMITER | NONE | Block start - opening delimiter |
| `block_end` | PARSER_DELIMITER | NONE | Block end - closing delimiter |

## Attributes

HCL key-value assignments

Attribute syntax: - `name = value` - `name = expression`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `attribute` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Attribute - `name = value` |

## Expressions

HCL expression constructs

Expression types: - Literals, references, function calls - Binary and unary operations - Conditionals: `cond ? a : b`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `expression` | ORGANIZATION_BLOCK | NONE | Expression container |
| `conditional` | FLOW_CONDITIONAL | NONE | Conditional - `cond ? true_val : false_val` |
| `operation` | OPERATOR_ARITHMETIC | NONE | Generic operation |
| `binary_operation` | OPERATOR_ARITHMETIC | NONE | Binary operation - `a + b`, `a && b` |
| `unary_operation` | OPERATOR_ARITHMETIC | NONE | Unary operation - `!a`, `-a` |

## Variable and Function References

HCL reference constructs

Reference types: - Variables: `var.name` - Functions: `function(args)` - Attributes: `resource.name.attr` - Indexes: `list[0]`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_expr` | NAME_IDENTIFIER | FIND_IDENTIFIER | Variable expression - `var.name` |
| `function_call` | COMPUTATION_CALL | FIND_CALL_TARGET | Function call - `function(args)` |
| `function_arguments` | ORGANIZATION_LIST | NONE | Function arguments - argument list |
| `get_attr` | COMPUTATION_ACCESS | FIND_IDENTIFIER | Get attribute - `.attribute` |
| `index` | COMPUTATION_ACCESS | NONE | Index access - `[index]` |
| `new_index` | COMPUTATION_ACCESS | NONE | New-style index - modern syntax |
| `legacy_index` | COMPUTATION_ACCESS | NONE | Legacy index - older syntax |

## Identifiers

HCL name tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - name token |

## Literals

HCL literal values

Literal types: - Numbers: `42`, `3.14` - Strings: `"text"` - Booleans: `true`, `false` - Null: `null`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `literal_value` | LITERAL_ATOMIC | NONE | Literal value container |
| `numeric_lit` | LITERAL_NUMBER | NODE_TEXT | Numeric literal - `42`, `3.14` |
| `string_lit` | LITERAL_STRING | NONE | String literal - `"text"` |
| `bool_lit` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true`, `false` |
| `null_lit` | LITERAL_ATOMIC | NODE_TEXT | Null literal - `null` |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean true - `true` |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean false - `false` |

## Collection Types

HCL collection values

Collection types: - Objects: `{ key = value }` - Tuples: `[value1, value2]`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `collection_value` | LITERAL_STRUCTURED | NONE | Collection value container |
| `object` | LITERAL_STRUCTURED | NONE | Object - `{ key = value }` |
| `object_elem` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Object element - `key = value` |
| `object_start` | PARSER_DELIMITER | NONE | Object start - `{` |
| `object_end` | PARSER_DELIMITER | NONE | Object end - `}` |
| `tuple` | LITERAL_STRUCTURED | NONE | Tuple - `[value, ...]` |
| `tuple_start` | PARSER_DELIMITER | NONE | Tuple start - `[` |
| `tuple_end` | PARSER_DELIMITER | NONE | Tuple end - `]` |

## For Expressions

HCL comprehension syntax

For expression syntax: - Tuple: `[for x in list : x * 2]` - Object: `{for k, v in map : k => v}` - With filter: `[for x in list : x if x > 0]`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_expr` | FLOW_LOOP | NONE | For expression container |
| `for_tuple_expr` | FLOW_LOOP | NONE | For tuple expression - `[for ...]` |
| `for_object_expr` | FLOW_LOOP | NONE | For object expression - `{for ...}` |
| `for_intro` | FLOW_LOOP | NONE | For intro - `for x in list` |
| `for_cond` | FLOW_CONDITIONAL | NONE | For condition - `if condition` |

## Splat Expressions

HCL splat syntax

Splat types: - Attribute: `list.*.attr` - Full: `list[*].attr`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `splat` | COMPUTATION_ACCESS | NONE | Splat expression container |
| `attr_splat` | COMPUTATION_ACCESS | NONE | Attribute splat - `.*` |
| `full_splat` | COMPUTATION_ACCESS | NONE | Full splat - `[*]` |
| `ellipsis` | PARSER_SYNTAX | NONE | Ellipsis - `...` |

## Template Expressions

HCL string interpolation

Template syntax: - Interpolation: `${expression}` - Directive: `%{if cond}...%{endif}`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `template_expr` | LITERAL_STRING | NONE | Template expression container |
| `quoted_template` | LITERAL_STRING | NONE | Quoted template - `"...${...}..."` |
| `quoted_template_start` | PARSER_DELIMITER | NONE | Quoted template start - opening `"` |
| `quoted_template_end` | PARSER_DELIMITER | NONE | Quoted template end - closing `"` |
| `template_literal` | LITERAL_STRING | NODE_TEXT | Template literal - static text |
| `template_interpolation` | COMPUTATION_CALL | NONE | Template interpolation - `${expr}` |
| `template_interpolation_start` | PARSER_DELIMITER | NONE | Interpolation start - `${` |
| `template_interpolation_end` | PARSER_DELIMITER | NONE | Interpolation end - `}` |

## Template Directives

HCL template control flow

Directive syntax: - If: `%{if cond}...%{else}...%{endif}` - For: `%{for x in list}...%{endfor}`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `template_directive` | FLOW_CONDITIONAL | NONE | Template directive container |
| `template_directive_start` | PARSER_DELIMITER | NONE | Template directive start - `%{` |
| `template_directive_end` | PARSER_DELIMITER | NONE | Template directive end - `}` |
| `template_if` | FLOW_CONDITIONAL | NONE | Template if block |
| `template_if_intro` | FLOW_CONDITIONAL | NONE | Template if intro - `%{if cond}` |
| `template_if_end` | FLOW_CONDITIONAL | NONE | Template if end - `%{endif}` |
| `template_else_intro` | FLOW_CONDITIONAL | NONE | Template else - `%{else}` |
| `template_for` | FLOW_LOOP | NONE | Template for block |
| `template_for_start` | FLOW_LOOP | NONE | Template for start - `%{for ...}` |
| `template_for_end` | FLOW_LOOP | NONE | Template for end - `%{endfor}` |
| `strip_marker` | PARSER_SYNTAX | NONE | Strip marker - `~` |

## Heredocs

HCL heredoc strings

Heredoc syntax: - Standard: `<<EOF ... EOF` - Indented: `<<-EOF ... EOF`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `heredoc_template` | LITERAL_STRING | NONE | Heredoc template - `<<EOF ... EOF` |
| `heredoc_start` | PARSER_DELIMITER | NONE | Heredoc start - `<<` or `<<-` |
| `heredoc_identifier` | NAME_IDENTIFIER | NODE_TEXT | Heredoc identifier - terminator name |

## Comments

HCL comment syntax

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment |

## Arithmetic Operators

HCL arithmetic operator tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_ARITHMETIC | NODE_TEXT | Plus - `+` |
| `-` | OPERATOR_ARITHMETIC | NODE_TEXT | Minus - `-` |
| `*` | OPERATOR_ARITHMETIC | NODE_TEXT | Multiply - `*` |
| `/` | OPERATOR_ARITHMETIC | NODE_TEXT | Divide - `/` |
| `%` | OPERATOR_ARITHMETIC | NODE_TEXT | Modulo - `%` |

## Comparison Operators

HCL comparison operator tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `==` | OPERATOR_COMPARISON | NODE_TEXT | Equals - `==` |
| `!=` | OPERATOR_COMPARISON | NODE_TEXT | Not equals - `!=` |
| `<` | OPERATOR_COMPARISON | NODE_TEXT | Less than - `<` |
| `>` | OPERATOR_COMPARISON | NODE_TEXT | Greater than - `>` |
| `<=` | OPERATOR_COMPARISON | NODE_TEXT | Less than or equal - `<=` |
| `>=` | OPERATOR_COMPARISON | NODE_TEXT | Greater than or equal - `>=` |

## Logical Operators

HCL logical operator tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `&&` | OPERATOR_LOGICAL | NODE_TEXT | Logical and - `&&` |
| `||` | OPERATOR_LOGICAL | NODE_TEXT | Logical or - `||` |
| `!` | OPERATOR_LOGICAL | NODE_TEXT | Logical not - `!` |

## Assignment Operators

HCL assignment operator tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Assignment - `=` |
| `=>` | OPERATOR_ASSIGNMENT | NODE_TEXT | Fat arrow - `=>` |

## Punctuation

HCL syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NODE_TEXT | Opening parenthesis - `(` |
| `)` | PARSER_DELIMITER | NODE_TEXT | Closing parenthesis - `)` |
| `[` | PARSER_DELIMITER | NODE_TEXT | Opening bracket - `[` |
| `]` | PARSER_DELIMITER | NODE_TEXT | Closing bracket - `]` |
| `{` | PARSER_DELIMITER | NODE_TEXT | Opening brace - `{` |
| `}` | PARSER_DELIMITER | NODE_TEXT | Closing brace - `}` |
| `,` | PARSER_PUNCTUATION | NODE_TEXT | Comma - `,` |
| `:` | PARSER_PUNCTUATION | NODE_TEXT | Colon - `:` |
| `.` | PARSER_PUNCTUATION | NODE_TEXT | Period - `.` |
| `?` | PARSER_PUNCTUATION | NODE_TEXT | Question mark - `?` |
| `.*` | PARSER_PUNCTUATION | NODE_TEXT | Attribute splat - `.*` |
| `[*]` | PARSER_PUNCTUATION | NODE_TEXT | Full splat - `[*]` |
| `<<` | PARSER_DELIMITER | NODE_TEXT | Heredoc standard - `<<` |
| `<<-` | PARSER_DELIMITER | NODE_TEXT | Heredoc indented - `<<-` |

## Keywords

HCL reserved keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for` | FLOW_LOOP | NODE_TEXT | For keyword - `for` |
| `in` | FLOW_LOOP | NODE_TEXT | In keyword - `in` |
| `if` | FLOW_CONDITIONAL | NODE_TEXT | If keyword - `if` |
| `else` | FLOW_CONDITIONAL | NODE_TEXT | Else keyword - `else` |
| `endif` | FLOW_CONDITIONAL | NODE_TEXT | Endif keyword - `endif` |
| `endfor` | FLOW_LOOP | NODE_TEXT | Endfor keyword - `endfor` |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `hcl_types.def`*
