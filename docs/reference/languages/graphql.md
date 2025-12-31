# Graphql Node Types

> GraphQL language node type mappings for AST semantic extraction

## Language Characteristics

- **Query language**: API query and schema definition
- **Operations**: `query`, `mutation`, `subscription`
- **Type system**: Strong typing with object, interface, union, enum, scalar
- **Fragments**: Reusable field selections
- **Directives**: `@directive(arg: value)` metadata
- **Variables**: `$variableName: Type` parameters
- **Arguments**: Named arguments on fields and directives
- **Selection sets**: `{ field1 field2 }` field requests
- **Descriptions**: Documentation strings (triple-quoted)
- **Introspection**: Schema self-documentation

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
- [Schema Definition](#schema-definition)
- [Type Definitions](#type-definitions)
- [Type Extensions](#type-extensions)
- [Operations](#operations)
- [Fragments](#fragments)
- [Fields](#fields)
- [Enum Definitions](#enum-definitions)
- [Union and Interface](#union-and-interface)
- [Input Fields](#input-fields)
- [Arguments](#arguments)
- [Variables](#variables)
- [Directives](#directives)
- [Type References](#type-references)
- [Values](#values)
- [Names](#names)
- [Documentation](#documentation)
- [Punctuation](#punctuation)
- [Keywords](#keywords)
- [Parser Error Handling](#parser-error-handling)

## Document Structure

Top-level GraphQL document

GraphQL documents contain: - Executable definitions (operations, fragments) - Type system definitions (schema, types) - Type system extensions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `source_file` | DEFINITION_MODULE | NONE | Source file - root GraphQL file |
| `document` | ORGANIZATION_CONTAINER | NONE | Document - GraphQL document container |
| `definition` | ORGANIZATION_BLOCK | NONE | Definition - generic definition container |
| `executable_definition` | ORGANIZATION_BLOCK | NONE | Executable definition - operation or fragment |
| `type_system_definition` | ORGANIZATION_BLOCK | NONE | Type system definition - schema or type |
| `type_system_extension` | ORGANIZATION_BLOCK | NONE | Type system extension - extends existing types |

## Schema Definition

GraphQL schema constructs

Schema syntax: - `schema { query: Query, mutation: Mutation }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `schema_definition` | DEFINITION_MODULE | NONE | Schema definition - `schema { }` |
| `schema_extension` | DEFINITION_MODULE | NONE | Schema extension - `extend schema { }` |
| `root_operation_type_definition` | DEFINITION_VARIABLE | NONE | Root operation type - `query: Query` |

## Type Definitions

GraphQL type system

Type kinds: - Object: `type Name { fields }` - Interface: `interface Name { fields }` - Union: `union Name = Type1 | Type2` - Enum: `enum Name { VALUE1 VALUE2 }` - Input: `input Name { fields }` - Scalar: `scalar Name`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_definition` | ORGANIZATION_BLOCK | NONE | Type definition container |
| `object_type_definition` | DEFINITION_CLASS | CUSTOM | Object type - `type Name { }` |
| `interface_type_definition` | DEFINITION_CLASS | CUSTOM | Interface type - `interface Name { }` |
| `union_type_definition` | DEFINITION_CLASS | CUSTOM | Union type - `union Name = A | B` |
| `enum_type_definition` | DEFINITION_CLASS | CUSTOM | Enum type - `enum Name { }` |
| `input_object_type_definition` | DEFINITION_CLASS | CUSTOM | Input object type - `input Name { }` |
| `scalar_type_definition` | DEFINITION_CLASS | CUSTOM | Scalar type - `scalar Name` |

## Type Extensions

GraphQL type extensions

Extension syntax: - `extend type Name { newFields }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_extension` | ORGANIZATION_BLOCK | NONE | Type extension container |
| `object_type_extension` | DEFINITION_CLASS | CUSTOM | Object type extension |
| `interface_type_extension` | DEFINITION_CLASS | CUSTOM | Interface type extension |
| `union_type_extension` | DEFINITION_CLASS | CUSTOM | Union type extension |
| `enum_type_extension` | DEFINITION_CLASS | CUSTOM | Enum type extension |
| `input_object_type_extension` | DEFINITION_CLASS | CUSTOM | Input object type extension |
| `scalar_type_extension` | DEFINITION_CLASS | CUSTOM | Scalar type extension |

## Operations

GraphQL executable operations

Operation types: - `query` - read operations - `mutation` - write operations - `subscription` - real-time updates

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `operation_definition` | DEFINITION_FUNCTION | CUSTOM | Operation definition - `query Name { }` |
| `operation_type` | NAME_IDENTIFIER | NODE_TEXT | Operation type - `query`, `mutation`, `subscription` |

## Fragments

GraphQL fragment definitions

Fragment syntax: - Definition: `fragment Name on Type { fields }` - Spread: `...FragmentName` - Inline: `... on Type { fields }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `fragment_definition` | DEFINITION_FUNCTION | CUSTOM | Fragment definition - `fragment Name on Type { }` |
| `fragment_spread` | COMPUTATION_CALL | CUSTOM | Fragment spread - `...FragmentName` |
| `fragment_name` | NAME_IDENTIFIER | NODE_TEXT | Fragment name - identifier |
| `inline_fragment` | ORGANIZATION_BLOCK | NONE | Inline fragment - `... on Type { }` |
| `type_condition` | TYPE_REFERENCE | CUSTOM | Type condition - `on TypeName` |

## Fields

GraphQL field definitions and selections

Field syntax: - Definition: `fieldName: Type` - Selection: `fieldName` or `alias: fieldName(args)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `fields_definition` | ORGANIZATION_LIST | NONE | Fields definition - field list in type |
| `field_definition` | DEFINITION_VARIABLE | CUSTOM | Field definition - `fieldName: Type` |
| `field` | COMPUTATION_ACCESS | CUSTOM | Field selection - field in query |
| `alias` | NAME_IDENTIFIER | CUSTOM | Alias - `aliasName: fieldName` |
| `selection_set` | ORGANIZATION_BLOCK | NONE | Selection set - `{ field1 field2 }` |
| `selection` | ORGANIZATION_BLOCK | NONE | Selection - single selection |

## Enum Definitions

GraphQL enum constructs

Enum syntax: - `enum Status { ACTIVE INACTIVE }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `enum_values_definition` | ORGANIZATION_LIST | NONE | Enum values definition - value list |
| `enum_value_definition` | DEFINITION_VARIABLE | CUSTOM | Enum value definition - single value |
| `enum_value` | NAME_IDENTIFIER | NODE_TEXT | Enum value - value reference |

## Union and Interface

GraphQL union and interface constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `union_member_types` | ORGANIZATION_LIST | NONE | Union member types - `= Type1 | Type2` |
| `implements_interfaces` | ORGANIZATION_LIST | NONE | Implements interfaces - `implements A & B` |

## Input Fields

GraphQL input object fields

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `input_fields_definition` | ORGANIZATION_LIST | NONE | Input fields definition - input type fields |
| `input_value_definition` | DEFINITION_VARIABLE | CUSTOM | Input value definition - input field |

## Arguments

GraphQL argument constructs

Argument syntax: - Definition: `(argName: Type = default)` - Usage: `(argName: value)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `arguments_definition` | ORGANIZATION_LIST | NONE | Arguments definition - parameter list |
| `arguments` | ORGANIZATION_LIST | NONE | Arguments - argument list at call site |
| `argument` | DEFINITION_VARIABLE | CUSTOM | Argument - single argument |

## Variables

GraphQL variable constructs

Variable syntax: - Definition: `($varName: Type = default)` - Usage: `$varName`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_definitions` | ORGANIZATION_LIST | NONE | Variable definitions - operation parameters |
| `variable_definition` | DEFINITION_VARIABLE | CUSTOM | Variable definition - `$name: Type` |
| `variable` | NAME_IDENTIFIER | CUSTOM | Variable reference - `$name` |
| `default_value` | LITERAL_ATOMIC | NONE | Default value - `= value` |

## Directives

GraphQL directive constructs

Directive syntax: - Definition: `directive @name on LOCATION` - Usage: `@directive(arg: value)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `directive_definition` | DEFINITION_FUNCTION | CUSTOM | Directive definition - `directive @name` |
| `directives` | ORGANIZATION_LIST | NONE | Directives list - applied directives |
| `directive` | METADATA_ANNOTATION | CUSTOM | Directive usage - `@name(args)` |
| `directive_locations` | ORGANIZATION_LIST | NONE | Directive locations - valid locations |
| `directive_location` | NAME_IDENTIFIER | NODE_TEXT | Directive location - single location |
| `executable_directive_location` | NAME_IDENTIFIER | NODE_TEXT | Executable directive location - operation locations |
| `type_system_directive_location` | NAME_IDENTIFIER | NODE_TEXT | Type system directive location - schema locations |

## Type References

GraphQL type reference constructs

Type syntax: - Named: `TypeName` - List: `[TypeName]` - Non-null: `TypeName!`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type` | TYPE_REFERENCE | NONE | Type reference container |
| `named_type` | TYPE_PRIMITIVE | CUSTOM | Named type - `TypeName` |
| `list_type` | TYPE_COMPOSITE | NONE | List type - `[TypeName]` |
| `non_null_type` | TYPE_REFERENCE | NONE | Non-null type - `TypeName!` |

## Values

GraphQL literal values

Value types: - String, Int, Float - Boolean (`true`, `false`) - Null - Enum values - Lists, Objects

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `value` | LITERAL_ATOMIC | NONE | Value container |
| `string_value` | LITERAL_STRING | NODE_TEXT | String value - `"text"` |
| `int_value` | LITERAL_NUMBER | NODE_TEXT | Integer value - `42` |
| `float_value` | LITERAL_NUMBER | NODE_TEXT | Float value - `3.14` |
| `boolean_value` | LITERAL_ATOMIC | NODE_TEXT | Boolean value - `true`, `false` |
| `null_value` | LITERAL_ATOMIC | NODE_TEXT | Null value - `null` |
| `list_value` | LITERAL_STRUCTURED | NONE | List value - `[1, 2, 3]` |
| `object_value` | LITERAL_STRUCTURED | NONE | Object value - `{key: value}` |
| `object_field` | DEFINITION_VARIABLE | CUSTOM | Object field - `key: value` in object |

## Names

GraphQL identifiers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `name` | NAME_IDENTIFIER | NODE_TEXT | Name - identifier token |

## Documentation

GraphQL documentation constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `description` | METADATA_COMMENT | NONE | Description - triple-quoted doc string |
| `comment` | METADATA_COMMENT | NONE | Comment - `# comment` |

## Punctuation

GraphQL syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `!` | PARSER_PUNCTUATION | NODE_TEXT | Exclamation - non-null `!` |
| `$` | PARSER_PUNCTUATION | NODE_TEXT | Dollar - variable prefix `$` |
| `@` | PARSER_PUNCTUATION | NODE_TEXT | At sign - directive prefix `@` |
| `&` | PARSER_PUNCTUATION | NODE_TEXT | Ampersand - interface separator `&` |
| `|` | PARSER_PUNCTUATION | NODE_TEXT | Pipe - union separator `|` |
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Equals - default value `=` |
| `:` | PARSER_PUNCTUATION | NODE_TEXT | Colon - type separator `:` |
| `...` | PARSER_PUNCTUATION | NODE_TEXT | Spread - `...` |
| `(` | PARSER_DELIMITER | NODE_TEXT | Opening parenthesis - `(` |
| `)` | PARSER_DELIMITER | NODE_TEXT | Closing parenthesis - `)` |
| `[` | PARSER_DELIMITER | NODE_TEXT | Opening bracket - `[` |
| `]` | PARSER_DELIMITER | NODE_TEXT | Closing bracket - `]` |
| `{` | PARSER_DELIMITER | NODE_TEXT | Opening brace - `{` |
| `}` | PARSER_DELIMITER | NODE_TEXT | Closing brace - `}` |
| `,` | PARSER_PUNCTUATION | NODE_TEXT | Comma - `,` |

## Keywords

GraphQL reserved keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `query` | DEFINITION_FUNCTION | NODE_TEXT | Query keyword |
| `mutation` | DEFINITION_FUNCTION | NODE_TEXT | Mutation keyword |
| `subscription` | DEFINITION_FUNCTION | NODE_TEXT | Subscription keyword |
| `fragment` | DEFINITION_FUNCTION | NODE_TEXT | Fragment keyword |
| `on` | PARSER_SYNTAX | NODE_TEXT | On keyword - type condition |
| `type` | DEFINITION_CLASS | NODE_TEXT | Type keyword |
| `interface` | DEFINITION_CLASS | NODE_TEXT | Interface keyword |
| `union` | DEFINITION_CLASS | NODE_TEXT | Union keyword |
| `enum` | DEFINITION_CLASS | NODE_TEXT | Enum keyword |
| `input` | DEFINITION_CLASS | NODE_TEXT | Input keyword |
| `scalar` | DEFINITION_CLASS | NODE_TEXT | Scalar keyword |
| `schema` | DEFINITION_MODULE | NODE_TEXT | Schema keyword |
| `extend` | PARSER_SYNTAX | NODE_TEXT | Extend keyword |
| `directive` | METADATA_ANNOTATION | NODE_TEXT | Directive keyword |
| `implements` | PARSER_SYNTAX | NODE_TEXT | Implements keyword |
| `repeatable` | PARSER_SYNTAX | NODE_TEXT | Repeatable keyword |
| `true` | LITERAL_ATOMIC | NODE_TEXT | True keyword |
| `false` | LITERAL_ATOMIC | NODE_TEXT | False keyword |
| `null` | LITERAL_ATOMIC | NODE_TEXT | Null keyword |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `graphql_types.def`*
