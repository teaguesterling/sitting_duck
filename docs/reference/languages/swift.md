# Swift Node Types

> Swift language node type mappings for AST semantic extraction

## Language Characteristics

- **Type safety**: Strong static typing with type inference
- **Optionals**: Explicit nil handling via `T?` optional types
- **Value types**: Structs and enums are value types (copied)
- **Reference types**: Classes are reference types (shared)
- **Protocols**: Interface-like contracts with protocol extensions
- **Extensions**: Add functionality to existing types
- **Closures**: First-class functions with capture semantics
- **Generics**: Parametric polymorphism with constraints
- **Actors**: Concurrency-safe reference types (Swift 5.5+)
- **Async/await**: Structured concurrency model

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
- [Import Statements](#import-statements)
- [Function Definitions](#function-definitions)
- [Class and Type Definitions](#class-and-type-definitions)
- [Variable and Property Declarations](#variable-and-property-declarations)
- [Type Definitions](#type-definitions)
- [Control Flow](#control-flow)
- [Loop Constructs](#loop-constructs)
- [Jump Statements](#jump-statements)
- [Expressions](#expressions)
- [Member Access](#member-access)
- [Literals](#literals)
- [Collection Literals](#collection-literals)
- [Identifiers and Types](#identifiers-and-types)
- [Async/Await and Concurrency](#async-await-and-concurrency)
- [Error Handling](#error-handling)
- [Closures and Lambdas](#closures-and-lambdas)
- [Attributes and Modifiers](#attributes-and-modifiers)
- [Comments](#comments)
- [Operators](#operators)
- [Code Organization](#code-organization)
- [Punctuation and Delimiters](#punctuation-and-delimiters)
- [Keywords](#keywords)
- [Generics and Protocols](#generics-and-protocols)
- [Property Wrappers and Result Builders](#property-wrappers-and-result-builders)
- [Parser Error Handling](#parser-error-handling)

## Program Structure

Top-level source file structure

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `source_file` | DEFINITION_MODULE | NONE | Source file root - represents the entire Swift file |

## Import Statements

Module import declarations

Swift import types: - `import Module` - import entire module - `import Module.Submodule` - import specific submodule - `import func Module.function` - import specific symbol

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `import_declaration` | EXTERNAL_IMPORT | Import::MODULE | FIND_IDENTIFIER | Import declaration - `import Foundation` |

## Function Definitions

Swift function and method declarations

Swift function features: - `func name(param: Type) -> ReturnType` - External/internal parameter names: `func foo(external internal: Type)` - Default parameters: `func foo(x: Int = 0)` - Variadic parameters: `func foo(_ items: Int...)` - inout parameters: `func foo(_ x: inout Int)` - Throwing functions: `func foo() throws` - Async functions: `func foo() async`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Function::REGULAR | CUSTOM | Function declaration - `func name(params) -> Type { }` |
| `init_declaration` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | CUSTOM | Initializer - `init(params) { }` |
| `deinit_declaration` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | CUSTOM | Deinitializer - `deinit { }` (destructor) |
| `subscript_declaration` | DEFINITION_FUNCTION | Function::REGULAR | CUSTOM | Subscript declaration - `subscript(index: Int) -> Element` |

## Class and Type Definitions

Swift type declarations

Swift type kinds: - `class` - reference type with inheritance - `struct` - value type, no inheritance - `enum` - value type with associated values - `protocol` - interface/trait definition - `extension` - add members to existing types - `actor` - concurrency-safe reference type (Swift 5.5+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Class declaration - reference type with inheritance |
| `struct_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Struct declaration - value type, copied on assignment |
| `actor_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Actor declaration - concurrency-safe reference type |
| `enum_declaration` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER | Enum declaration - value type with cases |
| `protocol_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | Protocol declaration - interface/trait |
| `extension_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Extension - adds members to existing type |

## Variable and Property Declarations

Properties and variables

Swift variable declarations: - `let` - immutable (constant) - `var` - mutable - Computed properties with get/set - Property observers: willSet, didSet - Lazy properties: `lazy var`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `property_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Property declaration - class/struct property |
| `variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable declaration - local or global `let`/`var` |
| `parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Function parameter |

## Type Definitions

Type aliases and associated types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `typealias_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Type alias - `typealias Name = ExistingType` |
| `associatedtype_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | Associated type - protocol type placeholder |

## Control Flow

Conditionals and branching

Swift control flow features: - `if let` for optional binding - `guard let` for early exit with optional binding - `switch` with pattern matching (must be exhaustive) - No implicit fallthrough in switch cases

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement - `if condition { }` |
| `guard_statement` | FLOW_CONDITIONAL | Conditional::GUARD | NONE | Guard statement - early exit `guard condition else { return }` |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement - exhaustive pattern matching |
| `case_item` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Case item in switch |
| `default_case` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Default case in switch |

## Loop Constructs

Iteration mechanisms

Swift loops: - `for item in collection` - iterate over Sequence - `for i in 0..<10` - range iteration - `while condition` - condition-based - `repeat { } while condition` - post-condition

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | For-in loop - `for item in collection` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While loop - `while condition { }` |
| `repeat_while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Repeat-while loop - `repeat { } while condition` |
| `do_statement` | FLOW_SYNC | NONE | Do block - `do { }` for scoping or error handling |
| `defer_statement` | FLOW_SYNC | NONE | Defer statement - executes when scope exits |

## Jump Statements

Control flow transfer

Swift jump statements: - `break` - exit loop or switch - `continue` - skip to next iteration - `fallthrough` - explicit fallthrough in switch - `return` - exit function with value - `throw` - raise error

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE | Break statement - exits loop or switch |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE | Continue statement - skips to next iteration |
| `fallthrough_statement` | FLOW_JUMP | Jump::CONTINUE | NONE | Fallthrough statement - explicit switch case fallthrough |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE | Return statement - exits function with value |
| `throw_statement` | ERROR_THROW | NONE | Throw statement - raises error |

## Expressions

Operators and expression types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function call - `function(args)` |
| `postfix_expression` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET | Postfix expression - method calls, subscripts |
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Binary expression - arithmetic, comparison, logical |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary expression - `-x`, `!x` |
| `ternary_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE | Ternary expression - `condition ? true : false` |
| `assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Assignment - `x = value` |
| `compound_assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::COMPOUND | NONE | Compound assignment - `x += value` |

## Member Access

Property and element access

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `navigation_expression` | COMPUTATION_ACCESS | FIND_IDENTIFIER | Navigation expression - `obj.property` |
| `member_access_expression` | COMPUTATION_ACCESS | FIND_IDENTIFIER | Member access - dot access |
| `subscript_expression` | COMPUTATION_ACCESS | NONE | Subscript expression - `arr[index]` |

## Literals

Swift literal values

Swift literals: - Integers: `42`, `0xFF`, `0b1010`, `0o755` - Floats: `3.14`, `1.0e10` - Strings: `"string"`, multiline `"""..."""` - Characters: implicit in String - Boolean: `true`, `false` - Nil: `nil` (typed null)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Integer literal |
| `real_literal` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT | Float literal |
| `boolean_literal` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true` or `false` |
| `string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal - `"text"` |
| `multiline_string_literal` | LITERAL_STRING | String::RAW | NODE_TEXT | Multiline string literal - `"""..."""` |
| `character_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Character literal |
| `nil_literal` | LITERAL_ATOMIC | NODE_TEXT | Nil literal - typed null |

## Collection Literals

Array, dictionary, and tuple literals

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `array_literal` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Array literal - `[1, 2, 3]` |
| `dictionary_literal` | LITERAL_STRUCTURED | Structured::MAPPING | NONE | Dictionary literal - `["key": value]` |
| `tuple_expression` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Tuple expression - `(a, b, c)` |

## Identifiers and Types

Names and type references

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `simple_identifier` | NAME_IDENTIFIER | NODE_TEXT | Simple identifier - name |
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier node |
| `type_identifier` | TYPE_PRIMITIVE | NODE_TEXT | Type identifier - type name |
| `user_type` | TYPE_COMPOSITE | NODE_TEXT | User-defined type |
| `optional_type` | TYPE_REFERENCE | NODE_TEXT | Optional type - `Type?` |
| `array_type` | TYPE_COMPOSITE | NODE_TEXT | Array type - `[Element]` |
| `dictionary_type` | TYPE_COMPOSITE | NODE_TEXT | Dictionary type - `[Key: Value]` |
| `function_type` | TYPE_COMPOSITE | NODE_TEXT | Function type - `(Params) -> Return` |
| `tuple_type` | TYPE_COMPOSITE | NODE_TEXT | Tuple type - `(A, B, C)` |

## Async/Await and Concurrency

Swift structured concurrency (5.5+)

Swift concurrency features: - `async` functions that can suspend - `await` to wait for async results - `actor` types for data isolation - Task groups for structured concurrency

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `await_expression` | FLOW_SYNC | NONE | Await expression - `await asyncCall()` |
| `async_keyword` | FLOW_SYNC | NODE_TEXT | Async keyword |

## Error Handling

Swift error handling constructs

Swift error handling: - `throw` to raise errors (must conform to Error) - `try` to call throwing function - `try?` returns optional (nil on error) - `try!` force unwraps (crashes on error) - `do { } catch { }` for handling

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_expression` | ERROR_TRY | FIND_IDENTIFIER | Try expression - `try throwingFunction()` |
| `catch_clause` | ERROR_CATCH | NONE | Catch clause - handles error |

## Closures and Lambdas

Swift closure expressions

Swift closures: - `{ (params) -> Return in body }` - Trailing closure syntax - Shorthand: `$0`, `$1` for arguments - Capture lists: `[weak self, unowned delegate]`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `lambda_literal` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Lambda/closure literal |
| `closure_expression` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Closure expression |

## Attributes and Modifiers

Annotations and access control

Swift attributes: - `@available` for API availability - `@escaping` for closure parameters - `@objc` for Objective-C interop - Property wrappers: `@Published`, `@State`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `attribute` | METADATA_ANNOTATION | NODE_TEXT | Attribute - `@attribute` |
| `availability_condition` | METADATA_ANNOTATION | NODE_TEXT | Availability condition - `#available(iOS 15, *)` |
| `modifiers` | METADATA_ANNOTATION | NODE_TEXT | Modifier list |
| `visibility_modifier` | METADATA_ANNOTATION | NODE_TEXT | Visibility modifier - `public`, `private`, etc. |
| `mutation_modifier` | METADATA_ANNOTATION | NODE_TEXT | Mutation modifier - `mutating`, `nonmutating` |

## Comments

Documentation and annotation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Single-line comment - `// comment` |
| `multiline_comment` | METADATA_COMMENT | NODE_TEXT | Multi-line comment - `/* comment */` |

## Operators

Swift custom operators

Swift supports custom operator definitions: - prefix: `prefix func -(x: T)` - infix: `infix func +(lhs: T, rhs: T)` - postfix: `postfix func ++(x: inout T)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `custom_operator` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NODE_TEXT | Custom operator |
| `prefix_operator` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NODE_TEXT | Prefix operator |
| `postfix_operator` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NODE_TEXT | Postfix operator |
| `infix_operator` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NODE_TEXT | Infix operator |

## Code Organization

Blocks and structural elements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `statements` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Statements block |
| `code_block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Code block - `{ }` |
| `capture_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Capture list - `[weak self]` in closures |

## Punctuation and Delimiters

Syntactic markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NODE_TEXT | Syntactic markers |
| `)` | PARSER_DELIMITER | NODE_TEXT |  |
| `{` | PARSER_DELIMITER | NODE_TEXT |  |
| `}` | PARSER_DELIMITER | NODE_TEXT |  |
| `[` | PARSER_DELIMITER | NODE_TEXT |  |
| `]` | PARSER_DELIMITER | NODE_TEXT |  |
| `<` | PARSER_DELIMITER | NODE_TEXT |  |
| `>` | PARSER_DELIMITER | NODE_TEXT |  |
| `;` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `,` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `.` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `:` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `?` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `!` | PARSER_PUNCTUATION | NODE_TEXT |  |

## Keywords

Swift reserved words

Swift has declaration keywords, statement keywords, expression keywords, and context-sensitive keywords.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `associatedtype` | DEFINITION_CLASS | NODE_TEXT | Swift reserved words |
| `class` | DEFINITION_CLASS | NODE_TEXT |  |
| `deinit` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `enum` | DEFINITION_CLASS | NODE_TEXT |  |
| `extension` | DEFINITION_CLASS | NODE_TEXT |  |
| `fileprivate` | METADATA_ANNOTATION | NODE_TEXT |  |
| `func` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `import` | EXTERNAL_IMPORT | NODE_TEXT |  |
| `init` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `inout` | METADATA_ANNOTATION | NODE_TEXT |  |
| `internal` | METADATA_ANNOTATION | NODE_TEXT |  |
| `let` | DEFINITION_VARIABLE | NODE_TEXT |  |
| `open` | METADATA_ANNOTATION | NODE_TEXT |  |
| `operator` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `private` | METADATA_ANNOTATION | NODE_TEXT |  |
| `protocol` | DEFINITION_CLASS | NODE_TEXT |  |
| `public` | METADATA_ANNOTATION | NODE_TEXT |  |
| `static` | METADATA_ANNOTATION | NODE_TEXT |  |
| `struct` | DEFINITION_CLASS | NODE_TEXT |  |
| `subscript` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `typealias` | DEFINITION_CLASS | NODE_TEXT |  |
| `var` | DEFINITION_VARIABLE | NODE_TEXT |  |

## Generics and Protocols

Generic type parameters and constraints

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `generic_parameter_clause` | TYPE_GENERIC | NONE | Generic parameter clause - `<T, U>` |
| `generic_where_clause` | TYPE_GENERIC | NONE | Generic where clause - `where T: Equatable` |
| `conformance_requirement` | TYPE_GENERIC | NONE | Conformance requirement - `T: Protocol` |
| `same_type_requirement` | TYPE_GENERIC | NONE | Same type requirement - `T == U` |

## Property Wrappers and Result Builders

Swift metaprogramming features

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `property_wrapper_type` | METADATA_ANNOTATION | NODE_TEXT | Property wrapper type - `@Published`, `@State` |
| `result_builder` | METADATA_ANNOTATION | NODE_TEXT | Result builder - `@ViewBuilder` |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

## Other Node Types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `break` | FLOW_JUMP | NODE_TEXT |  |
| `case` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `continue` | FLOW_JUMP | NODE_TEXT |  |
| `default` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `defer` | FLOW_SYNC | NODE_TEXT |  |
| `do` | FLOW_SYNC | NODE_TEXT |  |
| `else` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `fallthrough` | FLOW_JUMP | NODE_TEXT |  |
| `for` | FLOW_LOOP | NODE_TEXT |  |
| `guard` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `if` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `in` | FLOW_LOOP | NODE_TEXT |  |
| `repeat` | FLOW_LOOP | NODE_TEXT |  |
| `return` | FLOW_JUMP | NODE_TEXT |  |
| `switch` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `where` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `while` | FLOW_LOOP | NODE_TEXT |  |
| `as` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `catch` | ERROR_CATCH | NODE_TEXT |  |
| `false` | LITERAL_ATOMIC | NODE_TEXT |  |
| `is` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `nil` | LITERAL_ATOMIC | NODE_TEXT |  |
| `rethrows` | ERROR_THROW | NODE_TEXT |  |
| `super` | NAME_IDENTIFIER | NODE_TEXT |  |
| `self` | NAME_IDENTIFIER | NODE_TEXT |  |
| `Self` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `throw` | ERROR_THROW | NODE_TEXT |  |
| `throws` | ERROR_THROW | NODE_TEXT |  |
| `true` | LITERAL_ATOMIC | NODE_TEXT |  |
| `try` | ERROR_TRY | NODE_TEXT |  |
| `await` | FLOW_SYNC | NODE_TEXT |  |
| `async` | FLOW_SYNC | NODE_TEXT |  |
| `actor` | DEFINITION_CLASS | NODE_TEXT |  |

---

*Generated from `swift_types.def`*
