# Python Node Types

> Python language node type mappings for AST semantic extraction

## Language Characteristics

Python has several unique features that affect AST mapping:

- **Indentation-based blocks**: No explicit braces; tree-sitter uses 'block' nodes
- **Decorators**: First-class syntax via `decorated_definition` wrapper nodes
- **Comprehensions**: Distinct syntax for list/dict/set comprehensions and generators
- **Type hints**: Optional type annotations via `type` and `generic_type` nodes
- **Async/await**: Async functions are `function_definition` with `async` keyword child
- **Pattern matching**: Python 3.10+ match/case statements
- **Walrus operator**: Named expressions with `:=`

## Node Categories

- [Function Definitions](#function-definitions)
- [Class Definitions](#class-definitions)
- [Variable Definitions](#variable-definitions)
- [Function Calls](#function-calls)
- [Operators and Expressions](#operators-and-expressions)
- [Attribute and Index Access](#attribute-and-index-access)
- [Literal Values](#literal-values)
- [Control Flow](#control-flow)
- [Error Handling](#error-handling)
- [Modules and Imports](#modules-and-imports)
- [Type Annotations](#type-annotations)
- [Comprehensions and Generators](#comprehensions-and-generators)
- [Decorators and Metadata](#decorators-and-metadata)
- [Pattern Matching](#pattern-matching)
- [Scoped References](#scoped-references)
- [Structural Elements](#structural-elements)
- [Names and Identifiers](#names-and-identifiers)
- [Statement Nodes](#statement-nodes)
- [Comments](#comments)
- [Keywords](#keywords)
- [Operator Tokens](#operator-tokens)
- [Punctuation](#punctuation)
- [Parse Errors](#parse-errors)

## Function Definitions

Python function and method definitions

Python functions use FIND_IDENTIFIER to locate the function name which is a direct child of `function_definition`. The FUNCTION_WITH_PARAMS native strategy extracts the full signature including parameters and return type. Note: Python does NOT have a distinct `async_function_definition` node in tree-sitter-python. Async functions are regular `function_definition` nodes with an `async` keyword child. The async keyword itself gets FLOW_SYNC.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Python function and method definitions |
| `async_function_definition` | DEFINITION_FUNCTION | Function::ASYNC | FIND_IDENTIFIER |  |

## Class Definitions

Python class definitions including inheritance

Classes use CLASS_WITH_METHODS native extraction to capture the class name, base classes, and method signatures. Python classes always have bodies.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_definition` | DEFINITION_CLASS | FIND_IDENTIFIER | Python class definitions including inheritance |

## Variable Definitions

Variable assignments and parameter declarations

Python variables are always mutable (no const/final keyword). The refinements distinguish between regular assignments (MUTABLE) and function parameters (PARAMETER). Fields are not separately tracked since Python doesn't have formal field declarations.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assignment` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Variable assignments and parameter declarations |
| `typed_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `default_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `typed_default_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `named_expression` | DEFINITION_VARIABLE | FIND_IDENTIFIER |  |

## Function Calls

Function and method invocation

FIND_CALL_TARGET handles both simple calls (`foo()`) and method calls (`obj.method()`), extracting just the function/method name. FUNCTION_CALL native extraction captures the full call including arguments.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function and method invocation |

## Operators and Expressions

Arithmetic, logical, comparison, and assignment operators

Python operator nodes include both the compound expression and the individual operator tokens. Compound operators use NONE for name extraction since the operator itself carries the meaning.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `binary_operator` | OPERATOR_ARITHMETIC | NONE | Arithmetic, logical, comparison, and assignment operators |
| `unary_operator` | OPERATOR_ARITHMETIC | NONE |  |
| `comparison_operator` | OPERATOR_COMPARISON | NONE |  |
| `boolean_operator` | OPERATOR_LOGICAL | NONE |  |
| `not_operator` | OPERATOR_LOGICAL | NONE |  |
| `augmented_assignment` | OPERATOR_ASSIGNMENT | NONE |  |

## Attribute and Index Access

Dot notation and subscript access patterns

Attribute access (`obj.attr`) uses NODE_TEXT to capture the attribute name. Subscript access (`obj[key]`) doesn't have a meaningful name.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Dot notation and subscript access patterns |
| `attribute` | COMPUTATION_ACCESS | NODE_TEXT |  |
| `subscript` | COMPUTATION_ACCESS | NONE |  |
| `slice` | COMPUTATION_ACCESS | NONE |  |

## Literal Values

String, numeric, and structured literal values

Literals use NODE_TEXT to capture their textual representation. Structured literals (list, dict, set, tuple) use refinements to distinguish container types. Note: String literals in Python are complex due to f-strings, raw strings, and concatenated strings. The tree-sitter grammar breaks these into multiple node types.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `string` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String, numeric, and structured literal values |
| `string_content` | LITERAL_STRING | NONE |  |
| `concatenated_string` | LITERAL_STRING | NODE_TEXT |  |
| `escape_sequence` | LITERAL_STRING | NONE |  |
| `interpolation` | LITERAL_STRING | NONE |  |
| `escape_interpolation` | LITERAL_STRING | NONE |  |
| `format_specifier` | LITERAL_STRING | NONE |  |
| `format_expression` | LITERAL_STRING | NONE |  |
| `conversion` | LITERAL_STRING | NONE |  |
| `interpolation_conversion` | LITERAL_STRING | NONE |  |
| `integer` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT |  |
| `float` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT |  |
| `true` | LITERAL_ATOMIC | NODE_TEXT |  |
| `false` | LITERAL_ATOMIC | NODE_TEXT |  |
| `none` | LITERAL_ATOMIC | NODE_TEXT |  |
| `ellipsis` | LITERAL_ATOMIC | NODE_TEXT |  |
| `list` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE |  |
| `tuple` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE |  |
| `dictionary` | LITERAL_STRUCTURED | Structured::MAPPING | NONE |  |
| `pair` | LITERAL_STRUCTURED | Structured::MAPPING | NONE |  |
| `set` | LITERAL_STRUCTURED | Structured::SET | NONE |  |

## Control Flow

Conditional statements, loops, and jump statements

Python control flow uses refinements to distinguish between: - Binary conditionals (if/else) vs multiway (match/case) - Iterator loops (for) vs conditional loops (while) - Different jump types (return, break, continue)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Conditional statements, loops, and jump statements |
| `else_clause` | FLOW_CONDITIONAL | NONE |  |
| `elif_clause` | FLOW_CONDITIONAL | NONE |  |
| `conditional_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE |  |
| `match_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `case_clause` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `for_statement` | FLOW_LOOP | Loop::ITERATOR | NONE |  |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE |  |
| `for_in_clause` | FLOW_LOOP | NONE |  |
| `if_clause` | FLOW_CONDITIONAL | NONE |  |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE |  |
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE |  |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE |  |

## Error Handling

Exception handling constructs

Python uses try/except/finally for exception handling. The `raise` and `assert` statements both use ERROR_THROW since they can raise exceptions.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_statement` | ERROR_TRY | NONE | Exception handling constructs |
| `except_clause` | ERROR_CATCH | NONE |  |
| `finally_clause` | ERROR_FINALLY | NONE |  |
| `raise_statement` | ERROR_THROW | NONE |  |
| `assert_statement` | ERROR_THROW | NONE |  |

## Modules and Imports

Module structure and import statements

Python modules are the top-level `module` node. Import statements come in several forms: - `import module` (EXTERNAL_IMPORT) - `from module import name` (EXTERNAL_IMPORT) - `from . import name` (relative import) - `from module import *` (wildcard import)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `module` | DEFINITION_MODULE | NONE | Module structure and import statements |
| `import_statement` | EXTERNAL_IMPORT | NONE |  |
| `import_from_statement` | EXTERNAL_IMPORT | NONE |  |
| `aliased_import` | EXTERNAL_IMPORT | FIND_IDENTIFIER |  |
| `relative_import` | EXTERNAL_IMPORT | NONE |  |
| `import_prefix` | EXTERNAL_IMPORT | NONE |  |
| `wildcard_import` | EXTERNAL_IMPORT | NONE |  |
| `future_import_statement` | EXTERNAL_IMPORT | NONE |  |
| `__future__` | EXTERNAL_IMPORT | NODE_TEXT |  |

## Type Annotations

Type hints and generic types (PEP 484+)

Python's optional type system uses annotations that don't affect runtime behavior. The `type` node represents a type reference, while `generic_type` handles parameterized types like `List[int]` or `Dict[str, Any]`.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type` | TYPE_REFERENCE | NODE_TEXT | Type hints and generic types (PEP 484+) |
| `generic_type` | TYPE_GENERIC | NODE_TEXT |  |
| `type_parameter` | TYPE_GENERIC | FIND_IDENTIFIER |  |
| `->` | TYPE_REFERENCE | NONE |  |
| `type_conversion` | TYPE_REFERENCE | NONE |  |

## Comprehensions and Generators

List/dict/set comprehensions and generator expressions

Python comprehensions are a distinct syntax for creating collections via iteration. They map to TRANSFORM_QUERY with refinements: - SIMPLE: List/dict/set comprehensions `[x for x in items]` - NESTED: Generator expressions `(x for x in items)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `list_comprehension` | TRANSFORM_QUERY | Query::SIMPLE | NONE | List/dict/set comprehensions and generator expressions |
| `dictionary_comprehension` | TRANSFORM_QUERY | Query::SIMPLE | NONE |  |
| `set_comprehension` | TRANSFORM_QUERY | Query::SIMPLE | NONE |  |
| `generator_expression` | TRANSFORM_QUERY | Query::NESTED | NONE |  |

## Decorators and Metadata

Decorator syntax and annotation patterns

Decorators are a form of metaprogramming that wrap functions/classes. The `decorated_definition` node wraps the actual definition and contains one or more `decorator` children.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `decorator` | NAME_ATTRIBUTE | FIND_IDENTIFIER | Decorator syntax and annotation patterns |
| `decorated_definition` | METADATA_ANNOTATION | NONE |  |
| `as_clause` | METADATA_ANNOTATION | FIND_IDENTIFIER |  |
| `global_statement` | METADATA_ANNOTATION | NONE |  |
| `nonlocal_statement` | METADATA_ANNOTATION | NONE |  |

## Pattern Matching

Structural pattern matching constructs (PEP 634)

Python 3.10 introduced match/case statements with various pattern types: - PATTERN_DESTRUCTURE: Extracts values from structures - PATTERN_MATCH: Matches against specific patterns - PATTERN_COLLECT: Captures variable-length portions (*args)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `case_pattern` | PATTERN_MATCH | NONE | Structural pattern matching constructs (PEP 634) |
| `class_pattern` | PATTERN_MATCH | FIND_IDENTIFIER |  |
| `keyword_pattern` | PATTERN_MATCH | FIND_IDENTIFIER |  |
| `complex_pattern` | PATTERN_MATCH | NONE |  |
| `union_pattern` | PATTERN_MATCH | NONE |  |
| `value_pattern` | PATTERN_MATCH | NONE |  |
| `pattern_list` | PATTERN_DESTRUCTURE | NONE |  |
| `as_pattern_target` | PATTERN_DESTRUCTURE | FIND_IDENTIFIER |  |
| `as_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `tuple_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `list_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `attribute_pattern` | PATTERN_DESTRUCTURE | FIND_IDENTIFIER |  |
| `list_splat_pattern` | PATTERN_COLLECT | NODE_TEXT |  |
| `dictionary_splat_pattern` | PATTERN_COLLECT | NODE_TEXT |  |
| `list_splat` | PATTERN_COLLECT | NONE |  |
| `dictionary_splat` | PATTERN_COLLECT | NONE |  |
| `splat_pattern` | PATTERN_COLLECT | NONE |  |

## Scoped References

Self, super, and cls references

Python uses `self` for instance references and `cls` for class references by convention. `super()` provides parent class access.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `self` | NAME_SCOPED | NODE_TEXT | Self, super, and cls references |
| `super` | NAME_SCOPED | NODE_TEXT |  |
| `cls` | NAME_SCOPED | NODE_TEXT |  |

## Structural Elements

Blocks, lists, and organizational nodes

These nodes provide structure but don't have semantic meaning themselves. They organize other nodes into logical groups.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | ORGANIZATION_BLOCK | NONE | Blocks, lists, and organizational nodes |
| `parameters` | ORGANIZATION_LIST | NONE |  |
| `argument_list` | ORGANIZATION_LIST | NONE |  |
| `keyword_argument` | ORGANIZATION_LIST | FIND_IDENTIFIER |  |
| `expression_list` | ORGANIZATION_LIST | NONE |  |
| `lambda_parameters` | ORGANIZATION_LIST | NONE |  |
| `with_statement` | ORGANIZATION_BLOCK | NONE |  |
| `with_clause` | ORGANIZATION_BLOCK | NONE |  |
| `with_item` | COMPUTATION_EXPRESSION | NONE |  |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE |  |

## Names and Identifiers

Identifier nodes and qualified names

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `dotted_name` | NAME_QUALIFIED | NODE_TEXT | Identifier nodes and qualified names |
| `_` | NAME_IDENTIFIER | NODE_TEXT |  |

## Statement Nodes

Expression statements and execution constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `expression_statement` | EXECUTION_STATEMENT | NONE | Expression statements and execution constructs |
| `pass_statement` | EXECUTION_STATEMENT | NONE |  |
| `delete_statement` | EXECUTION_MUTATION | NONE |  |
| `print_statement` | EXECUTION_STATEMENT_CALL | NONE |  |
| `exec_statement` | EXECUTION_STATEMENT_CALL | NONE |  |

## Comments

Comment nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NONE | Comment nodes |

## Keywords

Python reserved words as syntax tokens

Keywords are marked with IS_KEYWORD flag and get the same semantic type as the constructs they introduce. This enables semantic queries that include or exclude keyword tokens as needed.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `def` | DEFINITION_FUNCTION | NONE | Python reserved words as syntax tokens |
| `class` | DEFINITION_CLASS | NONE |  |
| `lambda` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET |  |
| `if` | FLOW_CONDITIONAL | NONE |  |
| `else` | FLOW_CONDITIONAL | NONE |  |
| `elif` | FLOW_CONDITIONAL | NONE |  |
| `for` | FLOW_LOOP | NONE |  |
| `while` | FLOW_LOOP | NONE |  |
| `match` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `case` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `return` | FLOW_JUMP | NONE |  |
| `break` | FLOW_JUMP | NONE |  |
| `continue` | FLOW_JUMP | NONE |  |
| `pass` | EXECUTION_STATEMENT | NONE |  |
| `async` | FLOW_SYNC | NONE |  |
| `await` | FLOW_SYNC | NONE |  |
| `yield` | FLOW_SYNC | NONE |  |
| `with` | FLOW_SYNC | NONE |  |
| `import` | EXTERNAL_IMPORT | NONE |  |
| `from` | EXTERNAL_IMPORT | NONE |  |
| `try` | ERROR_TRY | NONE |  |
| `except` | ERROR_CATCH | NONE |  |
| `finally` | ERROR_FINALLY | NONE |  |
| `raise` | ERROR_THROW | NONE |  |
| `assert` | ERROR_THROW | NONE |  |
| `as` | METADATA_ANNOTATION | NONE |  |
| `global` | METADATA_ANNOTATION | NONE |  |
| `nonlocal` | METADATA_ANNOTATION | NONE |  |
| `del` | EXECUTION_MUTATION | NONE |  |
| `print` | EXECUTION_STATEMENT_CALL | NONE |  |
| `exec` | EXECUTION_STATEMENT_CALL | NONE |  |

## Operator Tokens

Individual operator symbols

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `+` | OPERATOR_ARITHMETIC | NONE | Individual operator symbols |
| `-` | OPERATOR_ARITHMETIC | NONE |  |
| `*` | OPERATOR_ARITHMETIC | NONE |  |
| `/` | OPERATOR_ARITHMETIC | NONE |  |
| `%` | OPERATOR_ARITHMETIC | NONE |  |
| `**` | OPERATOR_ARITHMETIC | NONE |  |
| `//` | OPERATOR_ARITHMETIC | NONE |  |
| `@` | OPERATOR_ARITHMETIC | NONE |  |
| `&` | OPERATOR_ARITHMETIC | NONE |  |
| `|` | OPERATOR_ARITHMETIC | NONE |  |
| `^` | OPERATOR_ARITHMETIC | NONE |  |
| `~` | OPERATOR_ARITHMETIC | NONE |  |
| `<<` | OPERATOR_ARITHMETIC | NONE |  |
| `>>` | OPERATOR_ARITHMETIC | NONE |  |
| `and` | OPERATOR_LOGICAL | NONE |  |
| `or` | OPERATOR_LOGICAL | NONE |  |
| `not` | OPERATOR_LOGICAL | NONE |  |
| `==` | OPERATOR_COMPARISON | NONE |  |
| `!=` | OPERATOR_COMPARISON | NONE |  |
| `<` | OPERATOR_COMPARISON | NONE |  |
| `>` | OPERATOR_COMPARISON | NONE |  |
| `<=` | OPERATOR_COMPARISON | NONE |  |
| `>=` | OPERATOR_COMPARISON | NONE |  |
| `<>` | OPERATOR_COMPARISON | NONE |  |
| `is` | OPERATOR_COMPARISON | NONE |  |
| `in` | OPERATOR_COMPARISON | NONE |  |
| `not in` | OPERATOR_COMPARISON | NONE |  |
| `is not` | OPERATOR_COMPARISON | NONE |  |
| `=` | OPERATOR_ASSIGNMENT | NONE |  |
| `+=` | OPERATOR_ASSIGNMENT | NONE |  |
| `-=` | OPERATOR_ASSIGNMENT | NONE |  |
| `*=` | OPERATOR_ASSIGNMENT | NONE |  |
| `/=` | OPERATOR_ASSIGNMENT | NONE |  |
| `%=` | OPERATOR_ASSIGNMENT | NONE |  |
| `**=` | OPERATOR_ASSIGNMENT | NONE |  |
| `//=` | OPERATOR_ASSIGNMENT | NONE |  |
| `@=` | OPERATOR_ASSIGNMENT | NONE |  |
| `&=` | OPERATOR_ASSIGNMENT | NONE |  |
| `|=` | OPERATOR_ASSIGNMENT | NONE |  |
| `^=` | OPERATOR_ASSIGNMENT | NONE |  |
| `<<=` | OPERATOR_ASSIGNMENT | NONE |  |
| `>>=` | OPERATOR_ASSIGNMENT | NONE |  |
| `:=` | OPERATOR_ASSIGNMENT | NONE |  |

## Punctuation

Delimiters, separators, and syntax markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NONE | Delimiters, separators, and syntax markers |
| `)` | PARSER_DELIMITER | NONE |  |
| `[` | PARSER_DELIMITER | NONE |  |
| `]` | PARSER_DELIMITER | NONE |  |
| `{` | PARSER_DELIMITER | NONE |  |
| `}` | PARSER_DELIMITER | NONE |  |
| `,` | PARSER_PUNCTUATION | NONE |  |
| `:` | PARSER_PUNCTUATION | NONE |  |
| `;` | PARSER_PUNCTUATION | NONE |  |
| `.` | PARSER_PUNCTUATION | NONE |  |
| `string_start` | PARSER_PUNCTUATION | NONE |  |
| `string_end` | PARSER_PUNCTUATION | NONE |  |
| `keyword_separator` | PARSER_PUNCTUATION | NONE |  |
| `chevron` | PARSER_PUNCTUATION | NONE |  |
| `positional_separator` | PARSER_PUNCTUATION | NONE |  |
| `line_continuation` | PARSER_SYNTAX | NONE |  |

## Parse Errors

Error nodes from failed parsing

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Error nodes from failed parsing |

---

*Generated from `python_types.def`*
