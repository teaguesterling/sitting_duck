# Rust Node Types

> Rust language node type mappings for AST semantic extraction

## Language Characteristics

Rust has several unique features that affect AST mapping:

- **Ownership system**: Variables are immutable by default; `mut` keyword for mutability
- **Pattern matching**: First-class `match` expressions with exhaustive checking
- **Traits**: Abstract interfaces similar to interfaces/protocols, use ABSTRACT refinement
- **Impl blocks**: Separate from struct/enum definitions, implement methods or traits
- **Closures**: Use `|args|` syntax, capture environment by reference or move
- **Macros**: Both declarative (`macro_rules!`) and procedural macros
- **Lifetimes**: Explicit lifetime annotations in generics (`'a`, `'static`)
- **Unsafe**: Blocks that opt out of safety guarantees
- **Async/await**: First-class async support with futures
- **Visibility**: `pub` modifier with optional scope (`pub(crate)`, `pub(super)`)

## Node Categories

- [Modules and Crates](#modules-and-crates)
- [Function Definitions](#function-definitions)
- [Type Definitions](#type-definitions)
- [Implementation Blocks](#implementation-blocks)
- [Variable Declarations](#variable-declarations)
- [Function Calls and Access](#function-calls-and-access)
- [Identifiers and Paths](#identifiers-and-paths)
- [Literal Values](#literal-values)
- [Control Flow](#control-flow)
- [Pattern Matching](#pattern-matching)
- [Type System](#type-system)
- [Operators and Expressions](#operators-and-expressions)
- [Blocks and Statements](#blocks-and-statements)
- [Unsafe and Async](#unsafe-and-async)
- [Attributes and Visibility](#attributes-and-visibility)
- [Comments](#comments)
- [Structural Elements](#structural-elements)
- [Keywords](#keywords)
- [Operator Tokens](#operator-tokens)
- [Punctuation](#punctuation)
- [Parse Errors](#parse-errors)

## Modules and Crates

Module structure and external crate declarations

Rust organizes code into modules (`mod`) and external crates. The `source_file` is the root module. The `use` declaration imports items from other modules with various patterns (simple, wildcard, aliased). Import refinements: - MODULE: Full crate/module import - SELECTIVE: Specific items via `use_as_clause` - WILDCARD: Glob imports via `use_wildcard`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `source_file` | DEFINITION_MODULE | NONE | Module structure and external crate declarations |
| `mod_item` | DEFINITION_MODULE | FIND_IDENTIFIER |  |
| `use_declaration` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT |  |
| `extern_crate_declaration` | EXTERNAL_IMPORT | Import::MODULE | FIND_IDENTIFIER |  |
| `use_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `use_as_clause` | EXTERNAL_IMPORT | Import::SELECTIVE | FIND_IDENTIFIER |  |
| `use_wildcard` | EXTERNAL_IMPORT | Import::WILDCARD | NODE_TEXT |  |

## Function Definitions

Functions, closures, and function signatures

Rust functions use `fn` keyword and always have bodies (except in trait definitions). Closures use `|params|` syntax and can capture their environment. Function refinements: - REGULAR: Named functions defined with `fn` - LAMBDA: Closures/anonymous functions with `|params|` Note: `function_signature_item` appears in trait definitions without a body, marked with IS_DECLARATION_ONLY flag.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_item` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Functions, closures, and function signatures |
| `closure_expression` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET |  |
| `function_signature_item` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER |  |
| `macro_definition` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER |  |

## Type Definitions

Structs, enums, unions, traits, and type aliases

Rust has several type definition constructs: - `struct`: Product types with named or tuple fields - `enum`: Sum types with variants - `union`: Unsafe C-compatible unions - `trait`: Abstract interfaces (ABSTRACT refinement) - `type`: Type aliases Note: Uses CUSTOM name extraction because tree-sitter-rust has complex identifier patterns for generic structs/enums.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `struct_item` | DEFINITION_CLASS | Class::REGULAR | CUSTOM | Structs, enums, unions, traits, and type aliases |
| `enum_item` | DEFINITION_CLASS | Class::ENUM | CUSTOM |  |
| `union_item` | DEFINITION_CLASS | Class::REGULAR | CUSTOM |  |
| `trait_item` | DEFINITION_CLASS | Class::ABSTRACT | CUSTOM |  |
| `type_item` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER |  |

## Implementation Blocks

Impl blocks and associated types

Rust separates type definitions from their implementations. An `impl` block can either implement inherent methods or a trait for a type. Associated types are type aliases within traits/impls.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `impl_item` | DEFINITION_CLASS | Class::REGULAR | CUSTOM | Impl blocks and associated types |
| `associated_type` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER |  |

## Variable Declarations

Let bindings, constants, and statics

Rust variables are immutable by default. The `mut` keyword makes them mutable. However, we mark `let_declaration` as IMMUTABLE since that's the default; the presence of `mut` would be a child node. Variable types: - `let`: Local bindings (default immutable) - `const`: Compile-time constants - `static`: Global variables with `'static` lifetime

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `let_declaration` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER | Let bindings, constants, and statics |
| `const_item` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER |  |
| `static_item` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER |  |
| `parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `field_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER |  |
| `enum_variant` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER |  |

## Function Calls and Access

Function calls, method calls, and field/index access

Rust distinguishes between function calls and method calls syntactically. Method calls use `.method()` syntax while function calls are direct. Call refinements: - FUNCTION: Direct function calls `foo()` - METHOD: Method calls `obj.method()` - MACRO: Macro invocations `println!()`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function calls, method calls, and field/index access |
| `method_call_expression` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET |  |
| `macro_invocation` | COMPUTATION_CALL | Call::MACRO | FIND_CALL_TARGET |  |
| `field_expression` | COMPUTATION_ACCESS | FIND_IDENTIFIER |  |
| `index_expression` | COMPUTATION_ACCESS | NONE |  |

## Identifiers and Paths

Identifiers, type identifiers, and scoped paths

Rust has distinct identifier types for values vs types. Scoped identifiers use `::` for path navigation (e.g., `std::collections::HashMap`).

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifiers, type identifiers, and scoped paths |
| `field_identifier` | NAME_IDENTIFIER | NODE_TEXT |  |
| `type_identifier` | TYPE_REFERENCE | NODE_TEXT |  |
| `scoped_identifier` | NAME_IDENTIFIER | NODE_TEXT |  |
| `scoped_type_identifier` | TYPE_REFERENCE | NODE_TEXT |  |

## Literal Values

Numeric, string, and structured literals

Rust literals include numbers, strings, chars, and structured data. Raw strings use `r#"..."#` syntax and get the RAW refinement. Structured literals: - Arrays: `[1, 2, 3]` (SEQUENCE refinement) - Tuples: `(a, b, c)` (SEQUENCE refinement) - Struct expressions: `Point { x: 1, y: 2 }` (MAPPING refinement)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Numeric, string, and structured literals |
| `float_literal` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT |  |
| `string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT |  |
| `raw_string_literal` | LITERAL_STRING | String::RAW | NODE_TEXT |  |
| `char_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT |  |
| `boolean_literal` | LITERAL_ATOMIC | NODE_TEXT |  |
| `array_expression` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE |  |
| `tuple_expression` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE |  |
| `struct_expression` | LITERAL_STRUCTURED | Structured::MAPPING | NONE |  |

## Control Flow

Conditionals, loops, and jump expressions

Rust control flow constructs are expressions that return values. The `match` expression provides exhaustive pattern matching. Loop refinements: - CONDITIONAL: `while` loops - ITERATOR: `for` loops (always iterator-based in Rust) - INFINITE: `loop` (infinite loop, must break to exit) Note: Rust uses expressions rather than statements, so these are `*_expression` rather than `*_statement`.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_expression` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Conditionals, loops, and jump expressions |
| `match_expression` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `match_arm` | FLOW_CONDITIONAL | NONE |  |
| `while_expression` | FLOW_LOOP | Loop::CONDITIONAL | NONE |  |
| `for_expression` | FLOW_LOOP | Loop::ITERATOR | NONE |  |
| `loop_expression` | FLOW_LOOP | Loop::INFINITE | NONE |  |
| `break_expression` | FLOW_JUMP | Jump::BREAK | NONE |  |
| `continue_expression` | FLOW_JUMP | Jump::CONTINUE | NONE |  |
| `return_expression` | FLOW_JUMP | Jump::RETURN | NONE |  |

## Pattern Matching

Destructuring and matching patterns

Rust patterns appear in `let` bindings, `match` arms, and function parameters. They can destructure complex types and bind variables. Pattern types: - PATTERN_DESTRUCTURE: Structural patterns (tuple, struct, reference) - PATTERN_COLLECT: Wildcard `_` and range patterns `..`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `match_pattern` | PATTERN_DESTRUCTURE | NONE | Destructuring and matching patterns |
| `tuple_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `struct_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `reference_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `wildcard_pattern` | PATTERN_COLLECT | NODE_TEXT |  |
| `range_pattern` | PATTERN_COLLECT | NONE |  |

## Type System

Type references, generics, and lifetimes

Rust has a rich type system with generics, lifetimes, and trait bounds. Type categories: - TYPE_PRIMITIVE: Built-in types (`i32`, `bool`, `str`) - TYPE_REFERENCE: References (`&T`, `&mut T`), pointers (`*const T`) - TYPE_GENERIC: Generic parameters and arguments - TYPE_COMPOSITE: Compound types (tuples, arrays, function types, trait objects) Lifetimes (`'a`, `'static`) are part of the generic system.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `primitive_type` | TYPE_PRIMITIVE | NODE_TEXT | Type references, generics, and lifetimes |
| `reference_type` | TYPE_REFERENCE | NONE |  |
| `pointer_type` | TYPE_REFERENCE | NONE |  |
| `tuple_type` | TYPE_COMPOSITE | NONE |  |
| `array_type` | TYPE_COMPOSITE | NONE |  |
| `function_type` | TYPE_COMPOSITE | NONE |  |
| `trait_object_type` | TYPE_COMPOSITE | NONE |  |
| `generic_type` | TYPE_GENERIC | NONE |  |
| `type_parameters` | TYPE_GENERIC | NONE |  |
| `type_parameter` | TYPE_GENERIC | FIND_IDENTIFIER |  |
| `type_arguments` | TYPE_GENERIC | NONE |  |
| `lifetime` | TYPE_GENERIC | NODE_TEXT |  |
| `lifetime_parameter` | TYPE_GENERIC | NODE_TEXT |  |
| `where_clause` | TYPE_GENERIC | NONE |  |
| `higher_ranked_trait_bound` | TYPE_GENERIC | NONE |  |

## Operators and Expressions

Arithmetic, logical, comparison, and assignment operators

Rust operators follow standard precedence rules. Range operators (`..`, `..=`) are used for iterating and slicing. Assignment refinements: - SIMPLE: Basic assignment `=` - COMPOUND: Compound assignment `+=`, `-=`, etc. Arithmetic refinements: - BINARY: Standard binary operators - UNARY: Unary operators (negation, dereference) - RANGE: Range operators `..` and `..=`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Arithmetic, logical, comparison, and assignment operators |
| `compound_assignment_expr` | OPERATOR_ASSIGNMENT | Assignment::COMPOUND | NONE |  |
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE |  |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE |  |
| `range_expression` | OPERATOR_ARITHMETIC | Arithmetic::RANGE | NONE |  |

## Blocks and Statements

Code blocks and expression statements

Rust blocks are expressions that evaluate to their final expression. Expression statements end with `;` and discard their value.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | ORGANIZATION_BLOCK | NONE | Code blocks and expression statements |
| `expression_statement` | EXECUTION_STATEMENT | NONE |  |

## Unsafe and Async

Unsafe blocks and async/await constructs

Unsafe blocks opt out of Rust's safety guarantees for low-level operations. Async blocks and await expressions handle asynchronous programming.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `unsafe_block` | EXECUTION_STATEMENT | NONE | Unsafe blocks and async/await constructs |
| `async_block` | FLOW_SYNC | NONE |  |
| `await_expression` | FLOW_SYNC | NONE |  |

## Attributes and Visibility

Attributes (#[...]) and visibility modifiers

Rust attributes provide metadata for items: - Outer attributes: `#[derive(Debug)]` apply to the following item - Inner attributes: `#![allow(unused)]` apply to the enclosing item Visibility modifiers control access: - `pub`: Public - `pub(crate)`: Crate-visible - `pub(super)`: Parent module visible - (no modifier): Private to module The `mut` specifier indicates mutability for bindings and references.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `attribute_item` | METADATA_ANNOTATION | NODE_TEXT | Attributes (#[...]) and visibility modifiers |
| `inner_attribute_item` | METADATA_ANNOTATION | NODE_TEXT |  |
| `visibility_modifier` | METADATA_ANNOTATION | NODE_TEXT |  |
| `mutable_specifier` | METADATA_ANNOTATION | NODE_TEXT |  |

## Comments

Line and block comments

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `line_comment` | METADATA_COMMENT | NODE_TEXT | Line and block comments |
| `block_comment` | METADATA_COMMENT | NODE_TEXT |  |

## Structural Elements

Lists and organizational nodes

These nodes organize other nodes into logical groups (parameters, arguments, field declarations, etc.).

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Lists and organizational nodes |
| `arguments` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `closure_parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `field_declaration_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `enum_variant_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |

## Keywords

Rust reserved words as syntax tokens

Keywords are marked with IS_KEYWORD flag and get the same semantic type as the constructs they introduce. This enables semantic queries that include or exclude keyword tokens as needed.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `fn` | DEFINITION_FUNCTION | NODE_TEXT | Rust reserved words as syntax tokens |
| `let` | DEFINITION_VARIABLE | NODE_TEXT |  |
| `const` | DEFINITION_VARIABLE | NODE_TEXT |  |
| `static` | DEFINITION_VARIABLE | NODE_TEXT |  |
| `struct` | DEFINITION_CLASS | NODE_TEXT |  |
| `enum` | DEFINITION_CLASS | NODE_TEXT |  |
| `trait` | DEFINITION_CLASS | NODE_TEXT |  |
| `impl` | DEFINITION_CLASS | NODE_TEXT |  |
| `type` | DEFINITION_CLASS | NODE_TEXT |  |
| `union` | DEFINITION_CLASS | NODE_TEXT |  |
| `mod` | DEFINITION_MODULE | NODE_TEXT |  |
| `use` | EXTERNAL_IMPORT | NODE_TEXT |  |
| `extern` | EXTERNAL_IMPORT | NODE_TEXT |  |
| `crate` | EXTERNAL_IMPORT | NODE_TEXT |  |
| `if` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `else` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `match` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `while` | FLOW_LOOP | NODE_TEXT |  |
| `for` | FLOW_LOOP | NODE_TEXT |  |
| `loop` | FLOW_LOOP | NODE_TEXT |  |
| `in` | FLOW_LOOP | NODE_TEXT |  |
| `break` | FLOW_JUMP | NODE_TEXT |  |
| `continue` | FLOW_JUMP | NODE_TEXT |  |
| `return` | FLOW_JUMP | NODE_TEXT |  |
| `async` | FLOW_SYNC | NODE_TEXT |  |
| `await` | FLOW_SYNC | NODE_TEXT |  |
| `pub` | METADATA_ANNOTATION | NODE_TEXT |  |
| `mut` | METADATA_ANNOTATION | NODE_TEXT |  |
| `unsafe` | EXECUTION_STATEMENT | NODE_TEXT |  |
| `where` | TYPE_GENERIC | NODE_TEXT |  |
| `dyn` | TYPE_COMPOSITE | NODE_TEXT |  |
| `Self` | TYPE_REFERENCE | NODE_TEXT |  |
| `as` | COMPUTATION_CALL | NODE_TEXT |  |
| `ref` | PATTERN_DESTRUCTURE | NODE_TEXT |  |
| `move` | COMPUTATION_EXPRESSION | NODE_TEXT |  |
| `self` | NAME_IDENTIFIER | NODE_TEXT |  |
| `super` | NAME_IDENTIFIER | NODE_TEXT |  |

## Operator Tokens

Individual operator symbols

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Individual operator symbols |
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
| `==` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `!=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `+` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `-` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `*` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `/` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `%` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `&` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `|` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `^` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `<<` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `>>` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `&&` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `||` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `!` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `->` | COMPUTATION_ACCESS | NODE_TEXT |  |
| `::` | COMPUTATION_ACCESS | NODE_TEXT |  |
| `.` | COMPUTATION_ACCESS | NODE_TEXT |  |
| `..` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `..=` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `?` | FLOW_CONDITIONAL | NODE_TEXT |  |

## Punctuation

Delimiters, separators, and syntax markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NODE_TEXT | Delimiters, separators, and syntax markers |
| `)` | PARSER_DELIMITER | NODE_TEXT |  |
| `[` | PARSER_DELIMITER | NODE_TEXT |  |
| `]` | PARSER_DELIMITER | NODE_TEXT |  |
| `{` | PARSER_DELIMITER | NODE_TEXT |  |
| `}` | PARSER_DELIMITER | NODE_TEXT |  |
| `,` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `;` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `:` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `=>` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `_` | PARSER_PUNCTUATION | NODE_TEXT |  |

## Parse Errors

Error nodes from failed parsing

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Error nodes from failed parsing |

---

*Generated from `rust_types.def`*
