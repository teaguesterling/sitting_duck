# Java Node Types

> Java language node type mappings for AST semantic extraction

## Language Characteristics

Java has several unique features that affect AST mapping:

- **Class-based OOP**: Everything exists within classes; no standalone functions
- **Package system**: Hierarchical namespace via packages
- **Access modifiers**: `public`, `private`, `protected`, package-private (default)
- **Static members**: Class-level methods and fields
- **Final keyword**: Immutable variables, prevent method override, sealed classes
- **Constructors**: Special methods for object initialization (CONSTRUCTOR refinement)
- **Initializer blocks**: Static and instance initialization blocks
- **Interfaces**: Abstract types, can have default methods (Java 8+)
- **Enums**: Type-safe enumerations with associated behavior
- **Annotations**: Metadata via `@Annotation` syntax
- **Generics**: Type parameters on classes and methods
- **Lambda expressions**: Functional interfaces (Java 8+)
- **Method references**: `::` operator for method handles
- **Records**: Immutable data classes (Java 16+)
- **Checked exceptions**: Exception handling is part of method signatures
- **Enhanced for**: Iterator-based for-each loops
- **Switch expressions**: Pattern matching in switch (Java 14+)

## Node Categories

- [Packages and Imports](#packages-and-imports)
- [Class and Type Definitions](#class-and-type-definitions)
- [Method and Constructor Definitions](#method-and-constructor-definitions)
- [Field and Variable Declarations](#field-and-variable-declarations)
- [Method Calls and Access](#method-calls-and-access)
- [Type System](#type-system)
- [Operators and Expressions](#operators-and-expressions)
- [Identifiers](#identifiers)
- [Literal Values](#literal-values)
- [Control Flow](#control-flow)
- [Exception Handling](#exception-handling)
- [Annotations](#annotations)
- [Modifiers](#modifiers)
- [Comments](#comments)
- [Structural Elements](#structural-elements)
- [Keywords](#keywords)
- [Operator Tokens](#operator-tokens)
- [Punctuation](#punctuation)

## Packages and Imports

Package declarations and import statements

Java organizes code into packages. The `program` node represents the entire compilation unit (source file). Import statements bring in classes from other packages.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `program` | DEFINITION_MODULE | NONE | Package declarations and import statements |
| `package_declaration` | DEFINITION_MODULE | FIND_IDENTIFIER |  |
| `import_declaration` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT |  |

## Class and Type Definitions

Classes, interfaces, enums, annotations, and records

Java's type system includes: - `class`: Regular classes (REGULAR refinement) - `interface`: Abstract types (ABSTRACT refinement) - `enum`: Type-safe enumerations (ENUM refinement) - `annotation_type`: Custom annotation definitions (ABSTRACT refinement) - `record`: Immutable data carriers (Java 16+) All Java type definitions have bodies and are marked with IS_EMBODIED.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER | Classes, interfaces, enums, annotations, and records |
| `interface_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER |  |
| `enum_declaration` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER |  |
| `annotation_type_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER |  |
| `record_declaration` | DEFINITION_CLASS | Class::REGULAR | FIND_IDENTIFIER |  |

## Method and Constructor Definitions

Methods, constructors, and initializer blocks

Java methods are always inside classes. Constructors are special methods that initialize objects and get the CONSTRUCTOR refinement. Initializer blocks: - `static_initializer`: Runs once when class is loaded - `instance_initializer`: Runs each time an object is created Lambda expressions (Java 8+) are anonymous functions implementing functional interfaces.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `method_declaration` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Methods, constructors, and initializer blocks |
| `constructor_declaration` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | FIND_IDENTIFIER |  |
| `static_initializer` | DEFINITION_FUNCTION | Function::REGULAR | NONE |  |
| `instance_initializer` | DEFINITION_FUNCTION | Function::REGULAR | NONE |  |
| `lambda_expression` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET |  |

## Field and Variable Declarations

Fields, local variables, and parameters

Java variables are mutable by default. The `final` modifier makes them immutable, but this is handled via modifier nodes rather than refinements. Variable types: - `field_declaration`: Class-level fields (FIELD refinement) - `local_variable_declaration`: Method-local variables (MUTABLE refinement) - `formal_parameter`: Method parameters (PARAMETER refinement) - `enum_constant`: Enum values (FIELD refinement)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `field_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Fields, local variables, and parameters |
| `local_variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER |  |
| `variable_declarator` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER |  |
| `formal_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `spread_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `catch_formal_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `enum_constant` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER |  |

## Method Calls and Access

Method invocations, object creation, and field access

Java call expressions: - `method_invocation`: Calling methods on objects (METHOD refinement) - `object_creation_expression`: `new` keyword (CONSTRUCTOR refinement) - `method_reference`: `Class::method` syntax (Java 8+) Field and array access are separate node types.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `method_invocation` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET | Method invocations, object creation, and field access |
| `object_creation_expression` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET |  |
| `method_reference` | COMPUTATION_ACCESS | NODE_TEXT |  |
| `field_access` | COMPUTATION_ACCESS | FIND_IDENTIFIER |  |
| `array_access` | COMPUTATION_ACCESS | NONE |  |

## Type System

Type references, primitives, generics, and arrays

Java has both primitive types and reference types: - Primitives: `int`, `boolean`, `float`, etc. (TYPE_PRIMITIVE) - Reference types: Classes, interfaces, arrays (TYPE_REFERENCE) - Generics: Type parameters `<T>` (TYPE_GENERIC) - Arrays: `Type[]` (TYPE_COMPOSITE) The `void` keyword is also treated as a type for return type declarations.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_identifier` | TYPE_REFERENCE | NODE_TEXT | Type references, primitives, generics, and arrays |
| `scoped_type_identifier` | TYPE_REFERENCE | NODE_TEXT |  |
| `integral_type` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `floating_point_type` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `boolean_type` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `void_type` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `array_type` | TYPE_COMPOSITE | NONE |  |
| `dimensions` | TYPE_COMPOSITE | NONE |  |
| `generic_type` | TYPE_GENERIC | NODE_TEXT |  |
| `type_arguments` | TYPE_GENERIC | NONE |  |

## Operators and Expressions

Arithmetic, logical, comparison, and assignment operators

Java operators include: - Arithmetic: `+`, `-`, `*`, `/`, `%`, bitwise, shifts - Logical: `&&`, `||`, `!` - Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`, `instanceof` - Assignment: `=`, `+=`, `-=`, etc. - Ternary: `? :` - Update: `++`, `--` (prefix and postfix) Java has unsigned right shift `>>>` unlike C/C++.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Arithmetic, logical, comparison, and assignment operators |
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE |  |
| `unary_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE |  |
| `update_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE |  |
| `ternary_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE |  |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE |  |

## Identifiers

Identifier and scoped identifier nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `identifier` | NAME_IDENTIFIER | NODE_TEXT | Identifier and scoped identifier nodes |
| `scoped_identifier` | NAME_QUALIFIED | NODE_TEXT |  |

## Literal Values

Numeric, string, and boolean literals

Java numeric literals can have various bases: - Decimal: `123` - Hexadecimal: `0xFF` - Octal: `0777` - Binary: `0b1010` String literals support escape sequences. Character literals use single quotes.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `decimal_integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Numeric, string, and boolean literals |
| `hex_integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT |  |
| `octal_integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT |  |
| `binary_integer_literal` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT |  |
| `decimal_floating_point_literal` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT |  |
| `hex_floating_point_literal` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT |  |
| `true` | LITERAL_ATOMIC | NODE_TEXT |  |
| `false` | LITERAL_ATOMIC | NODE_TEXT |  |
| `null_literal` | LITERAL_ATOMIC | NODE_TEXT |  |
| `string_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT |  |
| `string_fragment` | LITERAL_STRING | String::LITERAL | NODE_TEXT |  |
| `character_literal` | LITERAL_STRING | String::LITERAL | NODE_TEXT |  |

## Control Flow

Conditionals, loops, and jump statements

Java control flow constructs: - `if`/`else`: Binary conditional - `switch`: Multiway branching (statements and expressions) - `while`/`do-while`: Condition-based loops - `for`: Traditional counter-based loop - `enhanced_for`: Iterator-based for-each loop Loop refinements: - COUNTER: Traditional `for(int i=0; i<n; i++)` - ITERATOR: Enhanced for `for(Type x : collection)` - CONDITIONAL: `while` and `do-while` loops

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | Conditionals, loops, and jump statements |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `switch_expression` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE |  |
| `do_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE |  |
| `for_statement` | FLOW_LOOP | Loop::COUNTER | NONE |  |
| `enhanced_for_statement` | FLOW_LOOP | Loop::ITERATOR | NONE |  |
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE |  |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE |  |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE |  |
| `yield_statement` | FLOW_JUMP | Jump::RETURN | NONE |  |

## Exception Handling

Try/catch/finally and throw statements

Java has checked exceptions that must be declared or caught. The `try_with_resources_statement` (Java 7+) automatically closes resources. Exception types: - ERROR_TRY: `try` and `try-with-resources` blocks - ERROR_CATCH: `catch` clauses - ERROR_FINALLY: `finally` blocks - ERROR_THROW: `throw` statements and `throws` declarations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_statement` | ERROR_TRY | NONE | Try/catch/finally and throw statements |
| `try_with_resources_statement` | ERROR_TRY | NONE |  |
| `catch_clause` | ERROR_CATCH | NONE |  |
| `catch_type` | ERROR_CATCH | NONE |  |
| `finally_clause` | ERROR_FINALLY | NONE |  |
| `throw_statement` | ERROR_THROW | NONE |  |

## Annotations

Java annotation syntax

Annotations provide metadata for classes, methods, and fields. Common annotations: `@Override`, `@Deprecated`, `@SuppressWarnings`. - `marker_annotation`: Annotation without arguments (`@Override`) - `annotation`: Annotation with arguments (`@SuppressWarnings("unchecked")`)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `annotation` | METADATA_ANNOTATION | FIND_IDENTIFIER | Java annotation syntax |
| `marker_annotation` | METADATA_ANNOTATION | FIND_IDENTIFIER |  |
| `annotation_argument_list` | ORGANIZATION_LIST | NONE |  |
| `@` | NAME_ATTRIBUTE | NODE_TEXT |  |

## Modifiers

Access modifiers and other modifier keywords

Java modifiers control visibility and behavior: - Access: `public`, `private`, `protected` (default is package-private) - Other: `static`, `final`, `abstract`, `synchronized`, `volatile`, etc. Modifiers are children of declaration nodes and affect their semantics.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `modifiers` | METADATA_ANNOTATION | NONE | Access modifiers and other modifier keywords |
| `modifier` | METADATA_ANNOTATION | NODE_TEXT |  |

## Comments

Line and block comments

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `line_comment` | METADATA_COMMENT | NODE_TEXT | Line and block comments |
| `block_comment` | METADATA_COMMENT | NODE_TEXT |  |

## Structural Elements

Blocks, lists, and organizational nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `block` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE | Blocks, lists, and organizational nodes |
| `class_body` | ORGANIZATION_BLOCK | NONE |  |
| `interface_body` | ORGANIZATION_BLOCK | NONE |  |
| `enum_body` | ORGANIZATION_BLOCK | NONE |  |
| `constructor_body` | ORGANIZATION_BLOCK | NONE |  |
| `argument_list` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `formal_parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE |  |
| `expression_statement` | EXECUTION_STATEMENT | NONE |  |

## Keywords

Java reserved words as syntax tokens

Keywords are marked with IS_KEYWORD flag and get the same semantic type as the constructs they introduce. This enables semantic queries that include or exclude keyword tokens as needed.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `public` | METADATA_ANNOTATION | NODE_TEXT | Java reserved words as syntax tokens |
| `private` | METADATA_ANNOTATION | NODE_TEXT |  |
| `protected` | METADATA_ANNOTATION | NODE_TEXT |  |
| `static` | METADATA_ANNOTATION | NODE_TEXT |  |
| `final` | METADATA_ANNOTATION | NODE_TEXT |  |
| `abstract` | METADATA_ANNOTATION | NODE_TEXT |  |
| `class` | DEFINITION_CLASS | NODE_TEXT |  |
| `interface` | DEFINITION_CLASS | NODE_TEXT |  |
| `enum` | DEFINITION_CLASS | NODE_TEXT |  |
| `extends` | TYPE_REFERENCE | NODE_TEXT |  |
| `implements` | TYPE_REFERENCE | NODE_TEXT |  |
| `package` | DEFINITION_MODULE | NODE_TEXT |  |
| `import` | EXTERNAL_IMPORT | NODE_TEXT |  |
| `if` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `for` | FLOW_LOOP | NODE_TEXT |  |
| `return` | FLOW_JUMP | NODE_TEXT |  |
| `try` | ERROR_TRY | NODE_TEXT |  |
| `catch` | ERROR_CATCH | NODE_TEXT |  |
| `throw` | ERROR_THROW | NODE_TEXT |  |
| `throws` | ERROR_THROW | NODE_TEXT |  |
| `new` | COMPUTATION_CALL | NODE_TEXT |  |
| `this` | NAME_SCOPED | NODE_TEXT |  |
| `super` | NAME_SCOPED | NODE_TEXT |  |
| `instanceof` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `int` | TYPE_PRIMITIVE | NODE_TEXT |  |

## Operator Tokens

Individual operator symbols

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=` | OPERATOR_ASSIGNMENT | NODE_TEXT | Individual operator symbols |
| `+=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `-=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `*=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `/=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `%=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `&=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `|=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `^=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `<<=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `>>=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `>>>=` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `->` | OPERATOR_ASSIGNMENT | NODE_TEXT |  |
| `==` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `!=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `<=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `>=` | OPERATOR_COMPARISON | NODE_TEXT |  |
| `+` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `-` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `*` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `/` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `%` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `&` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `|` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `^` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `~` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `<<` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `>>` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `>>>` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `++` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `--` | OPERATOR_ARITHMETIC | NODE_TEXT |  |
| `&&` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `||` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `!` | OPERATOR_LOGICAL | NODE_TEXT |  |
| `?` | FLOW_CONDITIONAL | NODE_TEXT |  |
| `::` | COMPUTATION_ACCESS | NODE_TEXT |  |

## Punctuation

Delimiters, separators, and syntax markers

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `(` | PARSER_DELIMITER | NODE_TEXT | Delimiters, separators, and syntax markers |
| `)` | PARSER_DELIMITER | NODE_TEXT |  |
| `[` | PARSER_DELIMITER | NODE_TEXT |  |
| `]` | PARSER_DELIMITER | NODE_TEXT |  |
| `{` | PARSER_DELIMITER | NODE_TEXT |  |
| `}` | PARSER_DELIMITER | NODE_TEXT |  |
| `'` | PARSER_DELIMITER | NODE_TEXT |  |
| `,` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `;` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `.` | PARSER_PUNCTUATION | NODE_TEXT |  |
| `:` | PARSER_PUNCTUATION | NODE_TEXT |  |

---

*Generated from `java_types.def`*
