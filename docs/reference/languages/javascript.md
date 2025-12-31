# Javascript Node Types

> JavaScript language node type mappings for AST semantic extraction

## Language Characteristics

- **Dynamic typing**: No static type annotations (see TypeScript for typed variant)
- **First-class functions**: Functions are values, can be passed and returned
- **Prototypal inheritance**: Objects inherit directly from other objects
- **ES6+ classes**: Syntactic sugar over prototype-based inheritance
- **Arrow functions**: Concise syntax with lexical `this` binding
- **Async/await**: Promise-based asynchronous programming
- **Destructuring**: Pattern matching for arrays and objects
- **Template literals**: String interpolation with backticks
- **Modules**: ES6 import/export system
- **Spread/rest operators**: `...` for expansion and collection

## Semantic Type Encoding

Semantic types use 8-bit encoding:
- Bits 7-2: Base category (e.g., `DEFINITION_FUNCTION = 0x04`)
- Bits 1-0: Refinement within category

Example: `DEFINITION_FUNCTION | SemanticRefinements::Function::LAMBDA`
  - Base: 0x04 (function definition)
  - Refinement: 0x01 (lambda/arrow)
  - Combined: 0x05

## Node Categories

- [Function Definitions](#function-definitions)
- [Class Definitions](#class-definitions)
- [Variable Declarations](#variable-declarations)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Member Access](#member-access)
- [Identifiers and References](#identifiers-and-references)
- [Literals](#literals)
- [Control Flow](#control-flow)
- [Async/Sync Constructs](#async-sync-constructs)
- [Error Handling](#error-handling)
- [Structure and Organization](#structure-and-organization)
- [Module System](#module-system)
- [Destructuring Patterns](#destructuring-patterns)
- [Function Parameters](#function-parameters)
- [Comments](#comments)
- [Keywords](#keywords)
- [Punctuation](#punctuation)
- [Arithmetic Operators](#arithmetic-operators)
- [Logical Operators](#logical-operators)
- [Comparison Operators](#comparison-operators)
- [Assignment Operators](#assignment-operators)
- [Modern Operators](#modern-operators)
- [Type Constructs](#type-constructs)
- [Parser Errors](#parser-errors)

## Function Definitions

JavaScript function declaration forms

JavaScript has multiple function syntaxes, each with different `this` binding and hoisting behavior. All JavaScript functions have bodies (IS_EMBODIED).

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Named function declaration: `function name(params) { ... }` |
| `arrow_function` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Arrow function: `(params) => expr` or `(params) => { ... }` |
| `function_expression` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Function expression: `const f = function(params) { ... }` |
| `method_definition` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Method definition in class or object: `methodName(params) { ... }` |
| `async_function_declaration` | DEFINITION_FUNCTION | Function::ASYNC | FIND_IDENTIFIER | Async function: `async function name(params) { ... }` |
| `generator_function` | DEFINITION_FUNCTION | Function::ASYNC | FIND_IDENTIFIER | Generator function: `function* name(params) { ... }` |
| `generator_function_declaration` | DEFINITION_FUNCTION | Function::ASYNC | FIND_IDENTIFIER | Generator function declaration |
| `async_generator_function` | DEFINITION_FUNCTION | Function::ASYNC | FIND_IDENTIFIER | Async generator: `async function* name(params) { ... }` |

## Class Definitions

ES6 class syntax (syntactic sugar over prototypes)

JavaScript classes provide cleaner syntax for constructor functions and prototype-based inheritance.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | Class declaration: `class Name { ... }` |
| `class_expression` | DEFINITION_CLASS | FIND_IDENTIFIER | Class expression: `const C = class { ... }` |
| `class_body` | ORGANIZATION_BLOCK | Organization::MAPPING | NONE | Class body block containing methods and fields |
| `field_definition` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Field definition in class: `fieldName = value;` |

## Variable Declarations

Variable declaration forms with different scoping rules

JavaScript has three variable declaration keywords with different scoping and hoisting behaviors.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Variable declaration: `var name = value;` |
| `lexical_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Lexical declaration: `let` or `const` |
| `variable_declarator` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Variable declarator: `name = value` within declaration |

## Function Calls and Expressions

Call expressions and operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function call: `func(args)` with FUNCTION refinement |
| `new_expression` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET | Constructor call: `new Class(args)` with CONSTRUCTOR refinement |
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Binary expression: `a + b`, `a * b`, etc. |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary expression: `!a`, `-a`, `typeof a` |
| `assignment_expression` | OPERATOR_ASSIGNMENT | NONE | Assignment expression: `a = b` |
| `augmented_assignment_expression` | OPERATOR_ASSIGNMENT | NONE | Augmented assignment: `a += b`, `a *= b`, etc. |
| `update_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Update expression: `a++`, `++a`, `a--`, `--a` |
| `ternary_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE | Ternary/conditional expression: `a ? b : c` |
| `sequence_expression` | COMPUTATION_EXPRESSION | NONE | Sequence expression: `a, b, c` (evaluates all, returns last) |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE | Parenthesized expression: `(expr)` |
| `expression_statement` | EXECUTION_STATEMENT | NONE | Expression statement: expression used as statement |
| `empty_statement` | EXECUTION_STATEMENT | NONE | Empty statement: standalone `;` |

## Member Access

Property and element access expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `member_expression` | COMPUTATION_ACCESS | CUSTOM | Member access: `obj.property` |
| `subscript_expression` | COMPUTATION_ACCESS | NONE | Subscript access: `obj[key]` |
| `optional_chain` | COMPUTATION_ACCESS | NONE | Optional chaining: `obj?.property` |

## Identifiers and References

Names and special references

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier: variable, function, or parameter name |
| `property_identifier` | NAME_IDENTIFIER | NODE_TEXT | Property identifier in member access |
| `statement_identifier` | NAME_IDENTIFIER | NODE_TEXT | Statement identifier (label name) |
| `shorthand_property_identifier` | NAME_IDENTIFIER | NODE_TEXT | Shorthand property: `{ x }` equivalent to `{ x: x }` |
| `this` | NAME_SCOPED | NODE_TEXT | this keyword: current execution context |
| `super` | NAME_SCOPED | NODE_TEXT | super keyword: parent class reference |
| `meta_property` | NAME_SCOPED | NODE_TEXT | Meta property: `import.meta`, `new.target` |

## Literals

Literal values: strings, numbers, booleans, structures

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string` | LITERAL_STRING | NODE_TEXT | String literal: `"string"` or `'string'` |
| `template_string` | LITERAL_STRING | NODE_TEXT | Template string: `` `template ${expr}` `` |
| `template_substitution` | LITERAL_STRING | NONE | Template substitution: `${expr}` inside template |
| `string_fragment` | LITERAL_STRING | NONE | String fragment inside template |
| `escape_sequence` | LITERAL_STRING | NONE | Escape sequence in string: `\n`, `\t`, etc. |
| `number` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Number literal: integers and floats |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean true |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean false |
| `null` | LITERAL_ATOMIC | NODE_TEXT | Null value |
| `undefined` | LITERAL_ATOMIC | NODE_TEXT | Undefined value |
| `array` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Array literal: `[a, b, c]` |
| `object` | LITERAL_STRUCTURED | Structured::MAPPING | NONE | Object literal: `{ key: value }` |
| `pair` | LITERAL_STRUCTURED | Structured::MAPPING | NONE | Key-value pair in object: `key: value` |
| `property_assignment` | LITERAL_STRUCTURED | Structured::MAPPING | FIND_IDENTIFIER | Property assignment in object literal |
| `computed_property_name` | COMPUTATION_EXPRESSION | NONE | Computed property name: `[expr]: value` |
| `regex` | LITERAL_STRING | NODE_TEXT | Regular expression literal: `/pattern/flags` |
| `regex_pattern` | LITERAL_STRING | NODE_TEXT | Regex pattern content |
| `regex_flags` | LITERAL_STRING | NODE_TEXT | Regex flags: g, i, m, etc. |

## Control Flow

Conditionals, loops, and jumps

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement: `if (cond) { ... }` |
| `else_clause` | FLOW_CONDITIONAL | NONE | Else clause: `else { ... }` |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement: `switch (expr) { ... }` |
| `switch_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Switch body block |
| `switch_case` | FLOW_CONDITIONAL | NONE | Switch case: `case value:` |
| `case_clause` | FLOW_CONDITIONAL | NONE | Case clause in switch |
| `switch_default` | FLOW_CONDITIONAL | NONE | Switch default: `default:` |
| `default_clause` | FLOW_CONDITIONAL | NONE | Default clause in switch |
| `conditional_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE | Conditional expression: `a ? b : c` |
| `for_statement` | FLOW_LOOP | Loop::COUNTER | NONE | C-style for loop: `for (init; cond; update) { ... }` |
| `for_in_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | For-in loop: `for (key in obj) { ... }` |
| `for_of_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | For-of loop: `for (item of iterable) { ... }` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While loop: `while (cond) { ... }` |
| `do_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Do-while loop: `do { ... } while (cond);` |
| `return_statement` | FLOW_JUMP | NONE | Return statement: `return value;` |
| `break_statement` | FLOW_JUMP | NONE | Break statement: `break;` or `break label;` |
| `continue_statement` | FLOW_JUMP | NONE | Continue statement: `continue;` or `continue label;` |
| `labeled_statement` | EXECUTION_STATEMENT | FIND_IDENTIFIER | Labeled statement: `label: statement` |

## Async/Sync Constructs

Asynchronous programming primitives

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `await_expression` | FLOW_SYNC | NONE | Await expression: `await promise` |
| `yield_expression` | FLOW_SYNC | NONE | Yield expression: `yield value` in generators |

## Error Handling

Try/catch/finally and throw statements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_statement` | ERROR_TRY | NONE | Try statement: `try { ... }` |
| `catch_clause` | ERROR_CATCH | NONE | Catch clause: `catch (e) { ... }` |
| `throw_statement` | ERROR_THROW | NONE | Throw statement: `throw error;` |
| `finally_clause` | ERROR_FINALLY | NONE | Finally clause: `finally { ... }` |

## Structure and Organization

Blocks, modules, and organizational nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `statement_block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Statement block: `{ statements... }` |
| `program` | DEFINITION_MODULE | NONE | Program root node |
| `arguments` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Function arguments list |
| `formal_parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Formal parameters in function definition |

## Module System

ES6 import/export declarations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `import_statement` | EXTERNAL_IMPORT | Import::MODULE | NONE | Import statement: `import ... from "module"` |
| `export_statement` | EXTERNAL_EXPORT | NONE | Export statement: `export ...` |
| `import_specifier` | EXTERNAL_IMPORT | Import::SELECTIVE | FIND_IDENTIFIER | Import specifier: `{ name }` in import |
| `export_specifier` | EXTERNAL_EXPORT | FIND_IDENTIFIER | Export specifier: `{ name }` in export |
| `import_clause` | EXTERNAL_IMPORT | NONE | Import clause in import statement |
| `export_clause` | EXTERNAL_EXPORT | NONE | Export clause in export statement |
| `import_default_specifier` | EXTERNAL_IMPORT | Import::MODULE | FIND_IDENTIFIER | Default import: `import Name from "module"` |
| `export_default_declaration` | EXTERNAL_EXPORT | NONE | Default export: `export default ...` |
| `namespace_import` | EXTERNAL_IMPORT | Import::WILDCARD | FIND_IDENTIFIER | Namespace import: `import * as name from "module"` |
| `named_imports` | EXTERNAL_IMPORT | Import::SELECTIVE | NONE | Named imports: `{ a, b, c }` |
| `import_attribute` | EXTERNAL_IMPORT | NONE | Import attribute (import assertions) |

## Destructuring Patterns

Pattern matching for arrays and objects

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assignment_pattern` | PATTERN_DESTRUCTURE | FIND_IDENTIFIER | Assignment pattern: `a = default` in destructuring |
| `object_pattern` | PATTERN_DESTRUCTURE | NONE | Object pattern: `{ a, b }` in destructuring |
| `array_pattern` | PATTERN_DESTRUCTURE | NONE | Array pattern: `[a, b]` in destructuring |
| `rest_pattern` | PATTERN_COLLECT | NONE | Rest pattern: `...rest` in destructuring |
| `spread_element` | PATTERN_COLLECT | NONE | Spread element: `...items` in array/call |
| `shorthand_property_identifier_pattern` | PATTERN_DESTRUCTURE | NODE_TEXT | Shorthand property identifier in pattern |
| `pair_pattern` | PATTERN_DESTRUCTURE | NONE | Pair pattern in object destructuring |
| `object_assignment_pattern` | PATTERN_DESTRUCTURE | NONE | Object assignment pattern |

## Function Parameters

Parameter types in function definitions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `optional_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Optional parameter: `param = default` |
| `rest_parameter` | PATTERN_COLLECT | FIND_IDENTIFIER | Rest parameter: `...params` |
| `required_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Required parameter (TypeScript, but used in JS parsing) |
| `method_signature` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Method signature (declaration only) |
| `property_signature` | DEFINITION_VARIABLE | FIND_IDENTIFIER | Property signature (declaration only) |

## Comments

Documentation and comments

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment: line or block style |
| `hash_bang_line` | METADATA_COMMENT | NODE_TEXT | Hash-bang line: `#!/usr/bin/env node` |

## Keywords

Reserved words and language keywords

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function` | DEFINITION_FUNCTION | NONE | Reserved words and language keywords |
| `class` | DEFINITION_CLASS | NONE |  |
| `const` | DEFINITION_VARIABLE | Variable::IMMUTABLE | NONE |  |
| `let` | DEFINITION_VARIABLE | Variable::MUTABLE | NONE |  |
| `var` | DEFINITION_VARIABLE | Variable::MUTABLE | NONE |  |
| `if` | FLOW_CONDITIONAL | NONE |  |
| `else` | FLOW_CONDITIONAL | NONE |  |
| `for` | FLOW_LOOP | NONE |  |
| `while` | FLOW_LOOP | NONE |  |
| `do` | FLOW_LOOP | NONE |  |
| `switch` | FLOW_CONDITIONAL | NONE |  |
| `case` | FLOW_CONDITIONAL | NONE |  |
| `default` | FLOW_CONDITIONAL | NONE |  |
| `return` | FLOW_JUMP | NONE |  |
| `break` | FLOW_JUMP | NONE |  |
| `continue` | FLOW_JUMP | NONE |  |
| `async` | FLOW_SYNC | NONE |  |
| `await` | FLOW_SYNC | NONE |  |
| `yield` | FLOW_SYNC | NONE |  |
| `import` | EXTERNAL_IMPORT | NONE |  |
| `export` | EXTERNAL_EXPORT | NONE |  |
| `from` | EXTERNAL_IMPORT | NONE |  |
| `throw` | ERROR_THROW | NONE |  |
| `try` | ERROR_TRY | NONE |  |
| `catch` | ERROR_CATCH | NONE |  |
| `finally` | ERROR_FINALLY | NONE |  |
| `typeof` | OPERATOR_LOGICAL | NONE |  |
| `delete` | EXECUTION_STATEMENT | NONE |  |
| `with` | EXECUTION_STATEMENT | NONE |  |
| `new` | COMPUTATION_CALL | NONE |  |
| `extends` | TYPE_REFERENCE | NONE |  |
| `static` | METADATA_ANNOTATION | NONE |  |
| `get` | METADATA_ANNOTATION | NONE |  |
| `set` | METADATA_ANNOTATION | NONE |  |
| `constructor` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER |  |
| `of` | OPERATOR_COMPARISON | NONE |  |

## Punctuation

Syntactic punctuation tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_PUNCTUATION | NONE | Syntactic punctuation tokens |
| `)` | PARSER_PUNCTUATION | NONE |  |
| `[` | PARSER_PUNCTUATION | NONE |  |
| `]` | PARSER_PUNCTUATION | NONE |  |
| `{` | PARSER_PUNCTUATION | NONE |  |
| `}` | PARSER_PUNCTUATION | NONE |  |
| `:` | PARSER_PUNCTUATION | NONE |  |
| `'` | PARSER_PUNCTUATION | NONE |  |
| `,` | PARSER_DELIMITER | NONE |  |
| `;` | PARSER_DELIMITER | NONE |  |
| `.` | PARSER_DELIMITER | NONE |  |
| ``` | PARSER_DELIMITER | NONE |  |
| `${` | PARSER_DELIMITER | NONE |  |

## Arithmetic Operators

Mathematical and bitwise operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Mathematical and bitwise operators |
| `-` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE |  |
| `*` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE |  |
| `/` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE |  |
| `%` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE |  |
| `**` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE |  |
| `&` | OPERATOR_ARITHMETIC | Arithmetic::BITWISE | NONE |  |
| `|` | OPERATOR_ARITHMETIC | Arithmetic::BITWISE | NONE |  |
| `^` | OPERATOR_ARITHMETIC | Arithmetic::BITWISE | NONE |  |
| `~` | OPERATOR_ARITHMETIC | Arithmetic::BITWISE | NONE |  |
| `<<` | OPERATOR_ARITHMETIC | Arithmetic::BITWISE | NONE |  |
| `>>` | OPERATOR_ARITHMETIC | Arithmetic::BITWISE | NONE |  |
| `>>>` | OPERATOR_ARITHMETIC | Arithmetic::BITWISE | NONE |  |
| `++` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE |  |
| `--` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE |  |

## Logical Operators

Boolean and nullish operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `&&` | OPERATOR_LOGICAL | NONE | Boolean and nullish operators |
| `||` | OPERATOR_LOGICAL | NONE |  |
| `!` | OPERATOR_LOGICAL | NONE |  |
| `?\?` | OPERATOR_LOGICAL | NONE |  |
| `?` | OPERATOR_LOGICAL | NONE |  |
| `??` | OPERATOR_LOGICAL | NONE |  |

## Comparison Operators

Equality and relational operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `==` | OPERATOR_COMPARISON | NONE | Equality and relational operators |
| `===` | OPERATOR_COMPARISON | NONE |  |
| `!=` | OPERATOR_COMPARISON | NONE |  |
| `!==` | OPERATOR_COMPARISON | NONE |  |
| `<` | OPERATOR_COMPARISON | NONE |  |
| `>` | OPERATOR_COMPARISON | NONE |  |
| `<=` | OPERATOR_COMPARISON | NONE |  |
| `>=` | OPERATOR_COMPARISON | NONE |  |
| `instanceof` | OPERATOR_COMPARISON | NONE |  |
| `in` | OPERATOR_COMPARISON | NONE |  |

## Assignment Operators

Simple and compound assignment

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NONE | Simple and compound assignment |
| `=>` | OPERATOR_ASSIGNMENT | NONE |  |
| `+=` | OPERATOR_ASSIGNMENT | NONE |  |
| `-=` | OPERATOR_ASSIGNMENT | NONE |  |
| `*=` | OPERATOR_ASSIGNMENT | NONE |  |
| `/=` | OPERATOR_ASSIGNMENT | NONE |  |
| `%=` | OPERATOR_ASSIGNMENT | NONE |  |
| `**=` | OPERATOR_ASSIGNMENT | NONE |  |
| `&=` | OPERATOR_ASSIGNMENT | NONE |  |
| `|=` | OPERATOR_ASSIGNMENT | NONE |  |
| `^=` | OPERATOR_ASSIGNMENT | NONE |  |
| `<<=` | OPERATOR_ASSIGNMENT | NONE |  |
| `>>=` | OPERATOR_ASSIGNMENT | NONE |  |
| `>>>=` | OPERATOR_ASSIGNMENT | NONE |  |
| `&&=` | OPERATOR_ASSIGNMENT | NONE |  |
| `||=` | OPERATOR_ASSIGNMENT | NONE |  |
| `?\?=` | OPERATOR_ASSIGNMENT | NONE |  |

## Modern Operators

ES6+ operators

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `?.` | COMPUTATION_ACCESS | NONE | ES6+ operators |
| `...` | PATTERN_COLLECT | NODE_TEXT |  |

## Type Constructs

TypeScript-style types that appear in JavaScript parsing

These are included for compatibility when parsing TypeScript-like constructs or JSDoc annotations.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_annotation` | TYPE_REFERENCE | NONE | TypeScript-style types that appear in JavaScript parsing |
| `type_identifier` | TYPE_REFERENCE | NODE_TEXT |  |
| `predefined_type` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `non_null_expression` | TYPE_REFERENCE | NONE |  |
| `type_arguments` | TYPE_GENERIC | NONE |  |
| `type_assertion` | TYPE_REFERENCE | NONE |  |

## Parser Errors

Error nodes from parsing

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Error nodes from parsing |

---

*Generated from `javascript_types.def`*
