# Scala Node Types

> Scala language node type mappings for AST semantic extraction

## Language Characteristics

- **Multi-paradigm**: Functional and object-oriented
- **JVM language**: Interoperable with Java
- **Type inference**: Strong static typing with inference
- **Case classes**: Immutable data classes with pattern matching
- **Traits**: Mixins with implementation
- **Pattern matching**: Powerful match expressions
- **For-comprehensions**: Monadic operations
- **Implicit conversions**: Type-safe implicit parameters
- **Higher-order functions**: First-class function support
- **Companion objects**: Static-like members

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
- [Class and Object Definitions](#class-and-object-definitions)
- [Variable Declarations](#variable-declarations)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Control Flow](#control-flow)
- [Loop Constructs](#loop-constructs)
- [Jump Statements](#jump-statements)
- [Error Handling](#error-handling)
- [Lambdas and Anonymous Functions](#lambdas-and-anonymous-functions)
- [Identifiers and Literals](#identifiers-and-literals)
- [Comments](#comments)
- [Parser Error Handling](#parser-error-handling)

## Program Structure

Top-level file structure

Scala file organization: - Compilation unit is the root - Package declarations organize code - Objects can contain main methods

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `compilation_unit` | DEFINITION_MODULE | NONE | Compilation unit - root node for Scala file |
| `package_clause` | DEFINITION_MODULE | FIND_IDENTIFIER | Package clause - `package com.example` |

## Import Statements

Scala import declarations

Scala import features: - `import package.Class` - single import - `import package._` - wildcard import - `import package.{A, B}` - selective import - `import package.{A => B}` - renaming import

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `import_declaration` | EXTERNAL_IMPORT | FIND_IDENTIFIER | Import declaration |

## Function Definitions

Scala method and function declarations

Scala function features: - `def name(params): Type = body` - Curried parameters: `def add(x: Int)(y: Int)` - By-name parameters: `def foo(x: => Int)` - Implicit parameters: `def foo(implicit x: Context)` - Multiple parameter lists - Type parameters: `def foo[T](x: T)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Function definition - `def name(params): Type = body` |
| `function_declaration` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Function declaration - abstract method signature |

## Class and Object Definitions

Scala class hierarchy constructs

Scala class types: - `class` - regular class - `case class` - immutable data class with pattern matching - `object` - singleton object (companion or standalone) - `trait` - mixin with optional implementation - `abstract class` - cannot be instantiated - `sealed` - restricted subclass hierarchy

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Class definition - `class Name(params) extends Parent` |
| `object_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Object definition - singleton `object Name` |
| `trait_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Trait definition - `trait Name` |
| `case_class_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Case class definition - `case class Name(params)` |

## Variable Declarations

Value and variable bindings

Scala variable declarations: - `val` - immutable binding (recommended) - `var` - mutable variable (discouraged) - `lazy val` - lazily evaluated immutable - Pattern matching in bindings: `val (a, b) = tuple`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `val_definition` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Val definition - immutable `val x = value` |
| `var_definition` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Var definition - mutable `var x = value` |
| `val_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Val declaration - abstract val signature |
| `var_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Var declaration - abstract var signature |

## Function Calls and Expressions

Method invocations and expressions

Scala call syntax: - `method(args)` - regular call - `obj.method(args)` - method call - `method arg` - infix notation for single arg - `method { block }` - block as last argument

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | FIND_CALL_TARGET | Call expression - `function(args)` |
| `application_expression` | COMPUTATION_CALL | FIND_CALL_TARGET | Application expression - method application |

## Control Flow

Conditionals and pattern matching

Scala control flow: - `if` is an expression (returns value) - `match` for pattern matching (like switch but more powerful) - Everything is an expression

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_expression` | FLOW_CONDITIONAL | NONE | If expression - `if (cond) expr else expr` |
| `match_expression` | FLOW_CONDITIONAL | NONE | Match expression - pattern matching |

## Loop Constructs

Iteration mechanisms

Scala loops: - `for (x <- collection)` - for-comprehension - `for { ... } yield expr` - produces collection - `while (cond) body` - traditional loop - Prefer higher-order functions over loops

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_expression` | FLOW_LOOP | NONE | For expression - for-comprehension |
| `while_expression` | FLOW_LOOP | NONE | While expression - `while (cond) body` |

## Jump Statements

Control flow transfer

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `return_expression` | FLOW_JUMP | NONE | Return expression - `return value` (rarely used) |
| `throw_expression` | ERROR_THROW | NONE | Throw expression - `throw exception` |

## Error Handling

Exception handling constructs

Scala error handling: - `try { } catch { case e: Exception => } finally { }` - Pattern matching in catch clauses - Prefer `Try`, `Option`, `Either` over exceptions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_expression` | ERROR_TRY | NONE | Try expression - exception handling |
| `catch_clause` | ERROR_CATCH | NONE | Catch clause - handles exceptions |
| `finally_clause` | ERROR_FINALLY | NONE | Finally clause - always executed |

## Lambdas and Anonymous Functions

Function literals

Scala function literal syntax: - `(x: Int) => x * 2` - with type - `x => x * 2` - with type inference - `_ * 2` - placeholder syntax - `{ case pattern => expr }` - partial function

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `lambda_expression` | DEFINITION_FUNCTION | NONE | Lambda expression - `(x) => expr` |
| `anonymous_function` | DEFINITION_FUNCTION | NONE | Anonymous function |

## Identifiers and Literals

Names and literal values

Scala literals: - Integers: `42`, `0xFF`, `42L` - Floats: `3.14`, `3.14f`, `3.14d` - Strings: `"string"`, `"""raw"""`, `s"interpolated $x"` - Symbols: `'symbol` (deprecated in Scala 3) - Boolean: `true`, `false` - Null: `null` (avoid in idiomatic Scala)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - name |
| `integer_literal` | LITERAL_NUMBER | NODE_TEXT | Integer literal |
| `floating_point_literal` | LITERAL_NUMBER | NODE_TEXT | Floating-point literal |
| `string_literal` | LITERAL_STRING | NODE_TEXT | String literal |
| `character_literal` | LITERAL_STRING | NODE_TEXT | Character literal - `'a'` |
| `boolean_literal` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true` or `false` |
| `null_literal` | LITERAL_ATOMIC | NODE_TEXT | Null literal (avoid in idiomatic Scala) |

## Comments

Documentation and annotation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `scala_types.def`*
