# R Node Types

> R language node type mappings for AST semantic extraction

## Language Characteristics

- **Statistical computing**: Designed for data analysis and statistics
- **Vectorized operations**: Operations apply element-wise to vectors
- **Dynamic typing**: Variables have no declared types
- **Functional style**: Functions are first-class, many higher-order functions
- **Formula objects**: `y ~ x` for model specifications
- **NA values**: Explicit handling of missing data
- **Multiple assignment operators**: `<-`, `=`, `->`, `<<-`
- **Pipe operators**: `|>` (native), `%>%` (magrittr)
- **S3/S4/R6 systems**: Multiple object-oriented paradigms
- **REPL-oriented**: Interactive data exploration

## Semantic Type Encoding

Semantic types use an 8-bit encoding:
- Bits 7-2: Base semantic category (e.g., DEFINITION_FUNCTION = 0x04)
- Bits 1-0: Refinement within category (e.g., Function::REGULAR = 0x00)

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

- [Function Definitions](#function-definitions)
- [Control Flow](#control-flow)
- [Loop Constructs](#loop-constructs)
- [Jump Statements](#jump-statements)
- [Keywords](#keywords)
- [Identifiers and Parameters](#identifiers-and-parameters)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Data Access](#data-access)
- [Literals](#literals)
- [R Special Constants](#r-special-constants)
- [Structured Data](#structured-data)
- [Special Constructs](#special-constructs)
- [Organizational Structures](#organizational-structures)
- [String Components](#string-components)
- [Comments](#comments)
- [Assignment Operators](#assignment-operators)
- [Arithmetic Operators](#arithmetic-operators)
- [Comparison Operators](#comparison-operators)
- [Logical Operators](#logical-operators)
- [R-Specific Operators](#r-specific-operators)
- [Delimiters and Punctuation](#delimiters-and-punctuation)
- [Type Suffixes](#type-suffixes)
- [Parser Error Handling](#parser-error-handling)

## Function Definitions

R function declarations

R function features: - `name <- function(params) body` - standard definition - Anonymous: `function(x) x + 1` - Default arguments: `function(x, y = 1)` - Variadic: `function(...)` - All arguments passed by value (copy-on-modify)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Function::REGULAR | FIND_ASSIGNMENT_TARGET | Function definition - `name <- function(params) body` |
| `function` | DEFINITION_FUNCTION | NONE | Function keyword - `function` keyword |

## Control Flow

Conditionals and branching

R control flow: - `if (cond) expr` or `if (cond) expr else expr` - `ifelse(cond, yes, no)` - vectorized conditional - `switch(expr, ...)` - multi-way branch

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement - `if (cond) expr else expr` |

## Loop Constructs

Iteration mechanisms

R loops: - `for (x in seq) body` - for loop - `while (cond) body` - while loop - `repeat body` - infinite loop (use break to exit) - Prefer vectorized operations and `apply` family over loops

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | For statement - `for (x in seq) body` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While statement - `while (cond) body` |
| `repeat_statement` | FLOW_LOOP | Loop::INFINITE | NONE | Repeat statement - `repeat body` (infinite loop) |

## Jump Statements

Control flow transfer

R jump statements: - `return(value)` - exit function with value - `break` - exit loop - `next` - skip to next iteration (like continue)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `return` | FLOW_JUMP | Jump::RETURN | NONE | Return statement - `return(value)` |
| `break` | FLOW_JUMP | Jump::BREAK | NONE | Break statement - exits loop |
| `next` | FLOW_JUMP | Jump::CONTINUE | NONE | Next statement - skips to next iteration (like continue) |

## Keywords

R language keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if` | FLOW_CONDITIONAL | NONE | If keyword |
| `else` | FLOW_CONDITIONAL | NONE | Else keyword |
| `for` | FLOW_LOOP | NONE | For keyword |
| `while` | FLOW_LOOP | NONE | While keyword |
| `repeat` | FLOW_LOOP | NONE | Repeat keyword |
| `in` | FLOW_LOOP | NONE | In keyword - used in for loops |

## Identifiers and Parameters

Names and function parameters

R identifier conventions: - Variable names: `snake_case` or `period.case` - Function names: same as variables - Backticks for non-standard names: `` `invalid-name` ``

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier - name |
| `parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Parameter - function parameter |

## Function Calls and Expressions

Function invocations and operators

R call syntax: - `function(args)` - regular call - `x %op% y` - infix operator (user-defined) - `package::function()` - namespaced call - Method dispatch via S3/S4/R6 systems

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Call expression - `function(args)` |
| `binary_operator` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Binary operator - `x + y`, `x %*% y` |
| `unary_operator` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary operator - `-x`, `!x` |

## Data Access

Subsetting and extraction operators

R access operators: - `x[i]` - subset (preserves class) - `x[[i]]` - extract single element - `x$name` - named element access - `x@slot` - S4 slot access - `pkg::name` - namespace access

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `subset` | COMPUTATION_ACCESS | NONE | Subset - `x[i]` preserving class |
| `subset2` | COMPUTATION_ACCESS | NONE | Subset2 - `x[[i]]` extracting element |
| `extract_operator` | COMPUTATION_ACCESS | NONE | Extract operator - `x$name` |
| `namespace_operator` | COMPUTATION_ACCESS | NONE | Namespace operator - `pkg::name` |

## Literals

Numeric, string, and special values

R literals: - Integers: `42L` (L suffix required) - Doubles: `3.14`, `1e10`, `Inf`, `NaN` - Complex: `1+2i` - Strings: `"double"` or `'single'` - Raw: `r"(...)"` (R 4.0+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal |
| `integer` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Integer literal - `42L` |
| `float` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT | Float literal - `3.14` |
| `complex` | LITERAL_NUMBER | Number::COMPLEX | NODE_TEXT | Complex literal - `1+2i` |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean TRUE |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean FALSE |

## R Special Constants

NA, NULL, and other special values

R special values: - `NULL` - absence of a value - `NA` - missing value (logical) - `NA_integer_`, `NA_real_`, `NA_character_`, `NA_complex_` - typed NAs - `Inf`, `-Inf` - infinity - `NaN` - not a number

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `null` | LITERAL_ATOMIC | NODE_TEXT | NULL - absence of value |
| `na` | LITERAL_ATOMIC | NODE_TEXT | NA - missing value (logical) |
| `NA` | LITERAL_ATOMIC | NODE_TEXT | NA - missing value (alternate form) |
| `NA_integer_` | LITERAL_ATOMIC | NODE_TEXT | NA_integer_ - typed missing integer |
| `NA_real_` | LITERAL_ATOMIC | NODE_TEXT | NA_real_ - typed missing real |
| `NA_character_` | LITERAL_ATOMIC | NODE_TEXT | NA_character_ - typed missing character |
| `NA_complex_` | LITERAL_ATOMIC | NODE_TEXT | NA_complex_ - typed missing complex |
| `inf` | LITERAL_ATOMIC | NODE_TEXT | Inf - positive infinity |
| `nan` | LITERAL_ATOMIC | NODE_TEXT | NaN - not a number |

## Structured Data

Vectors, lists, and data frames

R data structures: - `c(1, 2, 3)` - vector (atomic) - `list(a = 1, b = 2)` - heterogeneous list - `data.frame(...)` - rectangular data - `matrix(...)` - 2D array - `array(...)` - N-dimensional array

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `vector` | LITERAL_STRUCTURED | Structured::SEQUENCE | NODE_TEXT | Vector - atomic vector |
| `list` | LITERAL_STRUCTURED | Structured::SEQUENCE | NODE_TEXT | List - heterogeneous list |
| `data_frame` | LITERAL_STRUCTURED | Structured::MAPPING | NODE_TEXT | Data frame - rectangular data |
| `matrix` | LITERAL_STRUCTURED | Structured::SEQUENCE | NODE_TEXT | Matrix - 2D array |
| `array` | LITERAL_STRUCTURED | Structured::SEQUENCE | NODE_TEXT | Array - N-dimensional array |

## Special Constructs

R-specific language features

R special constructs: - `...` - variadic arguments (dots) - `..1`, `..2` - accessing individual dot arguments - `%>%`, `%*%` - user-defined infix operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `dots` | PATTERN_COLLECT | NODE_TEXT | Dots - variadic arguments `...` |
| `dot_dot_i` | PATTERN_COLLECT | NODE_TEXT | Dot-dot-i - `..1`, `..2`, etc. |
| `special` | COMPUTATION_CALL | Call::FUNCTION | NODE_TEXT | Special operator - user-defined infix `%op%` |

## Organizational Structures

Program and expression structures

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `program` | ORGANIZATION_CONTAINER | Organization::HIERARCHICAL | NONE | Program - root node for R script |
| `braced_expression` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Braced expression - `{ ... }` |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE | Parenthesized expression - `( ... )` |
| `arguments` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Arguments - function call arguments |
| `parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Parameters - function parameters |
| `argument` | ORGANIZATION_LIST | FIND_IDENTIFIER | Argument - single named argument |

## String Components

String literal parts

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string_content` | LITERAL_STRING | NONE | String content - text inside string |
| `escape_sequence` | LITERAL_STRING | NONE | Escape sequence - `\n`, `\t`, etc. |

## Comments

Documentation

R comment style: - `# comment` - line comment (only comment type in R) - roxygen2: `#' @param x description`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment - `# comment` |

## Assignment Operators

R's multiple assignment forms

R assignment operators: - `<-` - standard assignment (preferred) - `=` - assignment (often in function calls) - `->` - rightward assignment - `<<-` - superassignment (modifies enclosing scope) - `->>` - rightward superassignment - `:=` - data.table assignment

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `<-` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Left assignment - `x <- value` (preferred) |
| `<<-` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Global assignment - `x <<- value` (modifies enclosing scope) |
| `->` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Right assignment - `value -> x` |
| `->>` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Right global assignment - `value ->> x` |
| `=` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Equals assignment - `x = value` |
| `:=` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Walrus assignment - `:=` (data.table) |

## Arithmetic Operators

Mathematical operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Addition - `+` |
| `-` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Subtraction - `-` |
| `*` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Multiplication - `*` |
| `/` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Division - `/` |
| `^` | OPERATOR_ARITHMETIC | NONE | Power - `^` |
| `**` | OPERATOR_ARITHMETIC | NONE | Power (alternate) - `**` |
| `%` | OPERATOR_ARITHMETIC | NONE | Modulo - `%%` |

## Comparison Operators

Relational operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `==` | OPERATOR_COMPARISON | NONE | Equality - `==` |
| `!=` | OPERATOR_COMPARISON | NONE | Inequality - `!=` |
| `<` | OPERATOR_COMPARISON | NONE | Less than - `<` |
| `<=` | OPERATOR_COMPARISON | NONE | Less than or equal - `<=` |
| `>` | OPERATOR_COMPARISON | NONE | Greater than - `>` |
| `>=` | OPERATOR_COMPARISON | NONE | Greater than or equal - `>=` |

## Logical Operators

Boolean operators

R logical operators: - `&` - element-wise AND (vectorized) - `&&` - short-circuit AND (scalar) - `|` - element-wise OR (vectorized) - `||` - short-circuit OR (scalar) - `!` - NOT

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `&` | OPERATOR_LOGICAL | NONE | Element-wise AND - `&` |
| `&&` | OPERATOR_LOGICAL | NONE | Short-circuit AND - `&&` |
| `|` | OPERATOR_LOGICAL | NONE | Element-wise OR - `|` |
| `||` | OPERATOR_LOGICAL | NONE | Short-circuit OR - `||` |
| `!` | OPERATOR_LOGICAL | NONE | Logical NOT - `!` |

## R-Specific Operators

Operators unique to R

R special operators: - `::` - namespace access - `:::` - internal namespace access - `@` - S4 slot access - `$` - named element access - `:` - sequence generation - `|>` - native pipe (R 4.1+) - `~` - formula operator - `?` - help operator

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `::` | COMPUTATION_ACCESS | NONE | Namespace - `pkg::name` |
| `:::` | COMPUTATION_ACCESS | NONE | Internal namespace - `pkg:::name` |
| `@` | COMPUTATION_ACCESS | NONE | Slot access - `obj@slot` (S4) |
| `$` | COMPUTATION_ACCESS | NONE | Element access - `obj$name` |
| `:` | OPERATOR_ARITHMETIC | NONE | Sequence operator - `1:10` |
| `|>` | OPERATOR_ARITHMETIC | NONE | Native pipe - `|>` (R 4.1+) |
| `~` | OPERATOR_ARITHMETIC | NONE | Formula operator - `y ~ x` |
| `?` | METADATA_ANNOTATION | NONE | Help operator - `?topic` |

## Delimiters and Punctuation

Syntax tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NONE | Left parenthesis |
| `)` | PARSER_DELIMITER | NONE | Right parenthesis |
| `{` | PARSER_DELIMITER | NONE | Left brace |
| `}` | PARSER_DELIMITER | NONE | Right brace |
| `[` | PARSER_DELIMITER | NONE | Left bracket |
| `]` | PARSER_DELIMITER | NONE | Right bracket |
| `[[` | PARSER_DELIMITER | NONE | Double left bracket |
| `]]` | PARSER_DELIMITER | NONE | Double right bracket |
| `comma` | PARSER_PUNCTUATION | NONE | Comma |
| `'` | PARSER_PUNCTUATION | NONE | Single quote |
| `\\` | PARSER_PUNCTUATION | NONE | Backslash |

## Type Suffixes

Literal type modifiers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `L` | TYPE_PRIMITIVE | NODE_TEXT | Integer suffix - `L` |
| `i` | TYPE_PRIMITIVE | NODE_TEXT | Imaginary suffix - `i` |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `r_types.def`*
