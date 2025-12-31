# Cpp Node Types

> C++ language node type mappings for AST semantic extraction

## Node Categories

- [Function Definitions](#function-definitions)
- [Class Definitions](#class-definitions)
- [Variable Declarations](#variable-declarations)
- [Namespaces and Modules](#namespaces-and-modules)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Member Access](#member-access)
- [Identifiers and References](#identifiers-and-references)
- [Type System](#type-system)
- [Literals](#literals)
- [Control Flow](#control-flow)
- [Coroutines (C++20)](#coroutines-(c++20))
- [Exception Handling](#exception-handling)
- [Lambda Expressions](#lambda-expressions)
- [Templates](#templates)
- [Structure and Organization](#structure-and-organization)
- [Imports and Using](#imports-and-using)
- [Preprocessor Directives](#preprocessor-directives)
- [Punctuation](#punctuation)
- [Assignment Operators](#assignment-operators)
- [Comparison Operators](#comparison-operators)
- [Arithmetic Operators](#arithmetic-operators)
- [Logical Operators](#logical-operators)
- [Member Access Operators](#member-access-operators)
- [Keywords](#keywords)
- [Metadata and Specifiers](#metadata-and-specifiers)
- [Preprocessor Directive Tokens](#preprocessor-directive-tokens)
- [Parser Errors](#parser-errors)

## Function Definitions

C++ function and method definitions

C++ has regular functions, member functions (methods), constructors, destructors, and lambda expressions. Definitions have bodies (IS_EMBODIED), declarations do not (IS_DECLARATION_ONLY).

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Function::REGULAR | FIND_QUALIFIED_IDENTIFIER | Function definition: `ReturnType name(params) { body }` |
| `function_declarator` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Function declarator (forward declaration): `ReturnType name(params);` |
| `method_definition` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Method definition in class body |
| `constructor_definition` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER | Constructor: `ClassName(params) : initializers { body }` |
| `destructor_definition` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER | Destructor: `~ClassName() { body }` |
| `lambda_expression` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Lambda expression: `[captures](params) { body }` |
| `template_function` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Template function definition |
| `template_method` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Template method definition |
| `preproc_function_def` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Preprocessor function-like macro |
| `default_method_clause` | DEFINITION_FUNCTION | NONE | Default method clause: `= default` |
| `delete_method_clause` | DEFINITION_FUNCTION | NONE | Delete method clause: `= delete` |

## Class Definitions

C++ class, struct, union, and enum types

C++ classes support inheritance, access control, and polymorphism. All class-like constructs have bodies (IS_EMBODIED).

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_specifier` | DEFINITION_CLASS | FIND_IDENTIFIER | Class definition: `class Name : public Base { members };` |
| `struct_specifier` | DEFINITION_CLASS | FIND_IDENTIFIER | Struct definition: `struct Name { members };` |
| `union_specifier` | DEFINITION_CLASS | FIND_IDENTIFIER | Union definition: `union Name { members };` |
| `enum_specifier` | DEFINITION_CLASS | FIND_IDENTIFIER | Enum definition: `enum Name { values };` or `enum class Name { values };` |
| `type_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Type definition (typedef or using) |
| `base_class_clause` | TYPE_REFERENCE | NONE | Base class clause: `: public Base, private Other` |

## Variable Declarations

Variable, parameter, and field declarations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable declaration: `Type name = value;` |
| `parameter_declaration` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Function parameter: `Type name` in parameter list |
| `optional_parameter_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Optional parameter with default: `Type name = default` |
| `field_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Field declaration in class: `Type name;` |
| `init_declarator` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Initializing declarator: `name = value` in declaration |
| `enumerator` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Enumerator: `NAME` or `NAME = value` in enum |
| `alias_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Type alias: `using Name = Type;` |

## Namespaces and Modules

Code organization constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `namespace_definition` | DEFINITION_MODULE | FIND_IDENTIFIER | Namespace definition: `namespace Name { ... }` |
| `translation_unit` | DEFINITION_MODULE | NONE | Translation unit: root of compilation unit |

## Function Calls and Expressions

Call expressions and operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function call: `func(args)` |
| `new_expression` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET | Constructor call: `new Type(args)` |
| `delete_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Destructor call: `delete ptr` or `delete[] ptr` |
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Binary expression: `a + b`, `a * b`, etc. |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary expression: `!a`, `-a`, `*ptr`, `&var` |
| `assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Assignment expression: `a = b` |
| `update_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Update expression: `a++`, `++a`, `a--`, `--a` |
| `cast_expression` | COMPUTATION_EXPRESSION | NONE | Cast expression: `(Type)expr` or C++ casts |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE | Parenthesized expression: `(expr)` |
| `expression_statement` | EXECUTION_STATEMENT | NONE | Expression statement: expression used as statement |

## Member Access

Property and pointer access expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `field_expression` | COMPUTATION_ACCESS | NONE | Field access: `obj.member` |
| `subscript_expression` | COMPUTATION_ACCESS | NONE | Subscript access: `array[index]` |
| `pointer_expression` | COMPUTATION_ACCESS | NONE | Pointer dereference or address-of |

## Identifiers and References

Names and qualified names

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Simple identifier: variable or function name |
| `field_identifier` | NAME_IDENTIFIER | NODE_TEXT | Field identifier in member access |
| `namespace_identifier` | NAME_QUALIFIED | NODE_TEXT | Namespace identifier: `std`, `boost`, etc. |
| `qualified_identifier` | NAME_QUALIFIED | NODE_TEXT | Qualified identifier: `Namespace::Name` |
| `scoped_identifier` | NAME_SCOPED | NODE_TEXT | Scoped identifier in scope resolution |
| `operator_name` | NAME_IDENTIFIER | NODE_TEXT | Operator name for overloading: `operator+` |
| `destructor_name` | NAME_IDENTIFIER | NODE_TEXT | Destructor name: `~ClassName` |
| `this` | NAME_SCOPED | NONE | this pointer reference |

## Type System

Type specifiers, references, and templates

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `primitive_type` | TYPE_PRIMITIVE | NODE_TEXT | Primitive type: int, char, float, double, void, bool |
| `type_identifier` | TYPE_PRIMITIVE | NODE_TEXT | Type identifier: user-defined type name |
| `sized_type_specifier` | TYPE_PRIMITIVE | NODE_TEXT | Sized type specifier: `long int`, `unsigned char`, etc. |
| `struct_type` | TYPE_COMPOSITE | NONE | Struct type reference |
| `union_type` | TYPE_COMPOSITE | NONE | Union type reference |
| `pointer_type` | TYPE_REFERENCE | NONE | Pointer type: `Type*` |
| `reference_type` | TYPE_REFERENCE | NONE | Reference type: `Type&` (lvalue) or `Type&&` (rvalue) |
| `template_type` | TYPE_GENERIC | NONE | Template type: `Template<Args>` |
| `auto` | TYPE_GENERIC | NONE | Auto type inference (C++11) |
| `type_descriptor` | TYPE_COMPOSITE | NONE | Type descriptor in expressions |
| `reference_declarator` | TYPE_REFERENCE | FIND_IDENTIFIER | Reference declarator: `&name` or `&&name` |
| `pointer_declarator` | TYPE_COMPOSITE | FIND_IDENTIFIER | Pointer declarator: `*name` |
| `array_declarator` | TYPE_COMPOSITE | FIND_IDENTIFIER | Array declarator: `name[size]` |
| `abstract_pointer_declarator` | TYPE_REFERENCE | NONE | Abstract pointer declarator (no name) |
| `abstract_reference_declarator` | TYPE_REFERENCE | NONE | Abstract reference declarator (no name) |
| `abstract_function_declarator` | TYPE_COMPOSITE | NONE | Abstract function declarator (no name) |
| `placeholder_type_specifier` | TYPE_GENERIC | NONE | Placeholder type specifier: `decltype(auto)` |
| `trailing_return_type` | TYPE_REFERENCE | NONE | Trailing return type: `-> Type` |
| `type_parameter_declaration` | TYPE_GENERIC | FIND_IDENTIFIER | Type parameter declaration in template |

## Literals

String, number, and structured literals

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal: `"hello"` |
| `raw_string_literal` | LITERAL_STRING | String::RAW | NODE_TEXT | Raw string literal: `R"(content)"` |
| `char_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Character literal: `'a'` |
| `number_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Number literal: 42, 3.14, 0xFF |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean true |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean false |
| `nullptr` | LITERAL_ATOMIC | NODE_TEXT | Null pointer: nullptr |
| `null` | LITERAL_ATOMIC | NODE_TEXT | Null pointer (C-style) |
| `initializer_list` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Initializer list: `{ values }` |
| `compound_literal_expression` | LITERAL_STRUCTURED | NONE | Compound literal expression |
| `string_content` | LITERAL_STRING | NODE_TEXT | String content (inside quotes) |
| `escape_sequence` | LITERAL_STRING | NODE_TEXT | Escape sequence: `\n`, `\t`, etc. |
| `character` | LITERAL_STRING | NODE_TEXT | Character content |
| `concatenated_string` | LITERAL_STRING | NODE_TEXT | Concatenated string literals |
| `raw_string_delimiter` | LITERAL_STRING | NODE_TEXT | Raw string delimiter |
| `raw_string_content` | LITERAL_STRING | NODE_TEXT | Raw string content |

## Control Flow

Conditionals, loops, and jumps

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement: `if (cond) stmt` |
| `else_clause` | FLOW_CONDITIONAL | NONE | Else clause in if statement |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement: `switch (expr) { cases }` |
| `case_statement` | FLOW_CONDITIONAL | NONE | Case statement: `case value:` |
| `conditional_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE | Conditional/ternary expression: `cond ? then : else` |
| `condition_clause` | FLOW_CONDITIONAL | NONE | Condition clause in control statements |
| `for_statement` | FLOW_LOOP | Loop::COUNTER | NONE | C-style for loop: `for (init; cond; update)` |
| `for_range_loop` | FLOW_LOOP | Loop::ITERATOR | NONE | Range-based for loop (C++11): `for (auto x : container)` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While loop: `while (cond) stmt` |
| `do_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Do-while loop: `do stmt while (cond);` |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE | Return statement: `return expr;` |
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE | Break statement: `break;` |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE | Continue statement: `continue;` |
| `goto_statement` | FLOW_JUMP | Jump::GOTO | NONE | Goto statement: `goto label;` |

## Coroutines (C++20)

Coroutine expressions and statements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `co_await_expression` | FLOW_SYNC | NONE | co_await expression: `co_await awaitable` |
| `co_yield_expression` | FLOW_SYNC | NONE | co_yield expression: `co_yield value` |
| `co_return_statement` | FLOW_SYNC | NONE | co_return statement: `co_return value;` |

## Exception Handling

try/catch/throw statements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_statement` | ERROR_TRY | NONE | Try block: `try { ... }` |
| `catch_clause` | ERROR_CATCH | NONE | Catch clause: `catch (Type e) { ... }` |
| `throw_statement` | ERROR_THROW | NONE | Throw statement: `throw exception;` |

## Lambda Expressions

Lambda-specific constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `lambda_capture_specifier` | COMPUTATION_CLOSURE | NONE | Lambda capture specifier: `[captures]` |
| `lambda_declarator` | COMPUTATION_CLOSURE | NONE | Lambda declarator: `(params) -> Type` |
| `lambda_default_capture` | COMPUTATION_CLOSURE | NONE | Lambda default capture: `[=]` or `[&]` |

## Templates

Generic programming constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `template_declaration` | PATTERN_TEMPLATE | NONE | Template declaration: `template<typename T>` |

## Structure and Organization

Blocks and lists

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `compound_statement` | ORGANIZATION_BLOCK | NONE | Compound statement (block): `{ statements }` |
| `parameter_list` | ORGANIZATION_LIST | NONE | Function parameter list |
| `argument_list` | ORGANIZATION_LIST | NONE | Function argument list |
| `template_parameter_list` | ORGANIZATION_LIST | NONE | Template parameter list |
| `template_argument_list` | ORGANIZATION_LIST | NONE | Template argument list: `<int, string>` |
| `subscript_argument_list` | ORGANIZATION_LIST | NONE | Subscript argument list |
| `declaration_list` | ORGANIZATION_LIST | NONE | Declaration list |
| `field_declaration_list` | ORGANIZATION_LIST | NONE | Field declaration list in class |
| `enumerator_list` | ORGANIZATION_LIST | NONE | Enumerator list in enum |
| `field_initializer_list` | ORGANIZATION_LIST | NONE | Field initializer list in constructor |
| `field_initializer` | LITERAL_STRUCTURED | FIND_IDENTIFIER | Field initializer: `member(value)` |
| `preproc_params` | ORGANIZATION_LIST | NONE | Preprocessor params |

## Imports and Using

Include directives and using declarations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `preproc_include` | EXTERNAL_IMPORT | NONE | #include directive |
| `using_declaration` | EXTERNAL_IMPORT | NONE | Using declaration: `using namespace std;` |
| `system_lib_string` | EXTERNAL_IMPORT | NODE_TEXT | System library string: `<header>` |

## Preprocessor Directives

Preprocessor and metadata

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `preproc_def` | METADATA_DIRECTIVE | NONE | Object-like macro definition |
| `preproc_ifdef` | METADATA_DIRECTIVE | NONE | #ifdef directive |
| `preproc_ifndef` | METADATA_DIRECTIVE | NONE | #ifndef directive |
| `preproc_if` | METADATA_DIRECTIVE | NONE | #if directive |
| `preproc_else` | METADATA_DIRECTIVE | NONE | #else directive |
| `preproc_call` | METADATA_DIRECTIVE | NONE | Preprocessor macro call |
| `preproc_directive` | METADATA_DIRECTIVE | NONE | Generic preprocessor directive |
| `preproc_arg` | METADATA_DIRECTIVE | NONE | Preprocessor argument |
| `comment` | METADATA_COMMENT | NONE | Comment: line or block style |

## Punctuation

Syntactic punctuation tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `,` | PARSER_PUNCTUATION | NONE | Syntactic punctuation tokens |
| `;` | PARSER_PUNCTUATION | NONE |  |
| `.` | PARSER_PUNCTUATION | NONE |  |
| `:` | PARSER_PUNCTUATION | NONE |  |
| `(` | PARSER_DELIMITER | NONE |  |
| `)` | PARSER_DELIMITER | NONE |  |
| `[` | PARSER_DELIMITER | NONE |  |
| `]` | PARSER_DELIMITER | NONE |  |
| `{` | PARSER_DELIMITER | NONE |  |
| `}` | PARSER_DELIMITER | NONE |  |
| `'` | PARSER_DELIMITER | NONE |  |
| `?` | FLOW_CONDITIONAL | NONE |  |
| `()` | PARSER_DELIMITER | NONE |  |
| `\n` | PARSER_DELIMITER | NONE |  |

## Assignment Operators

Simple and compound assignment

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NONE | Simple and compound assignment |
| `+=` | OPERATOR_ASSIGNMENT | NONE |  |
| `-=` | OPERATOR_ASSIGNMENT | NONE |  |
| `*=` | OPERATOR_ASSIGNMENT | NONE |  |
| `/=` | OPERATOR_ASSIGNMENT | NONE |  |
| `%=` | OPERATOR_ASSIGNMENT | NONE |  |
| `&=` | OPERATOR_ASSIGNMENT | NONE |  |
| `|=` | OPERATOR_ASSIGNMENT | NONE |  |
| `^=` | OPERATOR_ASSIGNMENT | NONE |  |
| `<<=` | OPERATOR_ASSIGNMENT | NONE |  |
| `>>=` | OPERATOR_ASSIGNMENT | NONE |  |

## Comparison Operators

Equality and relational operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `==` | OPERATOR_COMPARISON | NONE | Equality and relational operators |
| `!=` | OPERATOR_COMPARISON | NONE |  |
| `<` | OPERATOR_COMPARISON | NONE |  |
| `>` | OPERATOR_COMPARISON | NONE |  |
| `<=` | OPERATOR_COMPARISON | NONE |  |
| `>=` | OPERATOR_COMPARISON | NONE |  |

## Arithmetic Operators

Mathematical and bitwise operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_ARITHMETIC | NONE | Mathematical and bitwise operators |
| `-` | OPERATOR_ARITHMETIC | NONE |  |
| `*` | OPERATOR_ARITHMETIC | NONE |  |
| `/` | OPERATOR_ARITHMETIC | NONE |  |
| `%` | OPERATOR_ARITHMETIC | NONE |  |
| `&` | OPERATOR_ARITHMETIC | NONE |  |
| `|` | OPERATOR_ARITHMETIC | NONE |  |
| `^` | OPERATOR_ARITHMETIC | NONE |  |
| `~` | OPERATOR_ARITHMETIC | NONE |  |
| `<<` | OPERATOR_ARITHMETIC | NONE |  |
| `>>` | OPERATOR_ARITHMETIC | NONE |  |
| `++` | OPERATOR_ARITHMETIC | NONE |  |
| `--` | OPERATOR_ARITHMETIC | NONE |  |

## Logical Operators

Boolean operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `&&` | OPERATOR_LOGICAL | NONE | Boolean operators |
| `||` | OPERATOR_LOGICAL | NONE |  |
| `!` | OPERATOR_LOGICAL | NONE |  |

## Member Access Operators

Pointer and scope operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `->` | COMPUTATION_ACCESS | NONE | Pointer and scope operators |
| `::` | COMPUTATION_ACCESS | NONE |  |

## Keywords

C++ reserved words

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if` | FLOW_CONDITIONAL | NONE | C++ reserved words |
| `else` | FLOW_CONDITIONAL | NONE |  |
| `while` | FLOW_LOOP | NONE |  |
| `for` | FLOW_LOOP | NONE |  |
| `do` | FLOW_LOOP | NONE |  |
| `switch` | FLOW_CONDITIONAL | NONE |  |
| `case` | FLOW_CONDITIONAL | NONE |  |
| `default` | FLOW_CONDITIONAL | NONE |  |
| `break` | FLOW_JUMP | NONE |  |
| `continue` | FLOW_JUMP | NONE |  |
| `return` | FLOW_JUMP | NONE |  |
| `goto` | FLOW_JUMP | NONE |  |
| `try` | ERROR_TRY | NONE |  |
| `catch` | ERROR_CATCH | NONE |  |
| `throw` | ERROR_THROW | NONE |  |
| `const` | METADATA_ANNOTATION | NONE |  |
| `static` | METADATA_ANNOTATION | NONE |  |
| `extern` | METADATA_ANNOTATION | NONE |  |
| `inline` | METADATA_ANNOTATION | NONE |  |
| `virtual` | METADATA_ANNOTATION | NONE |  |
| `override` | METADATA_ANNOTATION | NONE |  |
| `final` | METADATA_ANNOTATION | NONE |  |
| `public` | METADATA_ANNOTATION | NONE |  |
| `private` | METADATA_ANNOTATION | NONE |  |
| `protected` | METADATA_ANNOTATION | NONE |  |
| `class` | DEFINITION_CLASS | NONE |  |
| `struct` | DEFINITION_CLASS | NONE |  |
| `union` | DEFINITION_CLASS | NONE |  |
| `enum` | DEFINITION_CLASS | NONE |  |
| `namespace` | DEFINITION_MODULE | NONE |  |
| `using` | EXTERNAL_IMPORT | NONE |  |
| `typedef` | DEFINITION_CLASS | NONE |  |
| `typename` | TYPE_GENERIC | NONE |  |
| `template` | PATTERN_TEMPLATE | NONE |  |
| `sizeof` | OPERATOR_ARITHMETIC | NONE |  |
| `new` | COMPUTATION_CALL | NONE |  |
| `delete` | COMPUTATION_CALL | NONE |  |
| `constexpr` | METADATA_ANNOTATION | NONE |  |
| `noexcept` | METADATA_ANNOTATION | NONE |  |
| `explicit` | METADATA_ANNOTATION | NONE |  |
| `mutable` | METADATA_ANNOTATION | NONE |  |
| `operator` | DEFINITION_FUNCTION | NONE |  |

## Metadata and Specifiers

Type qualifiers and specifiers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `virtual_specifier` | METADATA_ANNOTATION | NONE | Virtual specifier in class |
| `access_specifier` | METADATA_ANNOTATION | NONE | Access specifier block: `public:`, `private:`, etc. |
| `type_qualifier` | METADATA_ANNOTATION | NONE | Type qualifier: const, volatile |
| `storage_class_specifier` | METADATA_ANNOTATION | NONE | Storage class specifier: static, extern, etc. |
| `explicit_function_specifier` | METADATA_ANNOTATION | NONE | Explicit function specifier |
| `linkage_specification` | METADATA_ANNOTATION | NONE | Linkage specification: `extern "C"` |

## Preprocessor Directive Tokens

Preprocessor directive keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `#include` | METADATA_DIRECTIVE | NONE | Preprocessor directive keywords |
| `#define` | METADATA_DIRECTIVE | NONE |  |
| `#ifdef` | METADATA_DIRECTIVE | NONE |  |
| `#ifndef` | METADATA_DIRECTIVE | NONE |  |
| `#if` | METADATA_DIRECTIVE | NONE |  |
| `#else` | METADATA_DIRECTIVE | NONE |  |
| `#elif` | METADATA_DIRECTIVE | NONE |  |
| `#endif` | METADATA_DIRECTIVE | NONE |  |
| `#undef` | METADATA_DIRECTIVE | NONE |  |
| `#pragma` | METADATA_DIRECTIVE | NONE |  |

## Parser Errors

Error nodes from parsing

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NONE | Error nodes from parsing |

---

*Generated from `cpp_types.def`*
