# Go Node Types

> Go language node type mappings for AST semantic extraction

## Language Characteristics

Go has several unique features that affect AST mapping:

- **Package system**: Every file belongs to a package; `main` package is entry point
- **Goroutines**: Lightweight threads via `go` keyword (FLOW_SYNC)
- **Channels**: First-class communication primitives (`chan`, `<-`)
- **Defer**: Deferred execution until function returns (FLOW_SYNC)
- **Interfaces**: Implicit implementation (no `implements` keyword), use ABSTRACT
- **No classes**: Only structs with methods; methods have receiver parameters
- **Short declarations**: `:=` for declare-and-assign in one step
- **Multiple returns**: Functions can return multiple values
- **Range iteration**: `for range` for iterating collections
- **Select**: Multiplexing channel operations
- **Type switches**: Switch on type rather than value
- **iota**: Auto-incrementing constant generator
- **No generics keywords**: Uses `[T any]` syntax (Go 1.18+)

## Node Categories

- [Package and Imports](#package-and-imports)
- [Function Definitions](#function-definitions)
- [Type Definitions](#type-definitions)
- [Variable Declarations](#variable-declarations)
- [Function Calls and Access](#function-calls-and-access)
- [Identifiers](#identifiers)
- [Literal Values](#literal-values)
- [Control Flow](#control-flow)
- [Concurrency Constructs](#concurrency-constructs)
- [Type System](#type-system)
- [Operators and Expressions](#operators-and-expressions)
- [Statements](#statements)
- [Structural Elements](#structural-elements)
- [Comments](#comments)
- [Patterns](#patterns)
- [Keywords](#keywords)
- [Operator Tokens](#operator-tokens)
- [Punctuation](#punctuation)
- [Parse Errors](#parse-errors)

## Package and Imports

Package declarations and import statements

Every Go file starts with a package clause. The `main` package is special as it defines an executable. Imports bring in other packages. Note: `source_file` represents the entire file as a module.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `source_file` | DEFINITION_MODULE | NONE | Package declarations and import statements |
| `package_clause` | DEFINITION_MODULE | FIND_IDENTIFIER |  |
| `package_identifier` | NAME_IDENTIFIER | NODE_TEXT |  |
| `import_declaration` | EXTERNAL_IMPORT | Import::MODULE | NONE |  |
| `import_spec` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT |  |
| `import_spec_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |

## Function Definitions

Functions, methods, and function literals

Go distinguishes between: - `function_declaration`: Package-level functions - `method_declaration`: Functions with a receiver (attached to a type) - `func_literal`: Anonymous functions/closures Go functions always have bodies (no forward declarations). Methods use a receiver parameter to associate with a type.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Functions, methods, and function literals |
| `method_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER |  |
| `func_literal` | DEFINITION_FUNCTION | Function::LAMBDA | NONE |  |
| `method_elem` | DEFINITION_FUNCTION | FIND_IDENTIFIER |  |

## Type Definitions

Struct and interface definitions

Go uses composition over inheritance: - `struct_type`: Product types with fields (REGULAR refinement) - `interface_type`: Abstract types defining method sets (ABSTRACT refinement) Types are typically defined with `type Name struct/interface { ... }`. The `type_declaration` and `type_spec` wrap the actual type definition.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_declaration` | EXECUTION_DECLARATION | FIND_IDENTIFIER | Struct and interface definitions |
| `type_spec` | EXECUTION_DECLARATION | FIND_IDENTIFIER |  |
| `struct_type` | DEFINITION_CLASS | Class::REGULAR | NONE |  |
| `interface_type` | DEFINITION_CLASS | Class::ABSTRACT | NONE |  |

## Variable Declarations

Variable and constant declarations

Go has several variable declaration forms: - `var x int`: Explicit var declaration (MUTABLE) - `x := value`: Short declaration (MUTABLE, infers type) - `const x = value`: Constant declaration (IMMUTABLE) The `_spec` nodes contain the actual identifier and type information.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `var_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable and constant declarations |
| `var_spec` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER |  |
| `var_spec_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `const_declaration` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER |  |
| `const_spec` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER |  |
| `short_var_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER |  |
| `parameter_declaration` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `variadic_parameter_declaration` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `field_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER |  |

## Function Calls and Access

Function calls, method calls, and field/index access

Go uses `selector_expression` for both field access and method calls. The FIND_PROPERTY strategy extracts the accessed member name. Type conversions in Go look like function calls: `int(x)`, `string(bytes)`.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function calls, method calls, and field/index access |
| `type_conversion_expression` | COMPUTATION_CALL | FIND_CALL_TARGET |  |
| `selector_expression` | COMPUTATION_ACCESS | FIND_PROPERTY |  |
| `index_expression` | COMPUTATION_ACCESS | NONE |  |
| `slice_expression` | COMPUTATION_ACCESS | NONE |  |

## Identifiers

Identifier nodes for values, types, and fields

Go has distinct identifier types: - `identifier`: General value identifiers - `field_identifier`: Struct field names in selectors - `type_identifier`: Type names - `label_name`: Labels for goto/break/continue

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier nodes for values, types, and fields |
| `field_identifier` | NAME_IDENTIFIER | NODE_TEXT |  |
| `type_identifier` | TYPE_REFERENCE | NODE_TEXT |  |
| `label_name` | NAME_IDENTIFIER | NODE_TEXT |  |

## Literal Values

Numeric, string, and composite literals

Go literals include: - Numeric: int, float, imaginary (complex numbers!) - String: interpreted (`"..."`) and raw (`` `...` ``) - Rune: single characters (`'a'`) - Composite: struct/array/slice/map literals The `iota` keyword is a special constant generator in const blocks.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `int_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Numeric, string, and composite literals |
| `float_literal` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT |  |
| `imaginary_literal` | LITERAL_NUMBER | Number::COMPLEX | NODE_TEXT |  |
| `interpreted_string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT |  |
| `raw_string_literal` | LITERAL_STRING | String::RAW | NODE_TEXT |  |
| `rune_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT |  |
| `interpreted_string_literal_content` | LITERAL_STRING | NONE |  |
| `escape_sequence` | LITERAL_STRING | NONE |  |
| `nil` | LITERAL_ATOMIC | NODE_TEXT |  |
| `true` | LITERAL_ATOMIC | NODE_TEXT |  |
| `false` | LITERAL_ATOMIC | NODE_TEXT |  |
| `iota` | LITERAL_ATOMIC | NODE_TEXT |  |
| `composite_literal` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE |  |
| `literal_value` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE |  |
| `literal_element` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE |  |

## Control Flow

Conditionals, loops, and jump statements

Go control flow: - `if`: Binary conditional (can include init statement) - `for`: The only loop construct (counter, condition, or range-based) - `switch`: Value-based multiway branching - `type_switch`: Switch on type (unique to Go) - `select`: Multiplex channel operations Loop refinements: - CONDITIONAL: Basic `for` or `for condition` - ITERATOR: `for range` iteration

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Conditionals, loops, and jump statements |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `expression_switch_statement` | FLOW_CONDITIONAL | NONE |  |
| `type_switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `select_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `expression_case` | FLOW_CONDITIONAL | NONE |  |
| `default_case` | FLOW_CONDITIONAL | NONE |  |
| `for_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE |  |
| `for_clause` | FLOW_LOOP | NONE |  |
| `range_clause` | FLOW_LOOP | Loop::ITERATOR | NONE |  |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE |  |
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE |  |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE |  |
| `goto_statement` | FLOW_JUMP | Jump::GOTO | NONE |  |

## Concurrency Constructs

Goroutines, channels, and defer

Go's concurrency primitives: - `go_statement`: Launch a goroutine (lightweight thread) - `defer_statement`: Delay execution until function returns - `send_statement`: Send value to channel (`ch <- value`) - `receive_statement`: Receive from channel (`<-ch`) - `channel_type`: Channel type declaration (`chan T`, `<-chan T`, `chan<- T`) These use FLOW_SYNC semantic type as they relate to synchronization.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `go_statement` | FLOW_SYNC | NONE | Goroutines, channels, and defer |
| `defer_statement` | FLOW_SYNC | NONE |  |
| `send_statement` | EXECUTION_STATEMENT | NONE |  |
| `receive_statement` | EXECUTION_STATEMENT | NONE |  |
| `channel_type` | TYPE_COMPOSITE | NONE |  |

## Type System

Type references, composite types, and generics

Go types include: - `pointer_type`: Pointer to type (`*T`) - `slice_type`: Dynamic array (`[]T`) - `array_type`: Fixed-size array (`[N]T`) - `map_type`: Hash map (`map[K]V`) - `function_type`: Function signature - `channel_type`: Channel (`chan T`) Go 1.18+ generics use `type_arguments` and `type_instantiation_expression`.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `pointer_type` | TYPE_REFERENCE | NONE | Type references, composite types, and generics |
| `qualified_type` | TYPE_REFERENCE | NONE |  |
| `parenthesized_type` | TYPE_REFERENCE | NONE |  |
| `type_assertion_expression` | TYPE_REFERENCE | NONE |  |
| `type_elem` | TYPE_REFERENCE | NONE |  |
| `slice_type` | TYPE_COMPOSITE | NONE |  |
| `array_type` | TYPE_COMPOSITE | NONE |  |
| `map_type` | TYPE_COMPOSITE | NONE |  |
| `function_type` | TYPE_COMPOSITE | NONE |  |
| `generic_type` | TYPE_GENERIC | NONE |  |
| `type_arguments` | TYPE_GENERIC | NONE |  |
| `type_instantiation_expression` | TYPE_GENERIC | NONE |  |

## Operators and Expressions

Arithmetic, logical, comparison, and assignment operators

Go operators: - Standard arithmetic: `+`, `-`, `*`, `/`, `%` - Bitwise: `&`, `|`, `^`, `<<`, `>>`, `&^` (bit clear) - Logical: `&&`, `||`, `!` - Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=` - Assignment: `=`, `:=`, `+=`, `-=`, etc. - Increment/decrement: `++`, `--` (statements, not expressions!) Note: `++` and `--` are statements in Go, not expressions.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Arithmetic, logical, comparison, and assignment operators |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE |  |
| `assignment_statement` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE |  |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE |  |

## Statements

Expression statements and other execution constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `expression_statement` | EXECUTION_STATEMENT | NONE | Expression statements and other execution constructs |
| `inc_statement` | EXECUTION_STATEMENT | NONE |  |
| `dec_statement` | EXECUTION_STATEMENT | NONE |  |
| `labeled_statement` | EXECUTION_STATEMENT | FIND_IDENTIFIER |  |

## Structural Elements

Blocks, lists, and organizational nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Blocks, lists, and organizational nodes |
| `expression_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `argument_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `parameter_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `field_declaration_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |

## Comments

Line and block comments

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Line and block comments |
| `line_comment` | METADATA_COMMENT | NONE |  |
| `block_comment` | METADATA_COMMENT | NONE |  |

## Patterns

Range and variadic patterns

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `range` | PATTERN_COLLECT | NONE | Range and variadic patterns |
| `...` | PATTERN_COLLECT | NONE |  |

## Keywords

Go reserved words as syntax tokens

Keywords are marked with IS_KEYWORD flag and get the same semantic type as the constructs they introduce. This enables semantic queries that include or exclude keyword tokens as needed.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `package` | DEFINITION_MODULE | NONE | Go reserved words as syntax tokens |
| `import` | EXTERNAL_IMPORT | NONE |  |
| `func` | DEFINITION_FUNCTION | NONE |  |
| `var` | DEFINITION_VARIABLE | NONE |  |
| `const` | DEFINITION_VARIABLE | NONE |  |
| `type` | TYPE_REFERENCE | NONE |  |
| `struct` | TYPE_COMPOSITE | NONE |  |
| `interface` | TYPE_COMPOSITE | NONE |  |
| `map` | TYPE_COMPOSITE | NONE |  |
| `chan` | TYPE_COMPOSITE | NONE |  |
| `if` | FLOW_CONDITIONAL | NONE |  |
| `else` | FLOW_CONDITIONAL | NONE |  |
| `for` | FLOW_LOOP | NONE |  |
| `switch` | FLOW_CONDITIONAL | NONE |  |
| `case` | FLOW_CONDITIONAL | NONE |  |
| `default` | FLOW_CONDITIONAL | NONE |  |
| `select` | FLOW_CONDITIONAL | NONE |  |
| `return` | FLOW_JUMP | NONE |  |
| `break` | FLOW_JUMP | NONE |  |
| `continue` | FLOW_JUMP | NONE |  |
| `goto` | FLOW_JUMP | NONE |  |
| `go` | FLOW_SYNC | NONE |  |
| `defer` | FLOW_SYNC | NONE |  |

## Operator Tokens

Individual operator symbols

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NONE | Individual operator symbols |
| `:=` | OPERATOR_ASSIGNMENT | NONE |  |
| `+=` | OPERATOR_ASSIGNMENT | NONE |  |
| `-=` | OPERATOR_ASSIGNMENT | NONE |  |
| `*=` | OPERATOR_ASSIGNMENT | NONE |  |
| `/=` | OPERATOR_ASSIGNMENT | NONE |  |
| `|=` | OPERATOR_ASSIGNMENT | NONE |  |
| `&^=` | OPERATOR_ASSIGNMENT | NONE |  |
| `+` | OPERATOR_ARITHMETIC | NONE |  |
| `-` | OPERATOR_ARITHMETIC | NONE |  |
| `*` | OPERATOR_ARITHMETIC | NONE |  |
| `/` | OPERATOR_ARITHMETIC | NONE |  |
| `%` | OPERATOR_ARITHMETIC | NONE |  |
| `&` | OPERATOR_ARITHMETIC | NONE |  |
| `|` | OPERATOR_ARITHMETIC | NONE |  |
| `^` | OPERATOR_ARITHMETIC | NONE |  |
| `<<` | OPERATOR_ARITHMETIC | NONE |  |
| `>>` | OPERATOR_ARITHMETIC | NONE |  |
| `&^` | OPERATOR_ARITHMETIC | NONE |  |
| `++` | OPERATOR_ARITHMETIC | NONE |  |
| `--` | OPERATOR_ARITHMETIC | NONE |  |
| `==` | OPERATOR_COMPARISON | NONE |  |
| `!=` | OPERATOR_COMPARISON | NONE |  |
| `<` | OPERATOR_COMPARISON | NONE |  |
| `<=` | OPERATOR_COMPARISON | NONE |  |
| `>` | OPERATOR_COMPARISON | NONE |  |
| `>=` | OPERATOR_COMPARISON | NONE |  |
| `&&` | OPERATOR_LOGICAL | NONE |  |
| `||` | OPERATOR_LOGICAL | NONE |  |
| `!` | OPERATOR_LOGICAL | NONE |  |

## Punctuation

Delimiters, separators, and syntax markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NONE | Delimiters, separators, and syntax markers |
| `)` | PARSER_DELIMITER | NONE |  |
| `{` | PARSER_DELIMITER | NONE |  |
| `}` | PARSER_DELIMITER | NONE |  |
| `[` | PARSER_DELIMITER | NONE |  |
| `]` | PARSER_DELIMITER | NONE |  |
| `,` | PARSER_PUNCTUATION | NONE |  |
| `.` | PARSER_PUNCTUATION | NONE |  |
| `;` | PARSER_PUNCTUATION | NONE |  |
| `:` | PARSER_PUNCTUATION | NONE |  |
| `dot` | PARSER_PUNCTUATION | NONE |  |

## Parse Errors

Error nodes from failed parsing

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Error nodes from failed parsing |

---

*Generated from `go_types.def`*
