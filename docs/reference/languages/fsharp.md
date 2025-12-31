# Fsharp Node Types

> F# language node type mappings for AST semantic extraction

## Node Categories

- [Program Structure](#program-structure)
- [Import Statements](#import-statements)
- [Function Definitions](#function-definitions)
- [Type Definitions](#type-definitions)
- [Variable Declarations](#variable-declarations)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Control Flow](#control-flow)
- [Loop Constructs](#loop-constructs)
- [Error Handling](#error-handling)
- [Computation Expressions](#computation-expressions)
- [Lambda Expressions](#lambda-expressions)
- [Identifiers and Literals](#identifiers-and-literals)
- [Comments](#comments)
- [Parser Error Handling](#parser-error-handling)

## Program Structure

Top-level file and module organization

F# file organization: - Files can have module declarations at top - Namespaces group related modules - Files without module declaration use filename as module name

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `file` | DEFINITION_MODULE | NONE | File root - top-level compilation unit |
| `namespace_declaration` | DEFINITION_MODULE | FIND_IDENTIFIER | Namespace declaration - `namespace Name.Space` |
| `module_declaration` | DEFINITION_MODULE | FIND_IDENTIFIER | Module declaration - `module ModuleName` |

## Import Statements

Open declarations for namespace access

F# import features: - `open Namespace` - brings names into scope - `open type TypeName` - imports static members (F# 5.0+) - `#r` directives for assembly references

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `open_declaration` | EXTERNAL_IMPORT | FIND_IDENTIFIER | Open declaration - `open System.Collections` |

## Function Definitions

F# function and method declarations

F# function features: - `let name params = body` - value binding - `let rec` for recursive functions - Curried parameters: `let add x y = x + y` - Pattern matching in parameters - Type annotations: `let add (x: int) (y: int): int = x + y`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Function declaration - `let functionName params = body` |
| `method_declaration` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Method declaration - member in type definition |

## Type Definitions

F# type declarations

F# type system: - `type Name = ...` - type definition - Records: `type Person = { Name: string; Age: int }` - Discriminated unions: `type Option<'a> = Some of 'a | None` - Classes: `type Person(name, age) = ...` - Interfaces: `type IComparable = ...`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | Type declaration - general type definition |
| `record_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | Record declaration - `type Person = { Name: string }` |
| `union_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | Union declaration - discriminated union `type Option = Some | None` |

## Variable Declarations

Value bindings

F# binding features: - `let x = value` - immutable binding (default) - `let mutable x = value` - mutable binding - `use x = disposable` - auto-disposal binding - Pattern matching in bindings: `let (a, b) = tuple`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `value_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Value declaration - `let x = value` |

## Function Calls and Expressions

Function applications

F# call syntax: - `function arg1 arg2` - curried application - `obj.Method(args)` - .NET method invocation - Pipelining: `value |> transform |> process`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `application_expression` | COMPUTATION_CALL | FIND_CALL_TARGET | Application expression - function application `f x` |
| `invoke_expression` | COMPUTATION_CALL | FIND_CALL_TARGET | Invoke expression - .NET method call `obj.Method()` |

## Control Flow

Conditionals and pattern matching

F# control flow: - `if cond then expr1 else expr2` - conditional (is an expression) - `match value with | pattern -> expr` - pattern matching - Everything is an expression in F#

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_expression` | FLOW_CONDITIONAL | NONE | If expression - `if cond then a else b` |
| `match_expression` | FLOW_CONDITIONAL | NONE | Match expression - pattern matching |

## Loop Constructs

Iteration mechanisms

F# loops: - `for x in collection do body` - for-in loop - `for i = start to end do body` - counting loop - `while cond do body` - while loop - Prefer recursion or higher-order functions over loops

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_expression` | FLOW_LOOP | NONE | For expression - `for x in items do ...` |
| `while_expression` | FLOW_LOOP | NONE | While expression - `while cond do ...` |

## Error Handling

Exception handling constructs

F# error handling: - `try ... with pattern -> handler` - exception handling - `try ... finally cleanup` - cleanup handling - Prefer `Result<'T, 'E>` or `Option<'T>` over exceptions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_expression` | ERROR_TRY | NONE | Try expression - exception handling entry |
| `with_expression` | ERROR_CATCH | NONE | With expression - exception handler (`with pattern -> ...`) |
| `finally_expression` | ERROR_FINALLY | NONE | Finally expression - cleanup handler |

## Computation Expressions

Monadic workflows

F# computation expressions: - `async { ... }` - asynchronous workflows - `seq { ... }` - sequence generation - `query { ... }` - LINQ-style queries - Custom builders for domain-specific workflows

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ce_expression` | FLOW_SYNC | NONE | Computation expression - `builder { ... }` |

## Lambda Expressions

Anonymous functions

F# lambda syntax: - `fun x -> x + 1` - lambda expression - `fun x y -> x + y` - multi-parameter lambda - Pattern matching in lambdas: `fun (a, b) -> a + b`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `lambda_expression` | DEFINITION_FUNCTION | NONE | Lambda expression - `fun x -> expr` |
| `anon_record_expression` | DEFINITION_FUNCTION | NONE | Anonymous record expression - `{| Field = value |}` |

## Identifiers and Literals

Names and literal values

F# literals: - Integers: `42`, `0xFF`, `42L` (int64), `42un` (native) - Floats: `3.14`, `3.14f` (float32), `3.14M` (decimal) - Strings: `"string"`, `@"verbatim"`, `"""triple"""`, `$"interpolated"` - Characters: `'a'` - Boolean: `true`, `false` - Unit: `()` - similar to void

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - simple name |
| `long_identifier` | NAME_QUALIFIED | NODE_TEXT | Long identifier - qualified name `Module.SubModule.Name` |
| `int` | LITERAL_NUMBER | NODE_TEXT | Integer literal |
| `float` | LITERAL_NUMBER | NODE_TEXT | Floating-point literal |
| `string` | LITERAL_STRING | NODE_TEXT | String literal |
| `char` | LITERAL_STRING | NODE_TEXT | Character literal - `'a'` |
| `bool` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true` or `false` |

## Comments

Documentation and annotation

F# comment styles: - `// line comment` - `(* block comment *)` - `/// XML doc comment`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `fsharp_types.def`*
