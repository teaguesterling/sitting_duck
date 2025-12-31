# Julia Node Types

> Julia language node type mappings for AST semantic extraction

## Language Characteristics

- **Scientific computing**: Designed for numerical/scientific work
- **JIT compilation**: LLVM-based just-in-time compilation
- **Multiple dispatch**: Functions dispatch on all argument types
- **Type system**: Optional typing with parametric polymorphism
- **Metaprogramming**: Powerful macro system with AST access
- **Unicode identifiers**: Greek letters, math symbols as identifiers
- **Array-oriented**: First-class multidimensional arrays
- **Interoperability**: Easy calling of C/Fortran/Python code
- **Abstract types**: Type hierarchy without implementation
- **Coroutines**: Tasks and channels for concurrency

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
- [Type Definitions](#type-definitions)
- [Variable Declarations](#variable-declarations)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Control Flow](#control-flow)
- [Loop Constructs](#loop-constructs)
- [Jump Statements](#jump-statements)
- [Error Handling](#error-handling)
- [Anonymous Functions](#anonymous-functions)
- [Identifiers and Literals](#identifiers-and-literals)
- [Comments](#comments)
- [Parser Error Handling](#parser-error-handling)

## Program Structure

Top-level file and module organization

Julia file organization: - Files with `.jl` extension - Modules organize code: `module Name ... end` - Packages defined in Project.toml

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `source_file` | DEFINITION_MODULE | NONE | Source file root - top-level compilation unit |
| `module_definition` | DEFINITION_MODULE | FIND_IDENTIFIER | Module definition - `module Name ... end` |

## Import Statements

Module import and using declarations

Julia import features: - `import Module` - import module (qualified access) - `import Module: name1, name2` - selective import - `using Module` - bring all exports into scope - `using Module: name` - selective using

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `import_statement` | EXTERNAL_IMPORT | FIND_IDENTIFIER | Import statement - `import Module` |
| `using_statement` | EXTERNAL_IMPORT | FIND_IDENTIFIER | Using statement - `using Module` |

## Function Definitions

Julia function and macro declarations

Julia function features: - `function name(args) body end` - standard definition - `name(args) = expr` - short form - Multiple dispatch: methods for different argument types - Type annotations: `function add(x::Int, y::Int)::Int` - Keyword arguments: `function f(x; kwarg=default)` - Varargs: `function f(args...)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Function definition - `function name(params) body end` |
| `short_function_definition` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Short function definition - `f(x) = x + 1` |
| `macro_definition` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Macro definition - `macro name(args) body end` |

## Type Definitions

Julia type declarations

Julia type system: - `struct Name fields... end` - immutable composite type - `mutable struct Name fields... end` - mutable composite type - `abstract type Name end` - abstract type in hierarchy - `primitive type Name bits end` - primitive type definition - Type parameters: `struct Pair{T, S} ... end`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `struct_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Struct definition - `struct Name ... end` |
| `abstract_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Abstract type definition - `abstract type Name end` |
| `primitive_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Primitive type definition - `primitive type Name bits end` |

## Variable Declarations

Variable assignments and constants

Julia variable features: - `x = value` - assignment (creates binding) - `const NAME = value` - constant binding - `global x` - declare global variable - `local x` - declare local variable - Type annotation: `x::Int = 5`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assignment` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Assignment - variable binding `x = value` |
| `const_statement` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Const statement - `const NAME = value` |
| `global_statement` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Global statement - `global x` |

## Function Calls and Expressions

Function and macro invocations

Julia call syntax: - `function(args)` - regular call - `function.(args)` - broadcast (element-wise) - `@macro args` - macro invocation - `obj.method(args)` - method call syntax

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | FIND_CALL_TARGET | Call expression - `function(args)` |
| `macro_expression` | COMPUTATION_CALL | FIND_CALL_TARGET | Macro expression - `@macro args` |

## Control Flow

Conditionals and branching

Julia control flow: - `if cond body elseif cond body else body end` - Ternary: `cond ? a : b` - Short-circuit: `a && b`, `a || b`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | NONE | If statement - `if cond body end` |
| `if_expression` | FLOW_CONDITIONAL | NONE | If expression - ternary `cond ? a : b` |

## Loop Constructs

Iteration mechanisms

Julia loops: - `for x in iterable body end` - for loop - `for x = 1:10 body end` - range iteration - `while cond body end` - while loop - Comprehensions: `[f(x) for x in xs]`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_statement` | FLOW_LOOP | NONE | For statement - `for x in collection body end` |
| `while_statement` | FLOW_LOOP | NONE | While statement - `while cond body end` |

## Jump Statements

Control flow transfer

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `break_statement` | FLOW_JUMP | NONE | Break statement - exits loop |
| `continue_statement` | FLOW_JUMP | NONE | Continue statement - skips to next iteration |
| `return_statement` | FLOW_JUMP | NONE | Return statement - exits function with value |

## Error Handling

Exception handling constructs

Julia error handling: - `try body catch e handler finally cleanup end` - `throw(exception)` - raise exception - `error("message")` - raise error

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_statement` | ERROR_TRY | NONE | Try statement - exception handling |
| `catch_clause` | ERROR_CATCH | NONE | Catch clause - exception handler |
| `finally_clause` | ERROR_FINALLY | NONE | Finally clause - cleanup handler |

## Anonymous Functions

Lambda expressions

Julia anonymous function syntax: - `x -> x + 1` - single argument - `(x, y) -> x + y` - multiple arguments - `function(x) body end` - anonymous with body - `do` blocks: `map(xs) do x ... end`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_expression` | DEFINITION_FUNCTION | NONE | Function expression - anonymous function |
| `do_clause` | DEFINITION_FUNCTION | NONE | Do clause - `do x body end` (anonymous function in call) |

## Identifiers and Literals

Names and literal values

Julia identifiers: - Unicode support: `α`, `∑`, `π` - Convention: lowercase for variables, CamelCase for types Julia literals: - Integers: `42`, `0xFF`, `0b1010`, `1_000_000` - Floats: `3.14`, `1e10`, `Inf`, `NaN` - Strings: `"string"`, `"""multiline"""`, `raw"raw"` - Characters: `'a'` - Boolean: `true`, `false`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - name |
| `integer_literal` | LITERAL_NUMBER | NODE_TEXT | Integer literal |
| `float_literal` | LITERAL_NUMBER | NODE_TEXT | Floating-point literal |
| `string_literal` | LITERAL_STRING | NODE_TEXT | String literal |
| `character_literal` | LITERAL_STRING | NODE_TEXT | Character literal - `'a'` |
| `boolean_literal` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true` or `false` |

## Comments

Documentation and annotation

Julia comment styles: - `# line comment` - `#= block comment =#` - Docstrings: `"Documented function" function f() end`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `julia_types.def`*
