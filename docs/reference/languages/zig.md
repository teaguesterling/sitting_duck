# Zig Node Types

> Zig language node type mappings for AST semantic extraction

## Language Characteristics

- **Systems programming**: Direct hardware access and manual memory management
- **No hidden control flow**: No hidden allocations, no garbage collection
- **Compile-time execution**: `comptime` for powerful metaprogramming
- **Error handling**: Explicit error unions and `try`/`catch`
- **Optionals**: First-class optional types with `?Type`
- **Pointers and slices**: Rich pointer types with safety features
- **No undefined behavior**: Detectable at compile-time or runtime
- **C interop**: Direct ABI compatibility with C
- **SIMD**: First-class vector types for SIMD operations
- **Async/await**: Stackless coroutines for async programming

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
- [Function Definitions](#function-definitions)
- [Type Definitions](#type-definitions)
- [Variable Declarations](#variable-declarations)
- [Expressions](#expressions)
- [Control Flow](#control-flow)
- [Async/Await](#async-await)
- [Error Handling](#error-handling)
- [Defer Statements](#defer-statements)
- [Comptime](#comptime)
- [Type Expressions](#type-expressions)
- [Identifiers](#identifiers)
- [Literals](#literals)
- [Blocks and Statements](#blocks-and-statements)
- [Initializers](#initializers)
- [Assembly](#assembly)
- [Comments](#comments)
- [Keywords](#keywords)
- [Special Values](#special-values)
- [Punctuation](#punctuation)
- [Operators](#operators)
- [Parser Error Handling](#parser-error-handling)

## Program Structure

Top-level file organization

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `source_file` | DEFINITION_MODULE | NONE | Source file root - top-level compilation unit |

## Function Definitions

Zig function declarations

Zig function features: - `fn name(params) ReturnType { }` - `pub fn name(params) ReturnType { }` - public - `inline fn` - force inlining - `export fn` - export for C ABI - Error return types: `fn foo() !ReturnType` - Optional return types: `fn foo() ?ReturnType`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Function declaration - `fn name(params) Type { }` |
| `function_signature` | TYPE_COMPOSITE | NONE | Function signature - return type and parameters |
| `parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Parameters - `(param1, param2)` |
| `parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Parameter - function parameter |

## Type Definitions

Zig type declarations

Zig type system: - `struct { }` - struct type - `enum { }` - enumeration - `union { }` - tagged or untagged union - `opaque {}` - opaque type for C interop - Error sets: `error { OutOfMemory, ... }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `struct_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Struct declaration - `const S = struct { }` |
| `enum_declaration` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER | Enum declaration - `const E = enum { }` |
| `union_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Union declaration - `const U = union { }` |
| `opaque_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | Opaque declaration - for C interop |
| `error_set_declaration` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER | Error set declaration - `const E = error { }` |

## Variable Declarations

Variable and constant declarations

Zig variable declarations: - `var name: Type = value` - mutable variable - `const name = value` - immutable constant - `comptime var` - compile-time variable - `threadlocal var` - thread-local storage

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable declaration - `var` or `const` |
| `comptime_declaration` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER | Comptime declaration - compile-time variable |
| `test_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Test declaration - `test "name" { }` |
| `using_namespace_declaration` | EXTERNAL_IMPORT | Import::MODULE | NONE | Using namespace declaration - `usingnamespace @import("...")` |

## Expressions

Zig expression types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `expression` | ORGANIZATION_BLOCK | NONE | Expression - general expression |
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Binary expression - `a + b`, `a == b` |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary expression - `-a`, `!a` |
| `assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Assignment expression - `a = b` |
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Call expression - `function(args)` |
| `builtin_function` | COMPUTATION_CALL | Call::FUNCTION | NODE_TEXT | Builtin function - `@builtin(args)` |
| `field_expression` | COMPUTATION_ACCESS | NONE | Field expression - `struct.field` |
| `index_expression` | COMPUTATION_ACCESS | NONE | Index expression - `array[index]` |
| `dereference_expression` | COMPUTATION_ACCESS | NONE | Dereference expression - `ptr.*` |
| `range_expression` | LITERAL_STRUCTURED | NONE | Range expression - `a..b` |

## Control Flow

Conditionals and branching

Zig control flow: - `if (cond) body else body` - `switch (value) { cases... }` - `if (optional) |value| body` - optional unwrapping

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement - `if (cond) body` |
| `for_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | For statement - `for (items) |item| body` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While statement - `while (cond) body` |
| `switch_expression` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch expression - `switch (value) { }` |
| `switch_case` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch case - `pattern => body` |
| `break_expression` | FLOW_JUMP | Jump::BREAK | NONE | Break expression - `break` or `break :label value` |
| `continue_expression` | FLOW_JUMP | Jump::CONTINUE | NONE | Continue expression - `continue` |
| `return_expression` | FLOW_JUMP | Jump::RETURN | NONE | Return expression - `return value` |

## Async/Await

Zig's stackless coroutines

Zig async features: - `async function()` - start async frame - `await handle` - wait for completion - `suspend` - yield control - `resume frame` - resume suspended frame - `nosuspend` - assert no suspension

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `async_expression` | FLOW_SYNC | NONE | Async expression - `async function()` |
| `await_expression` | FLOW_SYNC | NONE | Await expression - `await handle` |
| `suspend_statement` | FLOW_SYNC | NONE | Suspend statement - `suspend { }` |
| `resume_expression` | FLOW_SYNC | NONE | Resume expression - `resume frame` |
| `nosuspend_expression` | FLOW_SYNC | NONE | Nosuspend expression - `nosuspend expr` |

## Error Handling

Zig error handling constructs

Zig error handling: - `try expr` - unwrap error union or return error - `catch` - handle errors - `orelse` - provide default for optional/error - `errdefer` - cleanup on error path

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_expression` | ERROR_TRY | NONE | Try expression - `try value` |
| `catch_expression` | ERROR_CATCH | NONE | Catch expression - `value catch |err| handler` |
| `orelse_expression` | FLOW_CONDITIONAL | NONE | Orelse expression - `optional orelse default` |

## Defer Statements

Deferred execution

Zig defer: - `defer expr` - execute on scope exit - `errdefer expr` - execute only on error

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `defer_statement` | FLOW_JUMP | NONE | Defer statement - `defer cleanup()` |
| `errdefer_statement` | ERROR_FINALLY | NONE | Errdefer statement - `errdefer cleanup()` |

## Comptime

Compile-time execution

Zig comptime: - `comptime { }` - compile-time block - `comptime var` - compile-time variable - Metaprogramming without macros

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comptime_statement` | METADATA_ANNOTATION | NONE | Comptime statement - `comptime { }` |
| `comptime_expression` | METADATA_ANNOTATION | NONE | Comptime expression - `comptime expr` |

## Type Expressions

Type system constructs

Zig types: - `*T` - single-item pointer - `[*]T` - many-item pointer - `[]T` - slice - `[N]T` - array - `?T` - optional - `E!T` - error union - `anyframe` - async frame type

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_expression` | TYPE_REFERENCE | NONE | Type expression |
| `pointer_type` | TYPE_REFERENCE | NONE | Pointer type - `*T`, `[*]T` |
| `slice_type` | TYPE_REFERENCE | NONE | Slice type - `[]T` |
| `array_type` | TYPE_REFERENCE | NONE | Array type - `[N]T` |
| `nullable_type` | TYPE_REFERENCE | NONE | Nullable type - `?T` |
| `error_union_type` | TYPE_REFERENCE | NONE | Error union type - `E!T` |
| `anyframe_type` | TYPE_REFERENCE | NONE | Anyframe type - async frame |
| `builtin_type` | TYPE_PRIMITIVE | NODE_TEXT | Builtin type - `u32`, `i64`, `f32`, etc. |

## Identifiers

Names and identifiers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - name |
| `builtin_identifier` | NAME_IDENTIFIER | NODE_TEXT | Builtin identifier - `@identifier` |

## Literals

Zig literal values

Zig literals: - Integers: `42`, `0xFF`, `0b1010`, `1_000` - Floats: `3.14`, `1e10` - Characters: `'a'` - Strings: `"string"`, `\\multiline string\\` - Boolean: `true`, `false`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `integer` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Integer literal |
| `float` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT | Float literal |
| `string` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal |
| `string_content` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String content |
| `multiline_string` | LITERAL_STRING | String::RAW | NODE_TEXT | Multiline string - `\\string\\` |
| `character` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Character literal - `'a'` |
| `boolean` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true` or `false` |

## Blocks and Statements

Code blocks and statements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Block - `{ statements }` |
| `block_expression` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Block expression - block that returns value |
| `statement` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Statement |
| `labeled_statement` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | FIND_IDENTIFIER | Labeled statement - `label: statement` |

## Initializers

Struct and array initializers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `struct_initializer` | LITERAL_STRUCTURED | Structured::MAPPING | NONE | Struct initializer - `.{ .field = value }` |
| `anonymous_struct_initializer` | LITERAL_STRUCTURED | Structured::MAPPING | NONE | Anonymous struct initializer |
| `array_initializer` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Array initializer - `.{ value, value }` |
| `initializer_list` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Initializer list |
| `arguments` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Arguments - function call arguments |

## Assembly

Inline assembly support

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `asm_expression` | METADATA_ANNOTATION | NONE | Asm expression - `asm volatile ("...")` |
| `asm_input` | METADATA_ANNOTATION | NONE | Asm input - input operand |
| `asm_output` | METADATA_ANNOTATION | NONE | Asm output - output operand |

## Comments

Documentation and comments

Zig comment styles: - `// line comment` - `/// doc comment` - No block comments by design

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment |
| `line_comment` | METADATA_COMMENT | NONE | Line comment - `// comment` |
| `doc_comment` | METADATA_COMMENT | NONE | Doc comment - `/// documentation` |

## Keywords

Zig language keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `const` | DEFINITION_VARIABLE | NODE_TEXT | Const keyword - `const` |
| `var` | DEFINITION_VARIABLE | NODE_TEXT | Var keyword - `var` |
| `pub` | METADATA_ANNOTATION | NODE_TEXT | Pub keyword - `pub` |
| `extern` | EXTERNAL_FOREIGN | NODE_TEXT | Extern keyword - `extern` |
| `export` | EXTERNAL_EXPORT | NODE_TEXT | Export keyword - `export` |
| `inline` | METADATA_ANNOTATION | NODE_TEXT | Inline keyword - `inline` |
| `noinline` | METADATA_ANNOTATION | NODE_TEXT | Noinline keyword - `noinline` |
| `comptime` | METADATA_ANNOTATION | NODE_TEXT | Comptime keyword - `comptime` |
| `test` | DEFINITION_FUNCTION | NODE_TEXT | Test keyword - `test` |
| `fn` | DEFINITION_FUNCTION | NODE_TEXT | Fn keyword - `fn` |
| `struct` | DEFINITION_CLASS | NODE_TEXT | Struct keyword - `struct` |
| `enum` | DEFINITION_CLASS | NODE_TEXT | Enum keyword - `enum` |
| `union` | DEFINITION_CLASS | NODE_TEXT | Union keyword - `union` |
| `if` | FLOW_CONDITIONAL | NODE_TEXT | If keyword - `if` |
| `else` | FLOW_CONDITIONAL | NODE_TEXT | Else keyword - `else` |
| `for` | FLOW_LOOP | NODE_TEXT | For keyword - `for` |
| `while` | FLOW_LOOP | NODE_TEXT | While keyword - `while` |
| `switch` | FLOW_CONDITIONAL | NODE_TEXT | Switch keyword - `switch` |
| `return` | FLOW_JUMP | NODE_TEXT | Return keyword - `return` |
| `break` | FLOW_JUMP | NODE_TEXT | Break keyword - `break` |
| `continue` | FLOW_JUMP | NODE_TEXT | Continue keyword - `continue` |
| `defer` | FLOW_JUMP | NODE_TEXT | Defer keyword - `defer` |
| `errdefer` | ERROR_FINALLY | NODE_TEXT | Errdefer keyword - `errdefer` |
| `try` | ERROR_TRY | NODE_TEXT | Try keyword - `try` |
| `catch` | ERROR_CATCH | NODE_TEXT | Catch keyword - `catch` |
| `orelse` | FLOW_CONDITIONAL | NODE_TEXT | Orelse keyword - `orelse` |
| `async` | FLOW_SYNC | NODE_TEXT | Async keyword - `async` |
| `await` | FLOW_SYNC | NODE_TEXT | Await keyword - `await` |
| `suspend` | FLOW_SYNC | NODE_TEXT | Suspend keyword - `suspend` |
| `resume` | FLOW_SYNC | NODE_TEXT | Resume keyword - `resume` |
| `nosuspend` | FLOW_SYNC | NODE_TEXT | Nosuspend keyword - `nosuspend` |

## Special Values

Zig special value literals

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `null` | LITERAL_ATOMIC | NODE_TEXT | Null literal - `null` |
| `undefined` | LITERAL_ATOMIC | NODE_TEXT | Undefined literal - `undefined` |
| `anytype` | TYPE_PRIMITIVE | NODE_TEXT | Anytype - generic type placeholder |
| `anyerror` | TYPE_PRIMITIVE | NODE_TEXT | Anyerror - any error type |
| `anyframe` | TYPE_PRIMITIVE | NODE_TEXT | Anyframe - any async frame type |
| `anyopaque` | TYPE_PRIMITIVE | NODE_TEXT | Anyopaque - opaque pointer type |

## Punctuation

Delimiters and punctuation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `{` | PARSER_DELIMITER | NODE_TEXT | Left brace - `{` |
| `}` | PARSER_DELIMITER | NODE_TEXT | Right brace - `}` |
| `(` | PARSER_DELIMITER | NODE_TEXT | Left parenthesis - `(` |
| `)` | PARSER_DELIMITER | NODE_TEXT | Right parenthesis - `)` |
| `[` | PARSER_DELIMITER | NODE_TEXT | Left bracket - `[` |
| `]` | PARSER_DELIMITER | NODE_TEXT | Right bracket - `]` |
| `;` | PARSER_PUNCTUATION | NODE_TEXT | Semicolon - `;` |
| `,` | PARSER_PUNCTUATION | NODE_TEXT | Comma - `,` |
| `:` | PARSER_PUNCTUATION | NODE_TEXT | Colon - `:` |
| `.` | PARSER_PUNCTUATION | NODE_TEXT | Period - `.` |
| `@` | PARSER_PUNCTUATION | NODE_TEXT | At sign - `@` |

## Operators

Zig operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Assignment - `=` |
| `+` | OPERATOR_ARITHMETIC | NODE_TEXT | Addition - `+` |
| `-` | OPERATOR_ARITHMETIC | NODE_TEXT | Subtraction - `-` |
| `*` | OPERATOR_ARITHMETIC | NODE_TEXT | Multiplication - `*` |
| `/` | OPERATOR_ARITHMETIC | NODE_TEXT | Division - `/` |
| `%` | OPERATOR_ARITHMETIC | NODE_TEXT | Modulo - `%` |
| `==` | OPERATOR_COMPARISON | NODE_TEXT | Equality - `==` |
| `!=` | OPERATOR_COMPARISON | NODE_TEXT | Inequality - `!=` |
| `<` | OPERATOR_COMPARISON | NODE_TEXT | Less than - `<` |
| `>` | OPERATOR_COMPARISON | NODE_TEXT | Greater than - `>` |
| `<=` | OPERATOR_COMPARISON | NODE_TEXT | Less than or equal - `<=` |
| `>=` | OPERATOR_COMPARISON | NODE_TEXT | Greater than or equal - `>=` |
| `and` | OPERATOR_LOGICAL | NODE_TEXT | Logical AND - `and` |
| `or` | OPERATOR_LOGICAL | NODE_TEXT | Logical OR - `or` |
| `!` | OPERATOR_LOGICAL | NODE_TEXT | Logical NOT - `!` |
| `&` | OPERATOR_ARITHMETIC | NODE_TEXT | Bitwise AND - `&` |
| `|` | OPERATOR_ARITHMETIC | NODE_TEXT | Bitwise OR - `|` |
| `^` | OPERATOR_ARITHMETIC | NODE_TEXT | Bitwise XOR - `^` |
| `~` | OPERATOR_ARITHMETIC | NODE_TEXT | Bitwise NOT - `~` |
| `<<` | OPERATOR_ARITHMETIC | NODE_TEXT | Left shift - `<<` |
| `>>` | OPERATOR_ARITHMETIC | NODE_TEXT | Right shift - `>>` |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `zig_types.def`*
