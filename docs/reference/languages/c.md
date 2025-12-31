# C Node Types

> C language node type mappings for AST semantic extraction

## Language Characteristics

- **Low-level systems language**: Direct memory access, pointer arithmetic
- **Static typing**: Compile-time type checking
- **Manual memory management**: malloc/free, no garbage collection
- **Preprocessor**: Text substitution before compilation (#include, #define)
- **Structs/unions/enums**: Aggregate data types (no classes)
- **Pointers**: First-class support for memory addresses
- **Function definitions vs declarations**: Prototypes vs implementations
- **Storage classes**: static, extern, register, auto
- **Type qualifiers**: const, volatile, restrict
- **No function overloading**: Each function has unique name

## Semantic Type Encoding

Semantic types use 8-bit encoding:
- Bits 7-2: Base category (e.g., `DEFINITION_FUNCTION = 0x04`)
- Bits 1-0: Refinement within category

Example: `DEFINITION_FUNCTION | SemanticRefinements::Function::REGULAR`
  - Base: 0x04 (function definition)
  - Refinement: 0x00 (regular function)
  - Combined: 0x04

## Node Categories

- [Translation Unit](#translation-unit)
- [Preprocessor Directives](#preprocessor-directives)
- [Declarations](#declarations)
- [Type Specifiers](#type-specifiers)
- [Statements](#statements)
- [Expressions](#expressions)
- [Identifiers and Literals](#identifiers-and-literals)
- [Comments](#comments)
- [Storage and Type Qualifiers](#storage-and-type-qualifiers)
- [Attributes and Extensions](#attributes-and-extensions)
- [Structural Elements](#structural-elements)
- [Punctuation](#punctuation)
- [Assignment Operators](#assignment-operators)
- [Comparison Operators](#comparison-operators)
- [Arithmetic Operators](#arithmetic-operators)
- [Logical Operators](#logical-operators)
- [Member Access Operators](#member-access-operators)
- [Keywords](#keywords)
- [Preprocessor Directive Tokens](#preprocessor-directive-tokens)
- [GNU Extensions](#gnu-extensions)
- [Parser Errors](#parser-errors)

## Translation Unit

Top-level compilation unit

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `translation_unit` | DEFINITION_MODULE | NONE | Translation unit: the root of a C source file |

## Preprocessor Directives

C preprocessor constructs (#include, #define, etc.)

The C preprocessor performs text substitution before compilation. These nodes represent preprocessor directives and macro expansions.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `preproc_include` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT | #include directive: `#include <header.h>` or `#include "header.h"` |
| `preproc_def` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER | Object-like macro: `#define NAME value` |
| `preproc_function_def` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Function-like macro: `#define NAME(args) body` |
| `preproc_call` | COMPUTATION_CALL | Call::MACRO | FIND_CALL_TARGET | Macro invocation: `MACRO(args)` |
| `preproc_if` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Conditional compilation: `#if condition` |
| `preproc_ifdef` | FLOW_CONDITIONAL | Conditional::BINARY | FIND_IDENTIFIER | Conditional compilation: `#ifdef MACRO` |
| `preproc_else` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Conditional else: `#else` |
| `preproc_elif` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Conditional elif: `#elif condition` |
| `preproc_params` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Preprocessor parameters in function-like macro |
| `preproc_arg` | METADATA_DIRECTIVE | NODE_TEXT | Preprocessor argument text |
| `defined` | METADATA_DIRECTIVE | NODE_TEXT | defined() operator in preprocessor conditionals |
| `preproc_defined` | METADATA_DIRECTIVE | NONE | Preprocessor defined check |
| `preproc_directive` | METADATA_DIRECTIVE | NODE_TEXT | Generic preprocessor directive |

## Declarations

Function definitions, variable declarations, type definitions

C distinguishes between declarations (prototypes) and definitions (implementations). Function definitions have bodies (IS_EMBODIED).

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IN_DECLARATOR | Function definition: `type name(params) { body }` |
| `declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable/function declaration: `type name;` or `type name(params);` |
| `struct_specifier` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Struct definition: `struct name { fields };` |
| `union_specifier` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Union definition: `union name { fields };` |
| `enum_specifier` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER | Enum definition: `enum name { values };` |
| `typedef_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Typedef: `typedef old_type new_name;` |
| `type_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Type definition (alternate form) |
| `field_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Field in struct/union: `type name;` |
| `enumerator` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Enumerator: `NAME` or `NAME = value` in enum |
| `parameter_declaration` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Function parameter: `type name` in parameter list |
| `init_declarator` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Initializing declarator: `name = value` in declaration |

## Type Specifiers

Type system nodes

C has primitive types, user-defined types, and compound types built with pointers, arrays, and function types.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `primitive_type` | TYPE_PRIMITIVE | NODE_TEXT | Primitive type: int, char, float, double, void, etc. |
| `type_identifier` | TYPE_REFERENCE | NODE_TEXT | User-defined type reference: struct/union/enum/typedef name |
| `sized_type_specifier` | TYPE_PRIMITIVE | NODE_TEXT | Sized type specifier: `long int`, `short int`, etc. |
| `pointer_declarator` | TYPE_COMPOSITE | FIND_IDENTIFIER | Pointer declarator: `*name` |
| `array_declarator` | TYPE_COMPOSITE | FIND_IDENTIFIER | Array declarator: `name[size]` |
| `function_declarator` | TYPE_COMPOSITE | FIND_IDENTIFIER | Function declarator: `name(params)` |
| `abstract_declarator` | TYPE_COMPOSITE | NONE | Abstract declarator (type without name) |
| `parenthesized_declarator` | TYPE_COMPOSITE | FIND_IDENTIFIER | Parenthesized declarator: `(*name)` |
| `abstract_pointer_declarator` | TYPE_REFERENCE | NONE | Abstract pointer declarator |
| `abstract_parenthesized_declarator` | TYPE_REFERENCE | NONE | Abstract parenthesized declarator |
| `abstract_array_declarator` | TYPE_COMPOSITE | NONE | Abstract array declarator |
| `abstract_function_declarator` | TYPE_COMPOSITE | NONE | Abstract function declarator |
| `type_descriptor` | TYPE_REFERENCE | NONE | Type descriptor in expressions |
| `macro_type_specifier` | TYPE_REFERENCE | NODE_TEXT | Macro used as type specifier |
| `bitfield_clause` | TYPE_COMPOSITE | NONE | Bitfield specification: `: width` |

## Statements

Control flow and executable statements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `compound_statement` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Compound statement (block): `{ statements }` |
| `labeled_statement` | EXECUTION_STATEMENT | FIND_IDENTIFIER | Labeled statement: `label: statement` |
| `expression_statement` | EXECUTION_STATEMENT | NONE | Expression statement: `expression;` |
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement: `if (cond) stmt` or `if (cond) stmt else stmt` |
| `else_clause` | FLOW_CONDITIONAL | NONE | Else clause in if statement |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement: `switch (expr) { cases }` |
| `case_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Case statement: `case value: statements` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While loop: `while (cond) stmt` |
| `for_statement` | FLOW_LOOP | Loop::COUNTER | NONE | For loop: `for (init; cond; update) stmt` |
| `do_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Do-while loop: `do stmt while (cond);` |
| `goto_statement` | FLOW_JUMP | Jump::GOTO | FIND_IDENTIFIER | Goto statement: `goto label;` |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE | Continue statement: `continue;` |
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE | Break statement: `break;` |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE | Return statement: `return expr;` |

## Expressions

Computation, access, and operator expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function call: `func(args)` |
| `field_expression` | COMPUTATION_ACCESS | FIND_IDENTIFIER | Field access: `struct.field` or `struct->field` |
| `subscript_expression` | COMPUTATION_ACCESS | NONE | Array subscript: `array[index]` |
| `assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Assignment: `lvalue = rvalue` |
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Binary expression: `a + b`, `a * b`, etc. |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary expression: `!a`, `-a`, `*ptr`, `&var` |
| `update_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Update expression: `a++`, `++a`, `a--`, `--a` |
| `cast_expression` | COMPUTATION_EXPRESSION | NONE | Cast expression: `(type)expr` |
| `sizeof_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Sizeof expression: `sizeof(type)` or `sizeof expr` |
| `conditional_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE | Conditional/ternary expression: `cond ? then : else` |
| `comma_expression` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Comma expression: `expr1, expr2` |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE | Parenthesized expression: `(expr)` |
| `pointer_expression` | COMPUTATION_ACCESS | NONE | Pointer dereference or address-of |
| `compound_literal_expression` | LITERAL_STRUCTURED | NONE | Compound literal: `(type){ initializers }` |
| `offsetof_expression` | COMPUTATION_CALL | NONE | offsetof expression: `offsetof(type, member)` |

## Identifiers and Literals

Names and constant values

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier: variable, function, or type name |
| `field_identifier` | NAME_IDENTIFIER | NODE_TEXT | Field identifier in struct access |
| `statement_identifier` | NAME_IDENTIFIER | NODE_TEXT | Statement identifier (goto label) |
| `number_literal` | LITERAL_NUMBER | NODE_TEXT | Number literal: 42, 3.14, 0xFF, etc. |
| `char_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Character literal: 'a', '\n', etc. |
| `string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal: "hello" |
| `concatenated_string` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Concatenated string: "hello" "world" |
| `string_content` | LITERAL_STRING | NODE_TEXT | String content (inside quotes) |
| `escape_sequence` | LITERAL_STRING | NODE_TEXT | Escape sequence in string: \n, \t, etc. |
| `character` | LITERAL_STRING | NODE_TEXT | Character content |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean true (C99 stdbool.h) |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean false (C99 stdbool.h) |
| `null` | LITERAL_ATOMIC | NODE_TEXT | Null pointer: null (lowercase) |
| `NULL` | LITERAL_ATOMIC | NODE_TEXT | Null pointer: NULL (uppercase macro) |
| `nullptr` | LITERAL_ATOMIC | NODE_TEXT | Null pointer: nullptr (C23) |

## Comments

Code comments

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment: line or block style |

## Storage and Type Qualifiers

Storage class specifiers and type qualifiers

Storage classes control linkage and lifetime. Type qualifiers modify how values can be accessed.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `storage_class_specifier` | METADATA_ANNOTATION | NODE_TEXT | Storage class specifier: static, extern, register, auto |
| `type_qualifier` | METADATA_ANNOTATION | NODE_TEXT | Type qualifier: const, volatile, restrict |

## Attributes and Extensions

Compiler-specific attributes and extensions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variadic_parameter` | PATTERN_COLLECT | NODE_TEXT | Variadic parameter: `...` in function declaration |
| `attribute_specifier` | METADATA_ANNOTATION | NODE_TEXT | GNU/C11 attribute specifier |
| `ms_declspec_modifier` | METADATA_ANNOTATION | NODE_TEXT | Microsoft __declspec modifier |
| `ms_restrict_modifier` | METADATA_ANNOTATION | NODE_TEXT | Microsoft restrict modifier |
| `ms_pointer_modifier` | METADATA_ANNOTATION | NODE_TEXT | Microsoft pointer modifier |
| `ms_call_modifier` | METADATA_ANNOTATION | NODE_TEXT | Microsoft call modifier (__cdecl, __stdcall, etc.) |
| `linkage_specification` | METADATA_ANNOTATION | NODE_TEXT | Linkage specification: `extern "C"` |
| `attribute_declaration` | METADATA_ANNOTATION | NONE | C11 attribute declaration: `[[attribute]]` |
| `attribute` | METADATA_ANNOTATION | NODE_TEXT | Attribute in attribute list |
| `alignas_qualifier` | METADATA_ANNOTATION | NODE_TEXT | C11 alignas qualifier |

## Structural Elements

Lists and organizational nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `initializer_pair` | PATTERN_DESTRUCTURE | FIND_PROPERTY | Designated initializer: `.field = value` or `[index] = value` |
| `argument_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Function call argument list |
| `subscript_designator` | COMPUTATION_ACCESS | NONE | Array subscript in designator |
| `initializer_list` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Initializer list: `{ values }` |
| `field_designator` | PATTERN_DESTRUCTURE | FIND_PROPERTY | Field designator: `.field` |
| `parameter_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Function parameter list |
| `field_declaration_list` | ORGANIZATION_LIST | NONE | Field declaration list in struct/union |
| `enumerator_list` | ORGANIZATION_LIST | NONE | Enumerator list in enum |
| `declaration_list` | ORGANIZATION_LIST | NONE | Declaration list (multiple declarations) |
| `system_lib_string` | EXTERNAL_IMPORT | NODE_TEXT | System library string: `<header.h>` |

## Punctuation

Syntactic punctuation tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `,` | PARSER_PUNCTUATION | NODE_TEXT | Syntactic punctuation tokens |
| `;` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `.` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `:` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `(` | PARSER_DELIMITER | NODE_TEXT |  |
| `)` | PARSER_DELIMITER | NODE_TEXT |  |
| `[` | PARSER_DELIMITER | NODE_TEXT |  |
| `]` | PARSER_DELIMITER | NODE_TEXT |  |
| `{` | PARSER_DELIMITER | NODE_TEXT |  |
| `}` | PARSER_DELIMITER | NODE_TEXT |  |
| `'` | PARSER_DELIMITER | NODE_TEXT |  |
| `?` | FLOW_CONDITIONAL | NONE |  |
| `...` | PATTERN_COLLECT | NODE_TEXT |  |
| `[[` | PARSER_DELIMITER | NONE |  |
| `]]` | PARSER_DELIMITER | NONE |  |
| `u'` | PARSER_DELIMITER | NONE |  |
| `\n` | PARSER_SYNTAX | NONE |  |

## Assignment Operators

Simple and compound assignment

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Simple and compound assignment |
| `+=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `-=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `*=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `/=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `%=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `&=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `|=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `^=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `<<=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `>>=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |

## Comparison Operators

Equality and relational operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `==` | OPERATOR_COMPARISON | NODE_TEXT | Equality and relational operators |
| `!=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>=` | OPERATOR_COMPARISON | NODE_TEXT |  |

## Arithmetic Operators

Mathematical and bitwise operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_ARITHMETIC | NODE_TEXT | Mathematical and bitwise operators |
| `-` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `*` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `/` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `%` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `&` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `|` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `^` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `~` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `<<` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `>>` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `++` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `--` | OPERATOR_ARITHMETIC | NODE_TEXT |  |

## Logical Operators

Boolean operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `&&` | OPERATOR_LOGICAL | NODE_TEXT | Boolean operators |
| `||` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `!` | OPERATOR_LOGICAL | NODE_TEXT |  |

## Member Access Operators

Pointer and scope operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `->` | COMPUTATION_ACCESS | NODE_TEXT | Pointer and scope operators |
| `::` | COMPUTATION_ACCESS | NONE |  |

## Keywords

Reserved words

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if` | FLOW_CONDITIONAL | NODE_TEXT | Reserved words |
| `else` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `while` | FLOW_LOOP | NODE_TEXT |  |
| `for` | FLOW_LOOP | NODE_TEXT |  |
| `do` | FLOW_LOOP | NODE_TEXT |  |
| `switch` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `case` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `default` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `break` | FLOW_JUMP | NODE_TEXT |  |
| `continue` | FLOW_JUMP | NODE_TEXT |  |
| `return` | FLOW_JUMP | NODE_TEXT |  |
| `goto` | FLOW_JUMP | NODE_TEXT |  |
| `const` | METADATA_ANNOTATION | NODE_TEXT |  |
| `static` | METADATA_ANNOTATION | NODE_TEXT |  |
| `extern` | METADATA_ANNOTATION | NODE_TEXT |  |
| `register` | METADATA_ANNOTATION | NODE_TEXT |  |
| `volatile` | METADATA_ANNOTATION | NODE_TEXT |  |
| `inline` | METADATA_ANNOTATION | NODE_TEXT |  |
| `typedef` | METADATA_ANNOTATION | NODE_TEXT |  |
| `struct` | METADATA_ANNOTATION | NODE_TEXT |  |
| `union` | METADATA_ANNOTATION | NODE_TEXT |  |
| `enum` | METADATA_ANNOTATION | NODE_TEXT |  |
| `sizeof` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `offsetof` | COMPUTATION_CALL | NODE_TEXT |  |
| `unsigned` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `signed` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `long` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `short` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `auto` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `restrict` | METADATA_ANNOTATION | NODE_TEXT |  |
| `thread_local` | METADATA_ANNOTATION | NODE_TEXT |  |
| `alignas` | METADATA_ANNOTATION | NODE_TEXT |  |
| `constexpr` | METADATA_ANNOTATION | NODE_TEXT |  |
| `asm` | EXECUTION_STATEMENT | NONE |  |

## Preprocessor Directive Tokens

Preprocessor directive keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `#include` | METADATA_DIRECTIVE | NODE_TEXT | Preprocessor directive keywords |
| `#define` | METADATA_DIRECTIVE | NODE_TEXT |  |
| `#ifdef` | METADATA_DIRECTIVE | NODE_TEXT |  |
| `#ifndef` | METADATA_DIRECTIVE | NODE_TEXT |  |
| `#if` | METADATA_DIRECTIVE | NODE_TEXT |  |
| `#else` | METADATA_DIRECTIVE | NODE_TEXT |  |
| `#elif` | METADATA_DIRECTIVE | NODE_TEXT |  |
| `#endif` | METADATA_DIRECTIVE | NODE_TEXT |  |
| `#undef` | METADATA_DIRECTIVE | NODE_TEXT |  |
| `#pragma` | METADATA_DIRECTIVE | NODE_TEXT |  |

## GNU Extensions

GCC-specific language extensions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `__asm__` | EXECUTION_STATEMENT | NODE_TEXT | GNU inline assembly: `__asm__(...)` |
| `gnu_asm_expression` | COMPUTATION_EXPRESSION | NONE | GNU asm expression |
| `gnu_asm_output_operand` | COMPUTATION_EXPRESSION | NONE | GNU asm output operand |
| `gnu_asm_input_operand` | COMPUTATION_EXPRESSION | NONE | GNU asm input operand |
| `gnu_asm_input_operand_list` | ORGANIZATION_LIST | NONE | GNU asm input operand list |
| `gnu_asm_output_operand_list` | ORGANIZATION_LIST | NONE | GNU asm output operand list |
| `gnu_asm_clobber_list` | ORGANIZATION_LIST | NONE | GNU asm clobber list |
| `gnu_asm_qualifier` | METADATA_ANNOTATION | NODE_TEXT | GNU asm qualifier (volatile, inline, goto) |
| `__attribute__` | METADATA_ANNOTATION | NODE_TEXT | GNU __attribute__ specifier |
| `__restrict__` | METADATA_ANNOTATION | NODE_TEXT | GNU __restrict__ keyword |
| `__declspec` | METADATA_ANNOTATION | NODE_TEXT | Microsoft __declspec |
| `__forceinline` | METADATA_ANNOTATION | NODE_TEXT | Microsoft __forceinline |
| `__stdcall` | METADATA_ANNOTATION | NODE_TEXT | Microsoft __stdcall |

## Parser Errors

Error nodes from parsing

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Error nodes from parsing |

---

*Generated from `c_types.def`*
