# Lua Node Types

> Lua language node type mappings for AST semantic extraction

## Language Characteristics

- **Lightweight scripting**: Designed for embedding in applications
- **Tables**: Single data structure for arrays, objects, modules
- **First-class functions**: Functions are values, closures supported
- **Coroutines**: Cooperative multitasking via coroutine library
- **Dynamic typing**: All values carry their type at runtime
- **Metatables**: Operator overloading and prototype-based OOP
- **Multiple return values**: Functions can return multiple values
- **Varargs**: `...` for variadic function parameters
- **Lexical scoping**: `local` keyword for local variables

## Semantic Type Encoding

Semantic types use an 8-bit encoding:
- Bits 7-2: Base semantic category (e.g., DEFINITION_FUNCTION = 0x04)
- Bits 1-0: Refinement within category (e.g., Function::REGULAR = 0x00)

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

- [Program Structure](#program-structure)
- [Function Definitions](#function-definitions)
- [Function Calls](#function-calls)
- [Variable Declarations](#variable-declarations)
- [Table Constructs](#table-constructs)
- [Identifiers and References](#identifiers-and-references)
- [Literals](#literals)
- [Control Flow - Conditionals](#control-flow---conditionals)
- [Control Flow - Loops](#control-flow---loops)
- [Jump Statements](#jump-statements)
- [Expressions](#expressions)
- [Comments and Metadata](#comments-and-metadata)
- [Operators](#operators)
- [Punctuation and Delimiters](#punctuation-and-delimiters)
- [Keywords](#keywords)
- [Error Handling](#error-handling)

## Program Structure

Top-level script structure

Lua programs are called "chunks" - a sequence of statements. A chunk can be stored in a file or a string.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `chunk` | DEFINITION_MODULE | NONE | Chunk - root node representing the entire Lua script |
| `block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Block - sequence of statements in a scope |

## Function Definitions

Lua function declarations

Lua function syntax: - Named: `function name(params) ... end` - Local: `local function name(params) ... end` - Anonymous: `function(params) ... end` - Method: `function obj:method(params) ... end` (implicit self) Functions in Lua: - Are first-class values - Support closures (capture upvalues) - Can return multiple values - Support tail-call optimization

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Function::REGULAR | CUSTOM | Named function declaration - `function name(...) end` |
| `function_definition` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Anonymous function - `function(...) end` as expression |
| `local_function_declaration` | DEFINITION_FUNCTION | Function::REGULAR | CUSTOM | Local function - `local function name(...) end` |
| `parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Parameter list - function parameters |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE | Return statement - `return expr, expr, ...` |

## Function Calls

Function and method invocations

Lua call syntax: - Function: `func(args)` or `func "string"` or `func {table}` - Method: `obj:method(args)` (passes obj as first argument)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_call` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function call - `func(args)` |
| `arguments` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Argument list |
| `method_index_expression` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET | Method call - `obj:method(args)` |

## Variable Declarations

Variable creation and assignment

Lua variable scoping: - Global by default (no keyword needed) - `local` for lexically scoped variables - Multiple assignment: `a, b = 1, 2` - Destructuring from function returns: `a, b = func()`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable declaration (global) |
| `local_variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Local variable declaration - `local x = value` |
| `assignment_statement` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | FIND_IDENTIFIER | Assignment statement - `x = value` or `a, b = 1, 2` |
| `variable_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Variable list - left side of multiple assignment |
| `expression_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Expression list - right side of multiple assignment |

## Table Constructs

Lua's universal data structure

Tables in Lua: - The only data structure (arrays, dictionaries, objects, modules) - Array syntax: `{1, 2, 3}` (1-indexed) - Dictionary syntax: `{key = value}` or `{["key"] = value}` - Mixed: `{"first", key = "value"}` - Used for OOP via metatables

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `table_constructor` | LITERAL_STRUCTURED | Structured::MAPPING | NONE | Table constructor - `{...}` |
| `field` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Field in table - `key = value` or `[expr] = value` |
| `field_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Field list in table constructor |

## Identifiers and References

Names and access expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - variable or function name |
| `dot_index_expression` | COMPUTATION_ACCESS | NONE | Dot index - `table.key` access |
| `bracket_index_expression` | COMPUTATION_ACCESS | NONE | Bracket index - `table[expr]` access |

## Literals

Lua literal values

Lua has simple literal types: - Numbers (double precision floats, integers in 5.3+) - Strings (single/double quoted, long strings `[[...]]`) - Booleans: `true`, `false` - Nil: `nil` (absence of value)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `number` | LITERAL_NUMBER | NODE_TEXT | Number literal - integers and floats |
| `string` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal - `"..."`, `'...'`, or `[[...]]` |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean true |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean false |
| `nil` | LITERAL_ATOMIC | NODE_TEXT | Nil value - absence of value |
| `vararg_expression` | NAME_IDENTIFIER | NODE_TEXT | Vararg expression - `...` for accessing variadic arguments |

## Control Flow - Conditionals

Conditional statements

Lua conditionals: - `if ... then ... elseif ... then ... else ... end` - Only `false` and `nil` are falsy; everything else is truthy

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement - `if ... then ... end` |
| `elseif_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Elseif clause - `elseif ... then ...` |
| `else_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Else clause - `else ...` |

## Control Flow - Loops

Iteration constructs

Lua loop types: - `for i = start, stop, step do ... end` (numeric) - `for k, v in pairs(t) do ... end` (generic/iterator) - `while ... do ... end` - `repeat ... until ...`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_statement` | FLOW_LOOP | NONE | For statement container |
| `for_generic_clause` | FLOW_LOOP | Loop::ITERATOR | NONE | Generic for clause - `for k, v in iterator do` |
| `for_numeric_clause` | FLOW_LOOP | Loop::COUNTER | NONE | Numeric for clause - `for i = 1, 10, 1 do` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While loop - `while ... do ... end` |
| `repeat_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Repeat loop - `repeat ... until ...` (do-while equivalent) |
| `do_statement` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Do block - `do ... end` creates a scope |

## Jump Statements

Control flow transfer

Lua jump statements: - `break` - exits innermost loop - `goto label` - jumps to label (Lua 5.2+) - `::label::` - label definition - No `continue` (use goto or restructure)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE | Break statement - exits loop |
| `goto_statement` | FLOW_JUMP | Jump::GOTO | FIND_IDENTIFIER | Goto statement - jumps to label (Lua 5.2+) |
| `label_statement` | NAME_IDENTIFIER | FIND_IDENTIFIER | Label statement - `::label::` for goto target |

## Expressions

Operators and expression types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Binary expression - arithmetic, comparison, logical, string concat |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary expression - `-`, `not`, `#` (length) |
| `parenthesized_expression` | ORGANIZATION_BLOCK | NONE | Parenthesized expression - `(expr)` |

## Comments and Metadata

Documentation and annotations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment - `--` or `--[[ multiline ]]` |
| `empty_statement` | PARSER_SYNTAX | NONE | Empty statement - semicolon (optional in Lua) |
| `attribute` | METADATA_ANNOTATION | FIND_IDENTIFIER | Attribute - `<const>` or `<close>` (Lua 5.4) |

## Operators

Lua operators

Lua operators: - Arithmetic: `+`, `-`, `*`, `/`, `//` (floor), `%`, `^` (power) - Comparison: `==`, `~=`, `<`, `>`, `<=`, `>=` - Logical: `and`, `or`, `not` (short-circuit evaluation) - String: `..` (concatenation) - Length: `#` (table/string length) - Bitwise (5.3+): `&`, `|`, `~`, `<<`, `>>`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_ARITHMETIC | NODE_TEXT | Lua operators |
| `-` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `*` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `/` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `//` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `%` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `^` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `..` | OPERATOR_ARITHMETIC | NODE_TEXT |  |

## Punctuation and Delimiters

Syntactic markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NODE_TEXT | Syntactic markers |
| `)` | PARSER_DELIMITER | NODE_TEXT |  |
| `[` | PARSER_DELIMITER | NODE_TEXT |  |
| `]` | PARSER_DELIMITER | NODE_TEXT |  |
| `{` | PARSER_DELIMITER | NODE_TEXT |  |
| `}` | PARSER_DELIMITER | NODE_TEXT |  |
| `,` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `;` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `.` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `:` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `::` | PARSER_PUNCTUATION | NODE_TEXT |  |

## Keywords

Lua reserved words

Lua has 22 reserved keywords. They cannot be used as identifiers.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `break` | FLOW_JUMP | NODE_TEXT | Lua reserved words |
| `do` | ORGANIZATION_BLOCK | NODE_TEXT |  |
| `else` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `elseif` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `end` | ORGANIZATION_BLOCK | NODE_TEXT |  |
| `for` | FLOW_LOOP | NODE_TEXT |  |
| `function` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `goto` | FLOW_JUMP | NODE_TEXT |  |
| `if` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `in` | FLOW_LOOP | NODE_TEXT |  |
| `local` | DEFINITION_VARIABLE | NODE_TEXT |  |
| `repeat` | FLOW_LOOP | NODE_TEXT |  |
| `return` | FLOW_JUMP | NODE_TEXT |  |
| `then` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `until` | FLOW_LOOP | NODE_TEXT |  |
| `while` | FLOW_LOOP | NODE_TEXT |  |

## Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

## Other Node Types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `==` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `~=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `and` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `or` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `not` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `&` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `|` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `~` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `<<` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `>>` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Assignment operator |
| `#` | OPERATOR_ARITHMETIC | NODE_TEXT | Length operator - `#table` or `#string` |

---

*Generated from `lua_types.def`*
