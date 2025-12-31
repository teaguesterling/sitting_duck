# Csharp Node Types

> C# language node type mappings for AST semantic extraction

## Node Categories

- [Program Structure](#program-structure)
- [Using Statements](#using-statements)
- [Method Definitions](#method-definitions)
- [Class and Type Definitions](#class-and-type-definitions)
- [Variable and Property Declarations](#variable-and-property-declarations)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Control Flow](#control-flow)
- [Loop Constructs](#loop-constructs)
- [Jump Statements](#jump-statements)
- [Error Handling](#error-handling)
- [Async/Await](#async-await)
- [Identifiers and Literals](#identifiers-and-literals)
- [Comments](#comments)
- [Parser Error Handling](#parser-error-handling)

## Program Structure

Top-level file structure

C# file organization: - `using` directives at top - `namespace` declarations organize code - File-scoped namespaces (C# 10+): `namespace Name;`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `compilation_unit` | DEFINITION_MODULE | NONE | Compilation unit - root node for C# file |
| `namespace_declaration` | DEFINITION_MODULE | FIND_IDENTIFIER | Namespace declaration - `namespace Name { }` |

## Using Statements

Import and namespace directives

C# using features: - `using Namespace;` - import namespace - `using static Class;` - import static members - `using Alias = Type;` - type alias - `global using` - file-scoped (C# 10+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `using_directive` | EXTERNAL_IMPORT | Import::MODULE | FIND_IDENTIFIER | Using directive - `using System;` |

## Method Definitions

C# method and function declarations

C# method features: - `public Type Method(params) { }` - `ref` and `out` parameters - `params` for variadic parameters - Expression-bodied: `int Foo() => 42;` - Local functions: nested function definitions - `async` methods for asynchronous programming

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `method_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Method declaration - class method |
| `constructor_declaration` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER | Constructor declaration - `ClassName(params) { }` |
| `destructor_declaration` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER | Destructor/finalizer - `~ClassName() { }` |
| `operator_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Operator overload - `public static T operator+(T a, T b)` |
| `lambda_expression` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Lambda expression - `(x) => x * 2` |

## Class and Type Definitions

C# type declarations

C# type kinds: - `class` - reference type with single inheritance - `struct` - value type, no inheritance - `interface` - contract definition - `enum` - enumeration type - `record` - immutable reference type (C# 9+) - `record struct` - immutable value type (C# 10+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Class declaration - reference type |
| `struct_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Struct declaration - value type |
| `interface_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | Interface declaration - contract |
| `enum_declaration` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER | Enum declaration - enumeration |
| `record_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Record declaration - immutable reference type (C# 9+) |

## Variable and Property Declarations

Fields, properties, and local variables

C# member types: - Fields: `private int _field;` - Properties: `public int Property { get; set; }` - Auto-properties: `public int Prop { get; set; }` - Init-only: `public int Prop { get; init; }` (C# 9+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `field_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Field declaration - class field |
| `property_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Property declaration - get/set accessor |
| `variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Local variable declaration |
| `parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Method parameter |

## Function Calls and Expressions

Method invocations and expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `invocation_expression` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET | Method invocation - `obj.Method(args)` |
| `object_creation_expression` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET | Object creation - `new ClassName(args)` |

## Control Flow

Conditionals and branching

C# control flow: - `if`/`else if`/`else` - standard conditional - `switch` with pattern matching (C# 8+) - Switch expressions: `x switch { pattern => value }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement - `if (cond) { }` |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement - pattern matching |

## Loop Constructs

Iteration mechanisms

C# loops: - `for (init; cond; incr)` - classic for - `foreach (var x in collection)` - iterator - `while (cond)` - pre-condition - `do { } while (cond)` - post-condition

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_statement` | FLOW_LOOP | Loop::COUNTER | NONE | For statement - `for (int i = 0; i < n; i++)` |
| `foreach_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | Foreach statement - `foreach (var x in collection)` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While statement - `while (cond) { }` |
| `do_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Do statement - `do { } while (cond);` |

## Jump Statements

Control flow transfer

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE | Break statement - exits loop or switch |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE | Continue statement - skips to next iteration |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE | Return statement - exits method with value |
| `throw_statement` | ERROR_THROW | NONE | Throw statement - raises exception |

## Error Handling

Exception handling constructs

C# exception handling: - `try { } catch (Exception e) { } finally { }` - Exception filters: `catch (E e) when (condition)` - `throw;` to rethrow preserving stack trace

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_statement` | ERROR_TRY | NONE | Try statement - exception handling |
| `catch_clause` | ERROR_CATCH | NONE | Catch clause - handles exceptions |
| `finally_clause` | ERROR_FINALLY | NONE | Finally clause - always executed |

## Async/Await

Asynchronous programming support

C# async features: - `async` modifier on methods - `await` for asynchronous operations - Returns `Task`, `Task<T>`, or `ValueTask<T>`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `await_expression` | FLOW_SYNC | NONE | Await expression - `await asyncOperation` |

## Identifiers and Literals

Names and literal values

C# literals: - Integers: `42`, `0xFF`, `42L`, `42u` - Floats: `3.14`, `3.14f`, `3.14m` (decimal) - Strings: `"string"`, `@"verbatim"`, `$"interpolated"` - Characters: `'a'` - Boolean: `true`, `false` - Null: `null`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - name |
| `integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Integer literal |
| `real_literal` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT | Real/float literal |
| `string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal |
| `character_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Character literal - `'a'` |
| `boolean_literal` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true` or `false` |
| `null_literal` | LITERAL_ATOMIC | NODE_TEXT | Null literal |

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

*Generated from `csharp_types.def`*
