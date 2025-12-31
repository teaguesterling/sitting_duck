# Ruby Node Types

> Ruby language node type mappings for AST semantic extraction

## Language Characteristics

- **Pure OOP**: Everything is an object, including primitives
- **Dynamic typing**: No static type declarations, duck typing
- **Blocks**: First-class closures passed to methods (`do...end`, `{...}`)
- **Modules**: Mixins for code reuse without inheritance
- **Symbols**: Immutable interned strings (`:symbol`)
- **Metaprogramming**: `define_method`, `method_missing`, `eval`
- **Pattern matching**: `case...in` patterns (Ruby 3.0+)
- **Multiple assignment**: Parallel assignment and destructuring
- **Implicit returns**: Last expression is return value

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

- [Module and Class Definitions](#module-and-class-definitions)
- [Method Definitions](#method-definitions)
- [Variable and Constant Definitions](#variable-and-constant-definitions)
- [Expressions and Calls](#expressions-and-calls)
- [Identifiers and Names](#identifiers-and-names)
- [Literals](#literals)
- [Control Flow](#control-flow)
- [Loop Constructs](#loop-constructs)
- [Jump Statements](#jump-statements)
- [Exception Handling](#exception-handling)
- [Blocks and Closures](#blocks-and-closures)
- [Import Constructs](#import-constructs)
- [Comments](#comments)
- [Special Ruby Constructs](#special-ruby-constructs)
- [Operators](#operators)
- [Keywords](#keywords)
- [Punctuation and Delimiters](#punctuation-and-delimiters)
- [Arithmetic Operators](#arithmetic-operators)
- [Logical Operators](#logical-operators)
- [Comparison Operators](#comparison-operators)
- [Assignment Operators](#assignment-operators)
- [Ruby-Specific Constructs](#ruby-specific-constructs)
- [Advanced Ruby Constructs](#advanced-ruby-constructs)
- [Method Definition Variations](#method-definition-variations)
- [Module and Class Constructs](#module-and-class-constructs)
- [Access Modifiers](#access-modifiers)
- [Special Variables](#special-variables)
- [Pattern Matching](#pattern-matching)
- [Metaprogramming Constructs](#metaprogramming-constructs)
- [Additional Ruby Literals](#additional-ruby-literals)
- [Remaining Edge Cases](#remaining-edge-cases)
- [Error Handling](#error-handling)

## Module and Class Definitions

Ruby modules and classes

Ruby class hierarchy: - `Module` - container for methods and constants (mixin) - `Class` - inherits from Module, can be instantiated - Single inheritance with multiple module mixins Modules provide: - Namespacing via `ModuleName::ClassName` - Mixins via `include` (instance methods) and `extend` (class methods) - `prepend` for method chain insertion

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `module` | DEFINITION_MODULE | FIND_IDENTIFIER | Module definition - `module Name ... end` for namespacing and mixins |
| `class` | DEFINITION_CLASS | FIND_IDENTIFIER | Class definition - `class Name < Superclass ... end` |

## Method Definitions

Ruby method declarations

Ruby method features: - `def name(args) ... end` for instance methods - `def self.name` or `def ClassName.name` for class methods - No explicit return type - implicit return of last expression - Optional parentheses for parameters - Keyword arguments: `def foo(name:, age: 18)` - Block parameters: `def foo(&block)` - Splat operators: `*args` (array), `**kwargs` (hash)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `method` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Instance method definition - `def method_name(params) ... end` |
| `singleton_method` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Singleton/class method - `def self.method_name` or `def obj.method_name` |
| `alias` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Method alias - `alias new_name old_name` |

## Variable and Constant Definitions

Ruby variable scopes and assignments

Ruby variable scopes (by naming convention): - `local` - local to current scope - `@instance` - instance variable, per-object - `@@class` - class variable, shared across instances - `$global` - global variable - `CONSTANT` - constants (uppercase), warning on reassignment Assignment operators: - `=` simple assignment - `||=` assign if nil/false (memoization pattern) - `&&=` assign if truthy - `+=`, `-=`, etc. compound assignment

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assignment` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Simple assignment - `var = value` |
| `operator_assignment` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Compound assignment - `var += value`, `var ||= default` |
| `multiple_assignment` | DEFINITION_VARIABLE | Variable::MUTABLE | NONE | Multiple/parallel assignment - `a, b = 1, 2` or `a, b = arr` |
| `constant` | DEFINITION_VARIABLE | Variable::IMMUTABLE | NODE_TEXT | Constant definition - `CONSTANT = value` (uppercase names) |

## Expressions and Calls

Method invocations and access

Ruby method calls: - Parentheses optional: `puts "hello"` or `puts("hello")` - Method chaining: `str.downcase.strip.split` - Safe navigation: `obj&.method` (nil-safe) - Block passing: `arr.map { |x| x * 2 }` or `arr.map(&:to_s)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function-style call - `method(args)` |
| `method_call` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET | Method call on object - `obj.method(args)` |
| `chained_call` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET | Chained method call - `obj.method1.method2` |
| `element_reference` | COMPUTATION_ACCESS | NONE | Array/hash element access - `arr[index]` or `hash[key]` |

## Identifiers and Names

Ruby naming conventions and variable types

Ruby identifier conventions: - `snake_case` for methods and local variables - `CamelCase` for classes and modules - `SCREAMING_SNAKE` for constants - Prefix conventions indicate scope (see variables section) - Query methods end with `?`: `empty?`, `nil?` - Mutating methods end with `!`: `sort!`, `map!`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Local variable or method name |
| `constant_identifier` | NAME_IDENTIFIER | NODE_TEXT | Constant identifier - uppercase names |
| `instance_variable` | NAME_IDENTIFIER | NODE_TEXT | Instance variable - `@name` |
| `class_variable` | NAME_IDENTIFIER | NODE_TEXT | Class variable - `@@name` |
| `global_variable` | NAME_IDENTIFIER | NODE_TEXT | Global variable - `$name` |

## Literals

Ruby literal values

Ruby literal types: - Numbers: integers, floats, rationals (`1/3r`), complex (`1+2i`) - Strings: single/double quoted, heredocs, `%q{}` and `%Q{}` - Symbols: `:symbol` or `:"symbol with spaces"` - Arrays: `[1, 2, 3]` or `%w[a b c]` or `%i[a b c]` - Hashes: `{key: value}` or `{:key => value}` - Ranges: `1..10` (inclusive) or `1...10` (exclusive) - Regex: `/pattern/flags`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `integer` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Integer literal - `42`, `1_000_000`, `0xFF` |
| `float` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT | Floating-point literal - `3.14`, `1.0e10` |
| `complex` | LITERAL_NUMBER | Number::COMPLEX | NODE_TEXT | Complex number - `1+2i` |
| `rational` | LITERAL_NUMBER | Number::COMPLEX | NODE_TEXT | Rational number - `1/3r` |

## Control Flow

Conditionals and branching

Ruby control flow features: - `if`/`elsif`/`else`/`end` - standard conditional - `unless` - negated if (preferred for negative conditions) - Modifier forms: `return if condition` - `case`/`when`/`end` - pattern matching - Ternary: `condition ? true_val : false_val` - Everything is an expression (returns a value)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement - `if condition ... end` |
| `unless` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Unless statement - `unless condition ... end` (negated if) |
| `case` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Case statement - `case expr when pattern ... end` |
| `when` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | When clause - pattern in case statement |
| `if_modifier` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If modifier - `expr if condition` |
| `unless_modifier` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Unless modifier - `expr unless condition` |

## Loop Constructs

Ruby iteration mechanisms

Ruby loop constructs: - `while` - condition-based loop - `until` - negated while - `for x in collection` - rarely used, prefer iterators - Modifier forms: `expr while condition` Preferred: Block iterators - `collection.each { |item| ... }` - `5.times { ... }` - `1.upto(10) { |n| ... }`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `while` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While loop - `while condition ... end` |
| `until` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Until loop - `until condition ... end` (negated while) |
| `for` | FLOW_LOOP | Loop::ITERATOR | NONE | For loop - `for x in collection ... end` |
| `while_modifier` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While modifier - `expr while condition` |
| `until_modifier` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Until modifier - `expr until condition` |

## Jump Statements

Control flow transfer

Ruby jump statements: - `return` - exit method with value - `break` - exit loop or block - `next` - skip to next iteration (like `continue`) - `redo` - restart current iteration - `retry` - restart from begin block

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `return` | FLOW_JUMP | Jump::RETURN | NONE | Return statement - exits method with value |
| `break` | FLOW_JUMP | Jump::BREAK | NONE | Break statement - exits loop or block |
| `next` | FLOW_JUMP | Jump::CONTINUE | NONE | Next statement - skips to next iteration |
| `redo` | FLOW_JUMP | Jump::CONTINUE | NONE | Redo statement - restarts current iteration |
| `retry` | FLOW_JUMP | Jump::CONTINUE | NONE | Retry statement - restarts from begin block |

## Exception Handling

Ruby error handling constructs

Ruby exception handling: - `begin ... rescue ... ensure ... end` - `raise` to throw exceptions - Multiple rescue clauses for different exception types - `retry` to re-execute begin block - Modifier form: `expr rescue default_value`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `begin` | ERROR_TRY | NONE | Begin block - starts exception handling region |
| `rescue` | ERROR_CATCH | NONE | Rescue clause - catches exceptions |
| `ensure` | ERROR_FINALLY | NONE | Ensure clause - always executed (like finally) |
| `raise` | ERROR_THROW | NONE | Raise statement - throws exception |

## Blocks and Closures

Ruby block and lambda constructs

Ruby closures: - Blocks: `{ |params| body }` or `do |params| body end` - Procs: `Proc.new { }` or `proc { }` - Lambdas: `lambda { }` or `-> { }` (strict arity) Blocks are passed implicitly to methods: - `yield` calls the block - `block_given?` checks for block - `&block` captures block as Proc parameter

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | ORGANIZATION_BLOCK | NONE | Block - `{ |params| ... }` single-line form |
| `do_block` | ORGANIZATION_BLOCK | NONE | Do block - `do |params| ... end` multi-line form |
| `lambda` | COMPUTATION_CLOSURE | NONE | Lambda - `-> { }` or `lambda { }` |
| `proc` | COMPUTATION_CLOSURE | NONE | Proc - `proc { }` or `Proc.new { }` |

## Import Constructs

Ruby file loading mechanisms

Ruby file loading: - `require 'lib'` - load from $LOAD_PATH (once) - `require_relative './file'` - load relative to current file - `load 'file.rb'` - always reload - `include Module` - mix in module methods as instance methods - `extend Module` - mix in module methods as class methods - `prepend Module` - insert module before class in method chain

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `require` | EXTERNAL_IMPORT | NONE | Require - loads library from load path |
| `require_relative` | EXTERNAL_IMPORT | NONE | Require relative - loads file relative to current |
| `load` | EXTERNAL_IMPORT | NONE | Load - loads and re-evaluates file |
| `include` | EXTERNAL_IMPORT | FIND_IDENTIFIER | Include - mixes in module as instance methods |
| `extend` | EXTERNAL_IMPORT | FIND_IDENTIFIER | Extend - mixes in module as class methods |
| `prepend` | EXTERNAL_IMPORT | FIND_IDENTIFIER | Prepend - inserts module before class in method chain |

## Comments

Documentation and annotation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment - `# single line comment` |

## Special Ruby Constructs

Ruby-specific language features

Ruby special constructs: - `yield` - invoke passed block - `super` - call parent method - `self` - current object reference

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `yield` | FLOW_SYNC | NONE | Yield - invokes block passed to method |
| `super` | COMPUTATION_CALL | NONE | Super - calls parent class method |
| `self` | NAME_SCOPED | NODE_TEXT | Self - reference to current object |

## Operators

Ruby operators and expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `binary` | OPERATOR_ARITHMETIC | NONE | Binary expression - two operand operation |
| `unary` | OPERATOR_ARITHMETIC | NONE | Unary expression - single operand operation |
| `assignment_operator` | OPERATOR_ASSIGNMENT | NONE | Assignment operator node |

## Keywords

Ruby reserved words with special meaning

Ruby keywords are reserved and cannot be used as identifiers. They are classified by their semantic function with the IS_KEYWORD flag.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `def` | DEFINITION_FUNCTION | NONE | Ruby reserved words with special meaning |
| `end` | ORGANIZATION_BLOCK | NONE |  |
| `class` | DEFINITION_CLASS | NONE |  |
| `module` | DEFINITION_MODULE | NONE |  |

## Punctuation and Delimiters

Syntactic markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `'` | PARSER_DELIMITER | NONE | Syntactic markers |
| `,` | PARSER_PUNCTUATION | NONE |  |
| `.` | PARSER_PUNCTUATION | NONE |  |
| `:` | PARSER_PUNCTUATION | NONE |  |
| `;` | PARSER_PUNCTUATION | NONE |  |
| `(` | PARSER_DELIMITER | NONE |  |
| `)` | PARSER_DELIMITER | NONE |  |
| `[` | PARSER_DELIMITER | NONE |  |
| `]` | PARSER_DELIMITER | NONE |  |
| `{` | PARSER_DELIMITER | NONE |  |
| `}` | PARSER_DELIMITER | NONE |  |
| `|` | PARSER_DELIMITER | NONE |  |

## Arithmetic Operators

Math and bitwise operations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_ARITHMETIC | NONE | Math and bitwise operations |
| `-` | OPERATOR_ARITHMETIC | NONE |  |
| `*` | OPERATOR_ARITHMETIC | NONE |  |
| `/` | OPERATOR_ARITHMETIC | NONE |  |
| `%` | OPERATOR_ARITHMETIC | NONE |  |
| `**` | OPERATOR_ARITHMETIC | NONE |  |
| `&` | OPERATOR_ARITHMETIC | NONE |  |
| `^` | OPERATOR_ARITHMETIC | NONE |  |
| `~` | OPERATOR_ARITHMETIC | NONE |  |
| `<<` | OPERATOR_ARITHMETIC | NONE |  |
| `>>` | OPERATOR_ARITHMETIC | NONE |  |

## Logical Operators

Boolean operators

Ruby has both symbolic and keyword logical operators: - `&&` / `and` - logical AND (different precedence) - `||` / `or` - logical OR (different precedence) - `!` / `not` - logical NOT

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `&&` | OPERATOR_LOGICAL | NONE | Boolean operators |
| `||` | OPERATOR_LOGICAL | NONE |  |
| `!` | OPERATOR_LOGICAL | NONE |  |

## Comparison Operators

Equality and ordering comparisons

Ruby comparison operators: - `==` - equality by value - `===` - case equality (pattern matching) - `<=>` - spaceship operator (returns -1, 0, 1) - `=~` / `!~` - regex match/non-match

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `==` | OPERATOR_COMPARISON | NONE | Equality and ordering comparisons |
| `!=` | OPERATOR_COMPARISON | NONE |  |
| `<` | OPERATOR_COMPARISON | NONE |  |
| `>` | OPERATOR_COMPARISON | NONE |  |
| `<=` | OPERATOR_COMPARISON | NONE |  |
| `>=` | OPERATOR_COMPARISON | NONE |  |
| `<=>` | OPERATOR_COMPARISON | NONE |  |
| `===` | OPERATOR_COMPARISON | NONE |  |
| `=~` | OPERATOR_COMPARISON | NONE |  |
| `!~` | OPERATOR_COMPARISON | NONE |  |

## Assignment Operators

Assignment and compound assignment

Notable Ruby idioms: - `||=` - memoization: `@cached ||= expensive_computation` - `&&=` - conditional assignment if truthy

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NONE | Assignment and compound assignment |
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
| `&&=` | OPERATOR_ASSIGNMENT | NONE |  |
| `||=` | OPERATOR_ASSIGNMENT | NONE |  |

## Ruby-Specific Constructs

Additional Ruby AST nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string_content` | LITERAL_STRING | NONE | String content - text inside string literal |
| `body_statement` | EXECUTION_STATEMENT | NONE | Body statement - method/class body |
| `argument_list` | ORGANIZATION_LIST | NONE | Argument list - method call arguments |
| `program` | DEFINITION_MODULE | NONE | Program root node |
| `method_parameters` | ORGANIZATION_LIST | NONE | Method parameters |
| `in_clause` | FLOW_LOOP | NONE | In clause - for loop iterator |
| `bare_symbol` | LITERAL_STRING | NODE_TEXT | Bare symbol - unquoted symbol |
| `parameter_list` | ORGANIZATION_LIST | NONE | Parameter list |
| `block_parameters` | ORGANIZATION_LIST | NONE | Block parameters - `|x, y|` |
| `parenthesized_statements` | ORGANIZATION_BLOCK | NONE | Parenthesized statements |
| `elsif_clause` | FLOW_CONDITIONAL | NONE | Elsif clause |
| `else_clause` | FLOW_CONDITIONAL | NONE | Else clause |
| `rescue_clause` | ERROR_CATCH | NONE | Rescue clause |
| `ensure_clause` | ERROR_FINALLY | NONE | Ensure clause |
| `when_clause` | FLOW_CONDITIONAL | NONE | When clause in case statement |

## Advanced Ruby Constructs

Scope resolution, splat, and complex structures

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `scope_resolution` | COMPUTATION_ACCESS | NONE | Scope resolution - `Module::Constant` or `Module::Class` |
| `::` | COMPUTATION_ACCESS | NONE | Scope resolution operator `::` |
| `splat_argument` | PATTERN_COLLECT | NONE | Splat argument - `*args` in call |
| `hash_splat_argument` | PATTERN_COLLECT | NONE | Hash splat argument - `**kwargs` in call |
| `keyword_splat` | PATTERN_COLLECT | NONE | Keyword splat - `**options` |
| `interpolation` | LITERAL_STRING | NONE | String interpolation - `#{expr}` |
| `escape_sequence` | LITERAL_STRING | NONE | Escape sequence in string |
| `heredoc_beginning` | LITERAL_STRING | NODE_TEXT | Heredoc start marker |
| `heredoc_end` | LITERAL_STRING | NODE_TEXT | Heredoc end marker |
| `heredoc_body` | LITERAL_STRING | NONE | Heredoc body content |

## Method Definition Variations

Alternative method definition forms

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `singleton_method` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Singleton method (class method form) |
| `alias_method` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Method alias |
| `undef` | EXECUTION_STATEMENT | FIND_IDENTIFIER | Undef - removes method definition |

## Module and Class Constructs

Inheritance and type references

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `superclass` | TYPE_REFERENCE | FIND_IDENTIFIER | Superclass reference - `< ParentClass` |

## Access Modifiers

Method visibility control

Ruby access modifiers: - `public` - callable from anywhere (default) - `protected` - callable from same class or subclasses - `private` - callable only with implicit receiver (no `self.`)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `private` | METADATA_ANNOTATION | NONE | Method visibility control |
| `protected` | METADATA_ANNOTATION | NONE |  |
| `public` | METADATA_ANNOTATION | NONE |  |

## Special Variables

Ruby magic constants and variables

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `__FILE__` | NAME_SCOPED | NODE_TEXT | Current file path |
| `__LINE__` | NAME_SCOPED | NODE_TEXT | Current line number |
| `__dir__` | NAME_SCOPED | NODE_TEXT | Current directory |
| `__ENCODING__` | NAME_SCOPED | NODE_TEXT | Current encoding |

## Pattern Matching

Ruby 3.0+ pattern matching constructs

Pattern matching syntax: - `case expr in pattern ... end` - Variable binding: `in x` captures value - Array patterns: `in [a, b, *rest]` - Hash patterns: `in {name:, age:}` - Guard clauses: `in pattern if condition`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `case_match` | PATTERN_MATCH | NONE | Case match expression - `case ... in` |
| `in_pattern` | PATTERN_MATCH | NONE | In pattern - pattern matching clause |
| `variable_pattern` | PATTERN_DESTRUCTURE | FIND_IDENTIFIER | Variable pattern - captures value in pattern |
| `array_pattern` | PATTERN_DESTRUCTURE | NONE | Array pattern - `[a, b, c]` |
| `hash_pattern` | PATTERN_DESTRUCTURE | NONE | Hash pattern - `{key:}` |
| `rest_pattern` | PATTERN_COLLECT | NONE | Rest pattern - `*rest` |
| `keyword_pattern` | PATTERN_MATCH | NONE | Keyword pattern |
| `alternative_pattern` | PATTERN_MATCH | NONE | Alternative pattern - `a | b` |

## Metaprogramming Constructs

Dynamic method and attribute generation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `attr_reader` | METADATA_ANNOTATION | FIND_IDENTIFIER | attr_reader - generates getter methods |
| `attr_writer` | METADATA_ANNOTATION | FIND_IDENTIFIER | attr_writer - generates setter methods |
| `attr_accessor` | METADATA_ANNOTATION | FIND_IDENTIFIER | attr_accessor - generates getter and setter methods |
| `define_method` | DEFINITION_FUNCTION | FIND_IDENTIFIER | define_method - dynamically defines instance method |
| `define_singleton_method` | DEFINITION_FUNCTION | FIND_IDENTIFIER | define_singleton_method - dynamically defines class method |

## Additional Ruby Literals

Various literal forms

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `word_literal` | LITERAL_STRING | NODE_TEXT | Various literal forms |
| `symbol_literal` | LITERAL_STRING | NODE_TEXT |  |
| `regex_literal` | LITERAL_STRING | NODE_TEXT |  |
| `hash_key_symbol` | LITERAL_STRING | NODE_TEXT |  |
| `bare_string` | LITERAL_STRING | NODE_TEXT |  |
| `delimited_symbol` | LITERAL_STRING | NODE_TEXT |  |
| `chained_string` | LITERAL_STRING | NODE_TEXT |  |
| `heredoc_content` | LITERAL_STRING | NONE |  |

## Remaining Edge Cases

Miscellaneous AST nodes for completeness

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `%i(` | PARSER_DELIMITER | NONE | Miscellaneous AST nodes for completeness |
| `%w(` | PARSER_DELIMITER | NONE |  |
| `encoding` | NAME_SCOPED | NODE_TEXT |  |
| `optional_parameter` | DEFINITION_VARIABLE | FIND_IDENTIFIER |  |
| `block_body` | ORGANIZATION_BLOCK | NONE |  |
| `file` | NAME_SCOPED | NODE_TEXT |  |
| `destructured_parameter` | PATTERN_DESTRUCTURE | FIND_IDENTIFIER |  |
| `lambda_parameters` | ORGANIZATION_LIST | NONE |  |
| `#{` | PARSER_DELIMITER | NONE |  |
| `line` | NAME_SCOPED | NODE_TEXT |  |
| `->` | OPERATOR_ASSIGNMENT | NONE |  |
| `keyword_parameter` | DEFINITION_VARIABLE | FIND_IDENTIFIER |  |
| `hash_splat_nil` | PATTERN_COLLECT | NONE |  |
| `pair` | LITERAL_STRUCTURED | NONE |  |
| `=>` | OPERATOR_ASSIGNMENT | NONE |  |
| `exceptions` | ERROR_CATCH | NONE |  |
| `exception_variable` | ERROR_CATCH | FIND_IDENTIFIER |  |
| `hash_splat_parameter` | PATTERN_COLLECT | FIND_IDENTIFIER |  |
| `conditional` | FLOW_CONDITIONAL | NONE |  |
| `?` | FLOW_CONDITIONAL | NONE |  |
| `&.` | COMPUTATION_ACCESS | NONE |  |
| `..` | LITERAL_STRUCTURED | NONE |  |
| `block_argument` | ORGANIZATION_LIST | NONE |  |
| `defined?` | COMPUTATION_CALL | NODE_TEXT |  |
| `block_parameter` | ORGANIZATION_LIST | FIND_IDENTIFIER |  |
| `splat_parameter` | PATTERN_COLLECT | FIND_IDENTIFIER |  |
| `...` | PATTERN_COLLECT | NONE |  |

## Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

## Other Node Types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal - double or single quoted |
| `character` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Character literal - `?a` |
| `symbol` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Symbol - `:name` immutable interned string |
| `simple_symbol` | LITERAL_STRING | String::LITERAL | NODE_TEXT | Simple symbol - unquoted symbol `:name` |
| `regex` | LITERAL_STRING | String::REGEX | NODE_TEXT | Regular expression - `/pattern/flags` |
| `string_array` | LITERAL_STRUCTURED | Structured::SEQUENCE | NODE_TEXT | String array literal - `%w[a b c]` |
| `symbol_array` | LITERAL_STRUCTURED | Structured::SEQUENCE | NODE_TEXT | Symbol array literal - `%i[a b c]` |
| `array` | LITERAL_STRUCTURED | NONE | Array literal - `[1, 2, 3]` |
| `hash` | LITERAL_STRUCTURED | NONE | Hash literal - `{key: value}` |
| `range` | LITERAL_STRUCTURED | NONE | Range literal - `1..10` or `1...10` |
| `true` | LITERAL_ATOMIC | NODE_TEXT | Boolean true |
| `false` | LITERAL_ATOMIC | NODE_TEXT | Boolean false |
| `nil` | LITERAL_ATOMIC | NODE_TEXT | Nil value - Ruby's null |
| `if` | FLOW_CONDITIONAL | NONE |  |
| `unless` | FLOW_CONDITIONAL | NONE |  |
| `else` | FLOW_CONDITIONAL | NONE |  |
| `elsif` | FLOW_CONDITIONAL | NONE |  |
| `case` | FLOW_CONDITIONAL | NONE |  |
| `when` | FLOW_CONDITIONAL | NONE |  |
| `then` | FLOW_CONDITIONAL | NONE |  |
| `while` | FLOW_LOOP | NONE |  |
| `until` | FLOW_LOOP | NONE |  |
| `for` | FLOW_LOOP | NONE |  |
| `in` | OPERATOR_COMPARISON | NONE |  |
| `do` | ORGANIZATION_BLOCK | NONE |  |
| `begin` | ERROR_TRY | NONE |  |
| `rescue` | ERROR_CATCH | NONE |  |
| `ensure` | ERROR_FINALLY | NONE |  |
| `raise` | ERROR_THROW | NONE |  |
| `return` | FLOW_JUMP | NONE |  |
| `break` | FLOW_JUMP | NONE |  |
| `next` | FLOW_JUMP | NONE |  |
| `redo` | FLOW_JUMP | NONE |  |
| `retry` | FLOW_JUMP | NONE |  |
| `yield` | FLOW_SYNC | NONE |  |
| `super` | COMPUTATION_CALL | NONE |  |
| `and` | OPERATOR_LOGICAL | NONE |  |
| `or` | OPERATOR_LOGICAL | NONE |  |
| `not` | OPERATOR_LOGICAL | NONE |  |

---

*Generated from `ruby_types.def`*
