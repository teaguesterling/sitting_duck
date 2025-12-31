# Kotlin Node Types

> Kotlin language node type mappings for AST semantic extraction

## Language Characteristics

- **JVM language**: Interoperable with Java, also targets JS and Native
- **Null safety**: Nullable types (`T?`) vs non-null types (`T`)
- **Data classes**: Automatic equals/hashCode/toString/copy
- **Coroutines**: Lightweight threads via suspend functions
- **Extension functions**: Add methods to existing classes
- **Smart casts**: Automatic casting after type checks
- **Sealed classes**: Restricted class hierarchies
- **Object declarations**: Singleton pattern built-in
- **Companion objects**: Static-like members for classes
- **Properties**: First-class, with getters/setters

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

- [Function Definitions](#function-definitions)
- [Class and Object Definitions](#class-and-object-definitions)
- [Variable and Property Declarations](#variable-and-property-declarations)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Identifiers and References](#identifiers-and-references)
- [Literals](#literals)
- [Control Flow](#control-flow)
- [Loop Constructs](#loop-constructs)
- [Jump Statements](#jump-statements)
- [Coroutines](#coroutines)
- [Module and Import](#module-and-import)
- [Type System](#type-system)
- [Operators](#operators)
- [Annotations and Metadata](#annotations-and-metadata)
- [Comments](#comments)
- [Keywords](#keywords)
- [Structure and Organization](#structure-and-organization)
- [Punctuation and Delimiters](#punctuation-and-delimiters)
- [Kotlin-Specific Operators](#kotlin-specific-operators)
- [Standard Operators](#standard-operators)
- [Error Handling](#error-handling)

## Function Definitions

Kotlin function and lambda declarations

Kotlin function features: - `fun name(params): ReturnType { }` - named functions - Single-expression: `fun double(x: Int) = x * 2` - Extension: `fun String.addHello() = "Hello, $this"` - Infix: `infix fun Int.add(x: Int) = this + x` - Operator overloading: `operator fun plus(other: T)` - Suspend functions for coroutines

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Function::REGULAR | CUSTOM | Function declaration - `fun name(params): Type { }` |
| `anonymous_function` | DEFINITION_FUNCTION | Function::LAMBDA | NONE | Anonymous function - `fun(params) { }` |
| `annotated_lambda` | DEFINITION_FUNCTION | Function::LAMBDA | NONE | Annotated lambda - lambda with annotations |
| `lambda_literal` | DEFINITION_FUNCTION | Function::LAMBDA | NONE | Lambda literal - `{ params -> body }` |
| `primary_constructor` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | NONE | Primary constructor - in class header |
| `secondary_constructor` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | NONE | Secondary constructor - `constructor(params) { }` |
| `constructor_invocation` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET | Constructor invocation - `ClassName(args)` |

## Class and Object Definitions

Kotlin class hierarchy constructs

Kotlin class types: - `class` - regular class (final by default) - `open class` - can be extended - `abstract class` - cannot be instantiated - `data class` - automatic equals/hashCode/toString/copy - `sealed class` - restricted hierarchy - `enum class` - enumeration - `interface` - can have default implementations - `object` - singleton declaration - `companion object` - static-like members

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Class declaration - `class Name(params) : Parent { }` |
| `object_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Object declaration - singleton `object Name { }` |
| `interface_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | Interface declaration - `interface Name { }` |
| `enum_class_body` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER | Enum class body - enum entries and members |
| `companion_object` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Companion object - `companion object { }` |
| `anonymous_initializer` | DEFINITION_FUNCTION | Function::REGULAR | NONE | Anonymous initializer - `init { }` block |

## Variable and Property Declarations

Properties and variables with mutability

Kotlin variables: - `val` - immutable (like final in Java) - `var` - mutable - Properties have automatic getters/setters - Delegated properties: `by lazy`, `by observable` - `lateinit var` - late initialization for non-null types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `property_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Property declaration - `val`/`var` in class |
| `variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable declaration - local `val`/`var` |
| `multi_variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | NONE | Multi-variable declaration - destructuring `val (a, b) = pair` |
| `parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Function parameter |
| `class_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Class constructor parameter |
| `lambda_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Lambda parameter |

## Function Calls and Expressions

Invocations and access expressions

Kotlin call features: - Named arguments: `func(name = value)` - Default parameters: `fun foo(x: Int = 0)` - Trailing lambdas: `list.map { it * 2 }` - Infix calls: `1 to 2` - Safe calls: `obj?.method()` - Elvis operator: `x ?: default`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function/method call - `func(args)` or `obj.method(args)` |
| `navigation_expression` | COMPUTATION_ACCESS | NONE | Navigation expression - `obj.property` or `obj?.property` |
| `indexing_expression` | COMPUTATION_ACCESS | NONE | Indexing expression - `arr[index]` |
| `this_expression` | NAME_SCOPED | NODE_TEXT | This expression - reference to current instance |
| `super_expression` | NAME_SCOPED | NODE_TEXT | Super expression - reference to parent class |

## Identifiers and References

Names and type references

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `simple_identifier` | NAME_IDENTIFIER | NODE_TEXT | Simple identifier - variable, function, or class name |
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier node |
| `import_identifier` | NAME_QUALIFIED | NODE_TEXT | Import identifier - qualified name in import |
| `type_identifier` | TYPE_REFERENCE | NODE_TEXT | Type identifier - type name |

## Literals

Kotlin literal values

Kotlin literals: - Integers: `123`, `123L`, `0x1F`, `0b1010` - Floats: `3.14`, `3.14f` - Characters: `'a'`, `'\n'` - Strings: `"string"`, `"""raw string"""` - String templates: `"Value: $x"` or `"${expr}"` - Boolean: `true`, `false` - Null: `null`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Integer literal - decimal, hex, or binary |
| `hex_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Hex literal - `0x1F` |
| `bin_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Binary literal - `0b1010` |
| `real_literal` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT | Real/float literal - `3.14` or `3.14f` |
| `boolean_literal` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true` or `false` |
| `character_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Character literal - `'a'` |
| `string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal - `"string"` or `"""raw"""` |
| `null_literal` | LITERAL_ATOMIC | NODE_TEXT | Null literal |
| `collection_literal` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Collection literal |
| `array_literal` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Array literal - `arrayOf(...)` |
| `list_literal` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | List literal - `listOf(...)` |

## Control Flow

Conditionals and branching

Kotlin control flow: - `if` is an expression (returns value) - `when` is pattern matching (like switch but more powerful) - No ternary operator (use `if` expression)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_expression` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If expression - `if (cond) expr else expr` |
| `when_expression` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | When expression - pattern matching `when (x) { ... }` |
| `when_condition` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | When condition - individual case in when |
| `try_expression` | ERROR_TRY | NONE | Try expression - exception handling |
| `catch_block` | ERROR_CATCH | NONE | Catch block |
| `finally_block` | ERROR_FINALLY | NONE | Finally block |

## Loop Constructs

Iteration mechanisms

Kotlin loops: - `for (item in collection)` - iterate over anything with iterator - `while (condition)` - condition-based loop - `do { } while (condition)` - post-condition loop - Labels for break/continue: `loop@ for (...)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `for_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | For statement - `for (x in collection)` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While statement - `while (cond) { }` |
| `do_while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Do-while statement - `do { } while (cond)` |

## Jump Statements

Control flow transfer

Kotlin jumps: - `return` - exit function - `break` - exit loop - `continue` - skip iteration - Labeled: `return@label`, `break@loop`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `return_expression` | FLOW_JUMP | Jump::RETURN | NONE | Return expression - `return value` |
| `throw_expression` | ERROR_THROW | NONE | Throw expression - `throw Exception()` |
| `break_expression` | FLOW_JUMP | Jump::BREAK | NONE | Break expression - exits loop |
| `continue_expression` | FLOW_JUMP | Jump::CONTINUE | NONE | Continue expression - skips to next iteration |

## Coroutines

Suspend functions and coroutine support

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `suspend_modifier` | FLOW_SYNC | NONE | Suspend modifier - marks function as suspendable |

## Module and Import

Package and import declarations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `source_file` | DEFINITION_MODULE | NONE | Source file - root node |
| `package_header` | DEFINITION_MODULE | FIND_IDENTIFIER | Package header - `package com.example` |
| `import_header` | EXTERNAL_IMPORT | Import::MODULE | FIND_IDENTIFIER | Import header - `import com.example.Class` |
| `import_list` | EXTERNAL_IMPORT | Import::MODULE | NONE | Import list |

## Type System

Kotlin type declarations and references

Kotlin type system: - Nullable types: `String?` - Platform types: `String!` (from Java) - Generic types: `List<T>` - Function types: `(Int) -> String` - Type aliases: `typealias Name = OtherType`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_alias` | DEFINITION_CLASS | FIND_IDENTIFIER | Type alias - `typealias Name = Type` |
| `type_reference` | TYPE_REFERENCE | NONE | Type reference |
| `user_type` | TYPE_REFERENCE | NONE | User type - class or interface type |
| `nullable_type` | TYPE_REFERENCE | NONE | Nullable type - `Type?` |
| `function_type` | TYPE_COMPOSITE | NONE | Function type - `(Params) -> Return` |
| `parenthesized_type` | TYPE_REFERENCE | NONE | Parenthesized type |

## Operators

Kotlin operators and expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assignment` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Assignment - `=` |
| `additive_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Additive expression - `+`, `-` |
| `multiplicative_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Multiplicative expression - `*`, `/`, `%` |
| `comparison_expression` | OPERATOR_COMPARISON | NONE | Comparison expression - `<`, `>`, `<=`, `>=` |
| `equality_expression` | OPERATOR_COMPARISON | NONE | Equality expression - `==`, `!=`, `===`, `!==` |
| `conjunction_expression` | OPERATOR_LOGICAL | NONE | Conjunction expression - `&&` |
| `disjunction_expression` | OPERATOR_LOGICAL | NONE | Disjunction expression - `||` |
| `range_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Range expression - `1..10` or `1..<10` |
| `infix_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Infix expression - `a to b` |
| `prefix_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Prefix expression - `!`, `-`, `++` |
| `postfix_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | FIND_IDENTIFIER | Postfix expression - `++`, `--`, `!!`, `?` |
| `as_expression` | OPERATOR_COMPARISON | NONE | As expression - type cast `x as Type` |
| `is_expression` | OPERATOR_COMPARISON | NONE | Is expression - type check `x is Type` |
| `in_expression` | OPERATOR_COMPARISON | NONE | In expression - containment check `x in collection` |
| `elvis_expression` | OPERATOR_LOGICAL | NONE | Elvis expression - `x ?: default` |

## Annotations and Metadata

Kotlin annotations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `annotation` | METADATA_ANNOTATION | FIND_IDENTIFIER | Annotation - `@Annotation` |
| `file_annotation` | METADATA_ANNOTATION | FIND_IDENTIFIER | File annotation - `@file:Annotation` |
| `use_site_target` | METADATA_ANNOTATION | NONE | Use-site target - `@get:`, `@set:`, `@field:` |

## Comments

Documentation and annotation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `line_comment` | METADATA_COMMENT | NONE | Line comment - `// comment` |
| `multiline_comment` | METADATA_COMMENT | NONE | Multiline comment - `/* comment */` |

## Keywords

Kotlin reserved and modifier keywords

Kotlin has hard keywords (always reserved) and soft keywords (reserved only in certain contexts).

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `abstract` | METADATA_ANNOTATION | NODE_TEXT | Kotlin reserved and modifier keywords |
| `actual` | METADATA_ANNOTATION | NODE_TEXT |  |
| `annotation` | METADATA_ANNOTATION | NODE_TEXT |  |
| `by` | METADATA_ANNOTATION | NODE_TEXT |  |
| `const` | DEFINITION_VARIABLE | NODE_TEXT |  |
| `crossinline` | METADATA_ANNOTATION | NODE_TEXT |  |
| `data` | METADATA_ANNOTATION | NODE_TEXT |  |
| `delegate` | METADATA_ANNOTATION | NODE_TEXT |  |
| `dynamic` | TYPE_REFERENCE | NODE_TEXT |  |
| `expect` | METADATA_ANNOTATION | NODE_TEXT |  |
| `external` | METADATA_ANNOTATION | NODE_TEXT |  |
| `final` | METADATA_ANNOTATION | NODE_TEXT |  |
| `infix` | METADATA_ANNOTATION | NODE_TEXT |  |
| `inline` | METADATA_ANNOTATION | NODE_TEXT |  |
| `inner` | METADATA_ANNOTATION | NODE_TEXT |  |
| `internal` | METADATA_ANNOTATION | NODE_TEXT |  |
| `lateinit` | METADATA_ANNOTATION | NODE_TEXT |  |
| `noinline` | METADATA_ANNOTATION | NODE_TEXT |  |
| `open` | METADATA_ANNOTATION | NODE_TEXT |  |
| `operator` | METADATA_ANNOTATION | NODE_TEXT |  |
| `out` | METADATA_ANNOTATION | NODE_TEXT |  |
| `override` | METADATA_ANNOTATION | NODE_TEXT |  |
| `private` | METADATA_ANNOTATION | NODE_TEXT |  |
| `protected` | METADATA_ANNOTATION | NODE_TEXT |  |
| `public` | METADATA_ANNOTATION | NODE_TEXT |  |
| `reified` | METADATA_ANNOTATION | NODE_TEXT |  |
| `sealed` | METADATA_ANNOTATION | NODE_TEXT |  |
| `suspend` | FLOW_SYNC | NODE_TEXT |  |
| `tailrec` | METADATA_ANNOTATION | NODE_TEXT |  |
| `vararg` | METADATA_ANNOTATION | NODE_TEXT |  |
| `where` | METADATA_ANNOTATION | NODE_TEXT |  |

## Structure and Organization

Block and list structures

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Class body - content of class |
| `block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Block - statement block |
| `lambda_literal` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Lambda literal body |
| `function_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Function body |
| `when_entry` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | When entry - case in when expression |
| `parameter_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Parameter list |
| `value_parameter_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Value parameter list |
| `argument_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Argument list |
| `type_parameter_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Type parameter list - generics |
| `type_arguments` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Type arguments - generic arguments |

## Punctuation and Delimiters

Syntactic markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NODE_TEXT | Syntactic markers |
| `)` | PARSER_DELIMITER | NODE_TEXT |  |
| `[` | PARSER_DELIMITER | NODE_TEXT |  |
| `]` | PARSER_DELIMITER | NODE_TEXT |  |
| `{` | PARSER_DELIMITER | NODE_TEXT |  |
| `}` | PARSER_DELIMITER | NODE_TEXT |  |
| `,` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `;` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `.` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `:` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `?` | PARSER_PUNCTUATION | NODE_TEXT |  |

## Kotlin-Specific Operators

Special Kotlin operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `!!` | OPERATOR_LOGICAL | NODE_TEXT | Non-null assertion - `!!` |
| `?:` | OPERATOR_LOGICAL | NODE_TEXT | Elvis operator - `?:` |
| `?..` | OPERATOR_COMPARISON | NODE_TEXT | Safe navigation - `?.` |
| `::` | COMPUTATION_ACCESS | NODE_TEXT | Member reference - `::` |
| `@` | METADATA_ANNOTATION | NODE_TEXT | Annotation prefix |
| `$` | LITERAL_STRING | NODE_TEXT | String interpolation - `$` |

## Standard Operators

Arithmetic, comparison, and logical operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Arithmetic, comparison, and logical operators |
| `+=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `-=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `*=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `/=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `%=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |

## Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

## Other Node Types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `as` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `class` | DEFINITION_CLASS | NODE_TEXT |  |
| `companion` | DEFINITION_CLASS | NODE_TEXT |  |
| `constructor` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `enum` | DEFINITION_CLASS | NODE_TEXT |  |
| `fun` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `get` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `init` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `interface` | DEFINITION_CLASS | NODE_TEXT |  |
| `object` | DEFINITION_CLASS | NODE_TEXT |  |
| `package` | DEFINITION_MODULE | NODE_TEXT |  |
| `set` | DEFINITION_FUNCTION | NODE_TEXT |  |
| `typealias` | DEFINITION_CLASS | NODE_TEXT |  |
| `val` | DEFINITION_VARIABLE | NODE_TEXT |  |
| `var` | DEFINITION_VARIABLE | NODE_TEXT |  |
| `break` | FLOW_JUMP | NODE_TEXT |  |
| `continue` | FLOW_JUMP | NODE_TEXT |  |
| `do` | FLOW_LOOP | NODE_TEXT |  |
| `else` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `for` | FLOW_LOOP | NODE_TEXT |  |
| `if` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `in` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `is` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `return` | FLOW_JUMP | NODE_TEXT |  |
| `when` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `while` | FLOW_LOOP | NODE_TEXT |  |
| `catch` | ERROR_CATCH | NODE_TEXT |  |
| `finally` | ERROR_FINALLY | NODE_TEXT |  |
| `throw` | ERROR_THROW | NODE_TEXT |  |
| `try` | ERROR_TRY | NODE_TEXT |  |
| `import` | EXTERNAL_IMPORT | NODE_TEXT |  |
| `super` | NAME_SCOPED | NODE_TEXT |  |
| `this` | NAME_SCOPED | NODE_TEXT |  |
| `false` | LITERAL_ATOMIC | NODE_TEXT |  |
| `null` | LITERAL_ATOMIC | NODE_TEXT |  |
| `true` | LITERAL_ATOMIC | NODE_TEXT |  |
| `+` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `-` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `*` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `/` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `%` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `++` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `--` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `==` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `!=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `===` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `!==` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `&&` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `||` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `!` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `..` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `..<` | OPERATOR_ARITHMETIC | NODE_TEXT |  |

---

*Generated from `kotlin_types.def`*
