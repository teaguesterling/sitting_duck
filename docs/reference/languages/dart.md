# Dart Node Types

> Dart language node type mappings for AST semantic extraction

## Language Characteristics

- **Client-optimized**: Designed for fast apps on any platform
- **Sound null safety**: Compile-time null checking (Dart 2.12+)
- **Strong typing**: Static type system with type inference
- **Async/await**: First-class asynchronous programming
- **Isolates**: Concurrent execution without shared memory
- **Mixins**: Code reuse across class hierarchies
- **Extensions**: Add functionality to existing types
- **Records**: Immutable composite values (Dart 3.0+)
- **Patterns**: Destructuring and pattern matching (Dart 3.0+)
- **Flutter**: Primary language for cross-platform UI development

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

- [Program Structure](#program-structure)
- [Class and Type Definitions](#class-and-type-definitions)
- [Function and Method Definitions](#function-and-method-definitions)
- [Constructor Definitions](#constructor-definitions)
- [Parameters](#parameters)
- [Variable Declarations](#variable-declarations)
- [Import/Export](#import-export)
- [Expressions](#expressions)
- [Function Calls and Selectors](#function-calls-and-selectors)
- [Control Flow](#control-flow)
- [Error Handling](#error-handling)
- [Async/Await](#async-await)
- [Blocks](#blocks)
- [Pattern Matching](#pattern-matching)
- [Types](#types)
- [Inheritance](#inheritance)
- [Initializers](#initializers)
- [Assertions](#assertions)
- [Literals](#literals)
- [Collections](#collections)
- [Identifiers](#identifiers)
- [Annotations and Metadata](#annotations-and-metadata)
- [Comments](#comments)
- [Operators](#operators)
- [Builtin Keywords](#builtin-keywords)
- [Modifiers](#modifiers)
- [Special Keywords](#special-keywords)
- [URI and Configuration](#uri-and-configuration)
- [Parser Error Handling](#parser-error-handling)

## Program Structure

Top-level file organization

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `program` | DEFINITION_MODULE | NONE | Program root - top-level compilation unit |

## Class and Type Definitions

Dart class hierarchy constructs

Dart class system: - `class Name { }` - regular class - `abstract class Name { }` - abstract class - `mixin Name { }` - mixin for code reuse - `enum Name { }` - enumeration - `extension Name on Type { }` - extend existing types - `typedef Name = Type` - type alias

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_definition` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Class definition - `class Name { }` |
| `class_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Class body - contents of class |
| `enum_declaration` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER | Enum declaration - `enum Name { A, B, C }` |
| `enum_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Enum body - enum constants and members |
| `enum_constant` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Enum constant - individual enum value |
| `mixin_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | Mixin declaration - `mixin Name { }` |
| `extension_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Extension declaration - `extension Name on Type { }` |
| `extension_type_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Extension type declaration - Dart 3.0+ |
| `extension_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Extension body - extension methods |
| `type_alias` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Type alias - `typedef Name = Type` |

## Function and Method Definitions

Dart function declarations

Dart function features: - `ReturnType name(params) { }` - standard function - `ReturnType name(params) => expr;` - arrow function - `(params) { }` - anonymous function - `(params) => expr` - lambda expression - Optional parameters: `[type param = default]` - Named parameters: `{type param = default}` Note: In Dart's tree-sitter grammar, signatures and bodies are siblings

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_signature` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Function signature - function declaration without body |
| `function_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Function body - function implementation |
| `function_expression` | DEFINITION_FUNCTION | Function::LAMBDA | NONE | Function expression - anonymous function |
| `function_expression_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Function expression body |
| `lambda_expression` | DEFINITION_FUNCTION | Function::LAMBDA | NONE | Lambda expression - `(x) => expr` |
| `method_signature` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Method signature - class method declaration |
| `getter_signature` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Getter signature - `get name => value` |
| `setter_signature` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Setter signature - `set name(value) { }` |
| `operator_signature` | DEFINITION_FUNCTION | Function::REGULAR | NONE | Operator signature - operator overload |
| `local_function_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Local function declaration - nested function |

## Constructor Definitions

Dart constructor variants

Dart constructor types: - `ClassName(params)` - generative constructor - `ClassName.named(params)` - named constructor - `const ClassName()` - const constructor - `factory ClassName()` - factory constructor

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `constructor_signature` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER | Constructor signature - regular constructor |
| `constant_constructor_signature` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER | Constant constructor signature - `const` constructor |
| `factory_constructor_signature` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER | Factory constructor signature - `factory` constructor |
| `redirecting_factory_constructor_signature` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER | Redirecting factory constructor |
| `constructor_invocation` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET | Constructor invocation - `new ClassName()` or `ClassName()` |
| `constructor_param` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Constructor parameter |
| `constructor_tearoff` | COMPUTATION_ACCESS | NONE | Constructor tearoff - reference to constructor |

## Parameters

Function parameter declarations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `formal_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Formal parameter - regular parameter |
| `formal_parameter_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Formal parameter list - `(params)` |
| `optional_formal_parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Optional formal parameters - `[optional]` or `{named}` |
| `super_formal_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Super formal parameter - `super.field` |

## Variable Declarations

Variable and field declarations

Dart variable types: - `var name = value` - inferred type - `Type name = value` - explicit type - `final name = value` - single assignment - `const name = value` - compile-time constant - `late Type name` - lazily initialized

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable declaration - `var x = value` |
| `local_variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Local variable declaration |
| `initialized_variable_definition` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Initialized variable definition |
| `initialized_identifier` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Initialized identifier - variable with initializer |
| `initialized_identifier_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Initialized identifier list |
| `static_final_declaration` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER | Static final declaration - `static final name = value` |
| `static_final_declaration_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Static final declaration list |
| `declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Declaration - general declaration |
| `pattern_variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Pattern variable declaration - Dart 3.0+ patterns |

## Import/Export

Library and import directives

Dart import features: - `import 'package:name/name.dart'` - `import 'path.dart' as alias` - `import 'path.dart' show name` - `import 'path.dart' hide name` - `export 'path.dart'` - `part 'path.dart'` / `part of 'path.dart'`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `import_or_export` | EXTERNAL_IMPORT | Import::MODULE | NONE | Import or export statement |
| `import_specification` | EXTERNAL_IMPORT | Import::MODULE | NONE | Import specification |
| `library_import` | EXTERNAL_IMPORT | Import::MODULE | NONE | Library import - `import 'uri'` |
| `library_export` | EXTERNAL_EXPORT | NONE | Library export - `export 'uri'` |
| `library_name` | DEFINITION_MODULE | FIND_IDENTIFIER | Library name - `library name;` |
| `part_directive` | EXTERNAL_IMPORT | Import::MODULE | NONE | Part directive - `part 'file.dart'` |
| `part_of_directive` | EXTERNAL_IMPORT | Import::MODULE | NONE | Part of directive - `part of 'main.dart'` |
| `uri` | LITERAL_STRING | NODE_TEXT | URI literal - `'package:name/name.dart'` |
| `configurable_uri` | LITERAL_STRING | NONE | Configurable URI - conditional imports |
| `combinator` | EXTERNAL_IMPORT | NONE | Combinator - `show` or `hide` |

## Expressions

Dart expression types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Assignment expression - `x = value` |
| `assignment_expression_without_cascade` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Assignment without cascade |
| `assignable_expression` | COMPUTATION_ACCESS | NONE | Assignable expression - left-hand side of assignment |
| `conditional_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE | Conditional expression - `cond ? a : b` |
| `if_null_expression` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If-null expression - `a ?? b` |
| `logical_or_expression` | OPERATOR_LOGICAL | NONE | Logical OR expression - `a || b` |
| `logical_and_expression` | OPERATOR_LOGICAL | NONE | Logical AND expression - `a && b` |
| `equality_expression` | OPERATOR_COMPARISON | NONE | Equality expression - `a == b`, `a != b` |
| `relational_expression` | OPERATOR_COMPARISON | NONE | Relational expression - `a < b`, `a >= b` |
| `bitwise_or_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Bitwise OR expression - `a | b` |
| `bitwise_xor_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Bitwise XOR expression - `a ^ b` |
| `bitwise_and_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Bitwise AND expression - `a & b` |
| `shift_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Shift expression - `a << b`, `a >> b` |
| `additive_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Additive expression - `a + b`, `a - b` |
| `multiplicative_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Multiplicative expression - `a * b`, `a / b` |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary expression - `-a`, `!a` |
| `postfix_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Postfix expression - `a++`, `a--` |
| `type_cast_expression` | TYPE_REFERENCE | NONE | Type cast expression - `expr as Type` |
| `type_test_expression` | TYPE_REFERENCE | NONE | Type test expression - `expr is Type` |
| `new_expression` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET | New expression - `new ClassName()` |
| `const_object_expression` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET | Const object expression - `const ClassName()` |
| `parenthesized_expression` | ORGANIZATION_BLOCK | NONE | Parenthesized expression - `(expr)` |
| `expression_statement` | ORGANIZATION_BLOCK | NONE | Expression statement |

## Function Calls and Selectors

Method invocations and member access

Dart call syntax: - `function(args)` - `obj.method(args)` - `obj?.method(args)` - null-aware - `obj..method()..other()` - cascade

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `arguments` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Arguments - `(arg1, arg2)` |
| `argument` | ORGANIZATION_BLOCK | NONE | Argument - single argument |
| `argument_part` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Argument part |
| `named_argument` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Named argument - `name: value` |
| `selector` | COMPUTATION_ACCESS | NONE | Selector - `.member` or `[index]` |
| `cascade_section` | COMPUTATION_ACCESS | NONE | Cascade section - `..method()` |
| `cascade_selector` | COMPUTATION_ACCESS | NONE | Cascade selector |
| `index_selector` | COMPUTATION_ACCESS | NONE | Index selector - `[index]` |
| `unconditional_assignable_selector` | COMPUTATION_ACCESS | NONE | Unconditional assignable selector - `.member` |
| `conditional_assignable_selector` | COMPUTATION_ACCESS | NONE | Conditional assignable selector - `?.member` |

## Control Flow

Conditionals and branching

Dart control flow: - `if (cond) { } else { }` - `switch (value) { case: ... }` - Switch expressions (Dart 3.0+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement - `if (cond) { }` |
| `if_element` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If element - if in collection |
| `for_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | For statement - `for (var x in items) { }` |
| `for_element` | FLOW_LOOP | Loop::ITERATOR | NONE | For element - for in collection |
| `for_loop_parts` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | For loop parts |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While statement - `while (cond) { }` |
| `do_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Do statement - `do { } while (cond)` |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement |
| `switch_block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Switch block |
| `switch_statement_case` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement case |
| `switch_statement_default` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement default |
| `switch_expression` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch expression - Dart 3.0+ |
| `switch_expression_case` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch expression case |
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE | Break statement |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE | Continue statement |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE | Return statement |

## Error Handling

Exception handling constructs

Dart error handling: - `try { } on Type catch (e) { } finally { }` - `throw exception` - `rethrow`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_statement` | ERROR_TRY | NONE | Try statement |
| `catch_clause` | ERROR_CATCH | NONE | Catch clause - `catch (e) { }` or `on Type catch (e) { }` |
| `catch_parameters` | ORGANIZATION_LIST | NONE | Catch parameters |
| `finally_clause` | ERROR_FINALLY | NONE | Finally clause |
| `throw_expression` | ERROR_THROW | NONE | Throw expression - `throw exception` |
| `throw_expression_without_cascade` | ERROR_THROW | NONE | Throw expression without cascade |
| `rethrow_expression` | ERROR_THROW | NONE | Rethrow expression - `rethrow` |

## Async/Await

Asynchronous programming support

Dart async features: - `async` / `async*` function modifiers - `await` for awaiting futures - `yield` / `yield*` for generators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `await_expression` | FLOW_SYNC | NONE | Await expression - `await future` |
| `yield_statement` | FLOW_SYNC | NONE | Yield statement - `yield value` |
| `yield_each_statement` | FLOW_SYNC | NONE | Yield each statement - `yield* stream` |

## Blocks

Code blocks

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Block - `{ statements }` |

## Pattern Matching

Dart 3.0+ pattern matching

Pattern types: - Constant patterns - Variable patterns - List/map/record patterns - Object patterns - Cast/null-check/null-assert patterns

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `constant_pattern` | LITERAL_STRUCTURED | NONE | Constant pattern |
| `variable_pattern` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Variable pattern |
| `list_pattern` | LITERAL_STRUCTURED | NONE | List pattern - `[a, b, c]` |
| `map_pattern` | LITERAL_STRUCTURED | NONE | Map pattern - `{'key': value}` |
| `record_pattern` | LITERAL_STRUCTURED | NONE | Record pattern - `(a, b: c)` |
| `object_pattern` | LITERAL_STRUCTURED | NONE | Object pattern - `ClassName(field: pattern)` |
| `cast_pattern` | TYPE_REFERENCE | NONE | Cast pattern - `pattern as Type` |
| `null_check_pattern` | FLOW_CONDITIONAL | NONE | Null check pattern - `pattern?` |
| `null_assert_pattern` | FLOW_CONDITIONAL | NONE | Null assert pattern - `pattern!` |
| `rest_pattern` | LITERAL_STRUCTURED | NONE | Rest pattern - `...` |
| `pattern_assignment` | OPERATOR_ASSIGNMENT | NONE | Pattern assignment |

## Types

Type system constructs

Dart type system: - Nullable types: `Type?` - Generic types: `List<Type>` - Function types: `Type Function(Type)` - Record types: `(Type, {Type name})` (Dart 3.0+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_identifier` | TYPE_REFERENCE | NODE_TEXT | Type identifier - type name |
| `type_arguments` | TYPE_GENERIC | NONE | Type arguments - `<Type1, Type2>` |
| `type_parameters` | TYPE_GENERIC | NONE | Type parameters - generic type parameters |
| `type_parameter` | TYPE_GENERIC | FIND_IDENTIFIER | Type parameter - single type parameter |
| `type_bound` | TYPE_REFERENCE | NONE | Type bound - `extends Type` |
| `type_alias` | TYPE_REFERENCE | FIND_IDENTIFIER | Type alias declaration |
| `type_test` | TYPE_REFERENCE | NONE | Type test - `is Type` |
| `type_cast` | TYPE_REFERENCE | NONE | Type cast - `as Type` |
| `void_type` | TYPE_PRIMITIVE | NODE_TEXT | Void type - `void` |
| `function_type` | TYPE_COMPOSITE | NONE | Function type - `Type Function(Type)` |
| `record_type` | TYPE_COMPOSITE | NONE | Record type - `(Type, Type)` (Dart 3.0+) |
| `record_type_field` | TYPE_REFERENCE | NONE | Record type field |
| `record_type_named_field` | TYPE_REFERENCE | NONE | Record type named field |
| `nullable_type` | TYPE_REFERENCE | NONE | Nullable type - `Type?` |
| `inferred_type` | TYPE_REFERENCE | NODE_TEXT | Inferred type - `var` |
| `typed_identifier` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Typed identifier |
| `parameter_type_list` | ORGANIZATION_LIST | NONE | Parameter type list |
| `normal_parameter_type` | TYPE_REFERENCE | NONE | Normal parameter type |
| `optional_parameter_types` | ORGANIZATION_LIST | NONE | Optional parameter types |
| `optional_positional_parameter_types` | ORGANIZATION_LIST | NONE | Optional positional parameter types |
| `named_parameter_types` | ORGANIZATION_LIST | NONE | Named parameter types |

## Inheritance

Class inheritance constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `superclass` | TYPE_REFERENCE | NONE | Superclass - `extends Parent` |
| `mixins` | TYPE_REFERENCE | NONE | Mixins - `with Mixin1, Mixin2` |
| `mixin_application` | TYPE_REFERENCE | NONE | Mixin application - `Base with Mixin` |
| `mixin_application_class` | DEFINITION_CLASS | FIND_IDENTIFIER | Mixin application class |
| `interfaces` | TYPE_REFERENCE | NONE | Interfaces - `implements Interface1, Interface2` |
| `representation_declaration` | TYPE_REFERENCE | NONE | Representation declaration - extension type representation |

## Initializers

Constructor initializer lists

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `initializers` | ORGANIZATION_LIST | NONE | Initializers - `: field = value, super()` |
| `initializer_list_entry` | ORGANIZATION_BLOCK | NONE | Initializer list entry |
| `field_initializer` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Field initializer - `this.field = value` |
| `redirection` | ORGANIZATION_BLOCK | NONE | Redirection - `: this()` or `: this.named()` |

## Assertions

Assert statements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assert_statement` | ERROR_THROW | NONE | Assert statement - `assert(condition)` |
| `assertion` | ERROR_THROW | NONE | Assertion expression |
| `assertion_arguments` | ORGANIZATION_LIST | NONE | Assertion arguments |

## Literals

Dart literal values

Dart literals: - `null` - null value - `true`, `false` - boolean - `42`, `0xFF` - integer - `3.14` - double - `'string'`, `"string"` - string - `#symbol` - symbol

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `null_literal` | LITERAL_ATOMIC | NODE_TEXT | Null literal - `null` |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean true |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean false |
| `decimal_integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Decimal integer literal |
| `hex_integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Hex integer literal - `0xFF` |
| `decimal_floating_point_literal` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT | Decimal floating point literal |
| `string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal |
| `symbol_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Symbol literal - `#symbol` |
| `escape_sequence` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Escape sequence - `\n`, `\t`, etc. |
| `template_substitution` | LITERAL_STRING | String::TEMPLATE | NONE | Template substitution - `${expr}` in string |

## Collections

Collection literals

Dart collections: - `[1, 2, 3]` - list - `{1, 2, 3}` or `{'key': value}` - set or map - `(1, 2)` - record (Dart 3.0+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `list_literal` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | List literal - `[items]` |
| `set_or_map_literal` | LITERAL_STRUCTURED | Structured::MAPPING | NONE | Set or map literal - `{items}` or `{key: value}` |
| `record_literal` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Record literal - `(value1, name: value2)` (Dart 3.0+) |
| `record_field` | DEFINITION_VARIABLE | Variable::FIELD | NONE | Record field |
| `pair` | DEFINITION_VARIABLE | Variable::FIELD | NONE | Pair - map entry `key: value` |
| `spread_element` | ORGANIZATION_BLOCK | NONE | Spread element - `...list` |

## Identifiers

Names and identifiers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - simple name |
| `identifier_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Identifier list |
| `identifier_dollar_escaped` | NAME_IDENTIFIER | NODE_TEXT | Identifier with dollar escaped |
| `dotted_identifier_list` | NAME_IDENTIFIER | NODE_TEXT | Dotted identifier list |
| `scoped_identifier` | NAME_IDENTIFIER | NODE_TEXT | Scoped identifier - `prefix.name` |
| `qualified` | NAME_IDENTIFIER | NODE_TEXT | Qualified identifier |

## Annotations and Metadata

Dart annotations and metadata

Dart annotations: - `@override` - `@deprecated` - `@CustomAnnotation()`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `annotation` | METADATA_ANNOTATION | FIND_IDENTIFIER | Annotation - `@annotation` |
| `label` | NAME_IDENTIFIER | FIND_IDENTIFIER | Label - `labelName:` |
| `script_tag` | METADATA_DIRECTIVE | NONE | Script tag - `#!/usr/bin/env dart` |

## Comments

Documentation and comments

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment |
| `documentation_comment` | METADATA_COMMENT | NONE | Documentation comment - `/// doc` |

## Operators

Dart operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `equality_operator` | OPERATOR_COMPARISON | NODE_TEXT | Equality operator - `==`, `!=` |
| `relational_operator` | OPERATOR_COMPARISON | NODE_TEXT | Relational operator - `<`, `>`, `<=`, `>=` |
| `additive_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Additive operator - `+`, `-` |
| `multiplicative_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Multiplicative operator - `*`, `/`, `~/`, `%` |
| `shift_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Shift operator - `<<`, `>>`, `>>>` |
| `bitwise_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Bitwise operator - `&`, `|`, `^` |
| `logical_or_operator` | OPERATOR_LOGICAL | NODE_TEXT | Logical OR operator - `||` |
| `logical_and_operator` | OPERATOR_LOGICAL | NODE_TEXT | Logical AND operator - `&&` |
| `prefix_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Prefix operator - `!`, `-`, `~` |
| `postfix_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Postfix operator - `!` (null assertion) |
| `increment_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Increment operator - `++`, `--` |
| `minus_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Minus operator - `-` |
| `negation_operator` | OPERATOR_LOGICAL | NODE_TEXT | Negation operator - `!` |
| `tilde_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Tilde operator - `~` |
| `binary_operator` | OPERATOR_ARITHMETIC | NODE_TEXT | Binary operator |
| `as_operator` | TYPE_REFERENCE | NODE_TEXT | As operator - `as` type cast |
| `is_operator` | TYPE_REFERENCE | NODE_TEXT | Is operator - `is` type test |

## Builtin Keywords

Dart language keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `const_builtin` | METADATA_ANNOTATION | NODE_TEXT | Const builtin - `const` |
| `final_builtin` | METADATA_ANNOTATION | NODE_TEXT | Final builtin - `final` |
| `break_builtin` | FLOW_JUMP | NODE_TEXT | Break builtin - `break` |
| `assert_builtin` | ERROR_THROW | NODE_TEXT | Assert builtin - `assert` |
| `case_builtin` | FLOW_CONDITIONAL | NODE_TEXT | Case builtin - `case` |
| `rethrow_builtin` | ERROR_THROW | NODE_TEXT | Rethrow builtin - `rethrow` |
| `part_of_builtin` | EXTERNAL_IMPORT | NODE_TEXT | Part of builtin - `part of` |

## Modifiers

Class and member modifiers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `abstract` | METADATA_ANNOTATION | NODE_TEXT | Abstract modifier - `abstract` |
| `base` | METADATA_ANNOTATION | NODE_TEXT | Base modifier - `base` (Dart 3.0+) |
| `sealed` | METADATA_ANNOTATION | NODE_TEXT | Sealed modifier - `sealed` (Dart 3.0+) |
| `interface` | METADATA_ANNOTATION | NODE_TEXT | Interface modifier - `interface` (Dart 3.0+) |
| `mixin` | METADATA_ANNOTATION | NODE_TEXT | Mixin modifier - `mixin` |

## Special Keywords

Special reference keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `this` | NAME_IDENTIFIER | NODE_TEXT | This keyword - `this` |
| `super` | NAME_IDENTIFIER | NODE_TEXT | Super keyword - `super` |

## URI and Configuration

Conditional imports

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `uri_test` | LITERAL_STRING | NONE | URI test - conditional import test |
| `configuration_uri` | FLOW_CONDITIONAL | NONE | Configuration URI |
| `configuration_uri_condition` | FLOW_CONDITIONAL | NONE | Configuration URI condition |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `dart_types.def`*
