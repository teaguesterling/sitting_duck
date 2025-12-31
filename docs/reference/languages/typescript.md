# Typescript Node Types

> TypeScript language node type mappings for AST semantic extraction

## Language Characteristics

- **Superset of JavaScript**: Includes all JS constructs via `#include`
- **Static typing**: Compile-time type checking with structural typing
- **Interfaces**: Contract definitions for object shapes (ABSTRACT refinement)
- **Type aliases**: Named types via `type` keyword
- **Enums**: Enumerated types (ENUM refinement)
- **Generics**: Type parameters with constraints
- **Access modifiers**: `public`, `private`, `protected`, `readonly`
- **Decorators**: Metadata annotations via `@decorator` syntax
- **Declaration files**: `.d.ts` ambient declarations
- **Namespaces/modules**: Code organization constructs

## Semantic Type Encoding

Semantic types use 8-bit encoding:
- Bits 7-2: Base category (e.g., `DEFINITION_CLASS = 0x0C`)
- Bits 1-0: Refinement within category

Example: `DEFINITION_CLASS | SemanticRefinements::Class::ABSTRACT`
  - Base: 0x0C (class definition)
  - Refinement: 0x01 (abstract/interface)
  - Combined: 0x0D

## Node Categories

- [Type System Constructs](#type-system-constructs)
- [Type Annotations and References](#type-annotations-and-references)
- [Composite Types](#composite-types)
- [Literal Types](#literal-types)
- [Generic/Template Types](#generic-template-types)
- [Advanced Type Constructs](#advanced-type-constructs)
- [Type Operators and Utilities](#type-operators-and-utilities)
- [Access Modifiers and Visibility](#access-modifiers-and-visibility)
- [Declaration Constructs](#declaration-constructs)
- [Import/Export with Types](#import-export-with-types)
- [Type Assertions and Guards](#type-assertions-and-guards)
- [Class-Specific Constructs](#class-specific-constructs)
- [Function Parameters](#function-parameters)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Control Flow](#control-flow)
- [Exception Handling](#exception-handling)
- [Pattern Matching and Destructuring](#pattern-matching-and-destructuring)
- [TypeScript-Specific Keywords](#typescript-specific-keywords)
- [Primitive Type Keywords](#primitive-type-keywords)
- [Special References and Identifiers](#special-references-and-identifiers)
- [Literals and Values](#literals-and-values)
- [TypeScript Operators](#typescript-operators)
- [Expressions](#expressions)
- [Delimiters and Punctuation](#delimiters-and-punctuation)
- [Parser Errors](#parser-errors)

## Type System Constructs

TypeScript's static type system definitions

TypeScript adds a rich type system on top of JavaScript. These nodes represent type declarations, interfaces, and type signatures that exist only at compile time and are erased during transpilation.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_alias_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | Type alias declaration: `type Name = SomeType` |
| `interface_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | Interface declaration: `interface Name { ... }` |
| `property_signature` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Property signature in interface: `name: Type;` |
| `method_signature` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Method signature in interface: `methodName(params): ReturnType;` |
| `construct_signature` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | NONE | Construct signature: `new (params): Type` |
| `call_signature` | DEFINITION_FUNCTION | Function::REGULAR | NONE | Call signature: `(params): ReturnType` |
| `index_signature` | DEFINITION_VARIABLE | Variable::FIELD | NONE | Index signature: `[key: KeyType]: ValueType` |
| `function_signature` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER | Function signature (declaration only): `function name(params): Type;` |

## Type Annotations and References

Type annotations attached to declarations

These nodes represent type annotations (`: Type`), type identifiers, and predefined primitive types.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_annotation` | TYPE_REFERENCE | NONE | Type annotations attached to declarations |
| `type_identifier` | TYPE_REFERENCE | NODE_TEXT |  |
| `predefined_type` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `this_type` | TYPE_REFERENCE | NODE_TEXT |  |
| `nested_type_identifier` | TYPE_REFERENCE | NODE_TEXT |  |

## Composite Types

Union, intersection, array, tuple, and other composite types

TypeScript's type combinators allow building complex types from simpler ones.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `union_type` | TYPE_COMPOSITE | NONE | Union, intersection, array, tuple, and other composite types |
| `intersection_type` | TYPE_COMPOSITE | NONE |  |
| `array_type` | TYPE_COMPOSITE | NONE |  |
| `tuple_type` | TYPE_COMPOSITE | NONE |  |
| `object_type` | TYPE_COMPOSITE | NONE |  |
| `function_type` | TYPE_COMPOSITE | NONE |  |
| `constructor_type` | TYPE_COMPOSITE | NONE |  |
| `template_literal_type` | TYPE_COMPOSITE | NONE |  |
| `template_type` | TYPE_COMPOSITE | NONE |  |

## Literal Types

Types representing specific literal values

TypeScript allows literal values as types for precise type narrowing.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `literal_type` | TYPE_PRIMITIVE | NODE_TEXT | Types representing specific literal values |
| `string_literal_type` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `number_literal_type` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `boolean_literal_type` | TYPE_PRIMITIVE | NODE_TEXT |  |

## Generic/Template Types

Parameterized types and type parameters

TypeScript supports generics with constraints and defaults.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_parameters` | TYPE_GENERIC | NONE | Parameterized types and type parameters |
| `type_parameter` | TYPE_GENERIC | FIND_IDENTIFIER |  |
| `type_arguments` | TYPE_GENERIC | NONE |  |
| `generic_type` | TYPE_GENERIC | NONE |  |
| `constraint` | TYPE_REFERENCE | NONE |  |
| `default_type` | TYPE_REFERENCE | NONE |  |

## Advanced Type Constructs

Conditional types, mapped types, indexed access, and inference

TypeScript's advanced type-level programming features.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `conditional_type` | TYPE_REFERENCE | NONE | Conditional types, mapped types, indexed access, and inference |
| `infer_type` | TYPE_REFERENCE | NONE |  |
| `mapped_type_clause` | TYPE_REFERENCE | NONE |  |
| `indexed_access_type` | TYPE_REFERENCE | NONE |  |
| `parenthesized_type` | TYPE_REFERENCE | NONE |  |
| `lookup_type` | TYPE_REFERENCE | NONE |  |
| `index_type_query` | TYPE_REFERENCE | NONE |  |
| `type_query` | TYPE_REFERENCE | NONE |  |
| `extends_type_clause` | TYPE_REFERENCE | NONE |  |
| `flow_maybe_type` | TYPE_REFERENCE | NONE |  |

## Type Operators and Utilities

Built-in type operators for type manipulation

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `typeof_type` | TYPE_REFERENCE | NONE | Built-in type operators for type manipulation |
| `keyof_type` | TYPE_REFERENCE | NONE |  |
| `readonly_type` | TYPE_REFERENCE | NONE |  |
| `asserts` | TYPE_REFERENCE | NONE |  |
| `asserts_annotation` | TYPE_REFERENCE | NONE |  |
| `opting_type_annotation` | TYPE_REFERENCE | NONE |  |
| `type_predicate_annotation` | TYPE_REFERENCE | NONE |  |

## Access Modifiers and Visibility

TypeScript access control keywords

Unlike JavaScript, TypeScript has compile-time access modifiers. These are mapped to METADATA_ANNOTATION as they affect visibility.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `accessibility_modifier` | METADATA_ANNOTATION | NODE_TEXT | TypeScript access control keywords |
| `public` | METADATA_ANNOTATION | NODE_TEXT |  |
| `private` | METADATA_ANNOTATION | NODE_TEXT |  |
| `protected` | METADATA_ANNOTATION | NODE_TEXT |  |
| `readonly` | METADATA_ANNOTATION | NODE_TEXT |  |
| `static` | METADATA_ANNOTATION | NODE_TEXT |  |
| `abstract` | METADATA_ANNOTATION | NODE_TEXT |  |
| `override_modifier` | METADATA_ANNOTATION | NODE_TEXT |  |
| `override` | METADATA_ANNOTATION | NONE |  |
| `get` | METADATA_ANNOTATION | NONE |  |
| `set` | METADATA_ANNOTATION | NONE |  |
| `global` | METADATA_ANNOTATION | NONE |  |

## Declaration Constructs

TypeScript-specific declaration types

Includes enums, namespaces, modules, and ambient declarations.

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ambient_declaration` | DEFINITION_MODULE | NONE | TypeScript-specific declaration types |
| `declaration_list` | ORGANIZATION_LIST | NONE |  |
| `enum_declaration` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER |  |
| `enum_assignment` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER |  |
| `module_declaration` | DEFINITION_MODULE | FIND_IDENTIFIER |  |
| `namespace_declaration` | DEFINITION_MODULE | FIND_IDENTIFIER |  |
| `internal_module` | DEFINITION_MODULE | FIND_IDENTIFIER |  |

## Import/Export with Types

TypeScript import/export extensions for types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `import_type_clause` | EXTERNAL_IMPORT | NONE | TypeScript import/export extensions for types |
| `export_type_clause` | EXTERNAL_EXPORT | NONE |  |
| `import_specifier` | EXTERNAL_IMPORT | FIND_IDENTIFIER |  |
| `export_specifier` | EXTERNAL_EXPORT | FIND_IDENTIFIER |  |
| `import_require_clause` | EXTERNAL_IMPORT | NONE |  |
| `external_module_reference` | EXTERNAL_IMPORT | NONE |  |
| `named_imports` | EXTERNAL_IMPORT | Import::SELECTIVE | NONE |  |
| `from` | EXTERNAL_IMPORT | NONE |  |

## Type Assertions and Guards

Runtime and compile-time type narrowing

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type_assertion` | TYPE_REFERENCE | NONE | Runtime and compile-time type narrowing |
| `as_expression` | TYPE_REFERENCE | NONE |  |
| `non_null_expression` | TYPE_REFERENCE | NONE |  |
| `satisfies_expression` | TYPE_REFERENCE | NONE |  |
| `type_predicate` | TYPE_REFERENCE | NONE |  |

## Class-Specific Constructs

TypeScript class extensions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `abstract_class_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | TypeScript class extensions |
| `abstract_method_signature` | DEFINITION_FUNCTION | Function::REGULAR | FIND_IDENTIFIER |  |
| `parameter_property` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `public_field_definition` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER |  |
| `class_heritage` | TYPE_REFERENCE | NONE |  |
| `implements_clause` | TYPE_REFERENCE | NONE |  |
| `extends_clause` | TYPE_REFERENCE | NONE |  |
| `class_body` | ORGANIZATION_BLOCK | NONE |  |
| `interface_body` | ORGANIZATION_BLOCK | NONE |  |
| `enum_body` | ORGANIZATION_BLOCK | NONE |  |

## Function Parameters

TypeScript function parameter types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `required_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | TypeScript function parameter types |
| `optional_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER |  |
| `rest_parameter` | PATTERN_COLLECT | FIND_IDENTIFIER |  |
| `decorator` | NAME_ATTRIBUTE | FIND_IDENTIFIER |  |

## Function Calls and Expressions

Call expressions with TypeScript refinements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Call expressions with TypeScript refinements |
| `new_expression` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET |  |
| `instantiation_expression` | COMPUTATION_CALL | NONE |  |

## Control Flow

Control flow statements and expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `labeled_statement` | EXECUTION_STATEMENT | FIND_IDENTIFIER | Control flow statements and expressions |
| `else_clause` | FLOW_CONDITIONAL | NONE |  |
| `switch` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `switch_body` | ORGANIZATION_BLOCK | Organization::SEQUENTIAL | NONE |  |
| `switch_default` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `default_clause` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `switch_case` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `case_clause` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE |  |
| `default` | FLOW_CONDITIONAL | NONE |  |
| `ternary_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE |  |
| `break` | FLOW_JUMP | Jump::BREAK | NONE |  |
| `continue` | FLOW_JUMP | Jump::CONTINUE | NONE |  |
| `do` | FLOW_LOOP | NONE |  |
| `yield` | FLOW_SYNC | NONE |  |

## Exception Handling

Try/catch/finally and throw statements

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `throw` | ERROR_THROW | NONE | Try/catch/finally and throw statements |
| `try` | ERROR_TRY | NONE |  |
| `catch` | ERROR_CATCH | NONE |  |
| `finally` | ERROR_FINALLY | NONE |  |

## Pattern Matching and Destructuring

Destructuring patterns for arrays and objects

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable_declarator` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Destructuring patterns for arrays and objects |
| `assignment_pattern` | PATTERN_DESTRUCTURE | FIND_IDENTIFIER |  |
| `object_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `array_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `rest_pattern` | PATTERN_COLLECT | NONE |  |
| `spread_element` | PATTERN_COLLECT | NONE |  |
| `shorthand_property_identifier_pattern` | PATTERN_DESTRUCTURE | NODE_TEXT |  |
| `pair_pattern` | PATTERN_DESTRUCTURE | NONE |  |
| `object_assignment_pattern` | PATTERN_DESTRUCTURE | NONE |  |

## TypeScript-Specific Keywords

Reserved words specific to TypeScript

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `type` | TYPE_REFERENCE | NONE | Reserved words specific to TypeScript |
| `interface` | DEFINITION_CLASS | NONE |  |
| `enum` | DEFINITION_CLASS | NONE |  |
| `namespace` | DEFINITION_MODULE | NONE |  |
| `module` | DEFINITION_MODULE | NONE |  |
| `declare` | METADATA_ANNOTATION | NONE |  |
| `keyof` | TYPE_REFERENCE | NONE |  |
| `typeof` | TYPE_REFERENCE | NONE |  |
| `infer` | TYPE_REFERENCE | NONE |  |
| `extends` | TYPE_REFERENCE | NONE |  |
| `implements` | TYPE_REFERENCE | NONE |  |
| `as` | TYPE_REFERENCE | NONE |  |
| `is` | TYPE_REFERENCE | NONE |  |
| `satisfies` | TYPE_REFERENCE | NONE |  |
| `of` | OPERATOR_COMPARISON | NONE |  |
| `delete` | EXECUTION_STATEMENT | NONE |  |
| `new` | COMPUTATION_CALL | NONE |  |

## Primitive Type Keywords

Built-in primitive type names

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `boolean` | TYPE_PRIMITIVE | NODE_TEXT | Built-in primitive type names |
| `string` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `number` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `bigint` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `symbol` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `object` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `any` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `unknown` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `never` | TYPE_PRIMITIVE | NODE_TEXT |  |
| `void` | TYPE_PRIMITIVE | NODE_TEXT |  |

## Special References and Identifiers

Special identifier nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `this` | NAME_SCOPED | NODE_TEXT | Special identifier nodes |
| `super` | NAME_SCOPED | NODE_TEXT |  |
| `shorthand_property_identifier` | NAME_IDENTIFIER | NODE_TEXT |  |
| `nested_identifier` | NAME_QUALIFIED | NODE_TEXT |  |
| `target` | NAME_IDENTIFIER | NODE_TEXT |  |
| `meta` | METADATA_ANNOTATION | NODE_TEXT |  |

## Literals and Values

String, template, and regex literals

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `template_literal_type_span` | LITERAL_STRING | NONE | String, template, and regex literals |
| `template_substitution` | LITERAL_STRING | NONE |  |
| `regex` | LITERAL_STRING | NODE_TEXT |  |
| `regex_pattern` | LITERAL_STRING | NODE_TEXT |  |
| `regex_flags` | LITERAL_STRING | NODE_TEXT |  |
| `escape_sequence` | LITERAL_STRING | NONE |  |
| `property_assignment` | LITERAL_STRUCTURED | FIND_IDENTIFIER |  |
| `computed_property_name` | COMPUTATION_EXPRESSION | NONE |  |

## TypeScript Operators

Operators including TypeScript-specific ones

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `=>` | OPERATOR_ASSIGNMENT | NONE | Operators including TypeScript-specific ones |
| `?.` | COMPUTATION_ACCESS | NONE |  |
| `optional_chain` | COMPUTATION_ACCESS | NONE |  |
| `?` | OPERATOR_LOGICAL | NONE |  |
| `??` | OPERATOR_LOGICAL | NONE |  |
| `!` | OPERATOR_LOGICAL | NONE |  |
| `?:` | OPERATOR_LOGICAL | NONE |  |
| `augmented_assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::COMPOUND | NONE |  |
| `...` | PATTERN_COLLECT | NODE_TEXT |  |

## Expressions

Expression nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `sequence_expression` | COMPUTATION_EXPRESSION | NONE | Expression nodes |
| `parenthesized_expression` | COMPUTATION_EXPRESSION | NONE |  |
| `empty_statement` | EXECUTION_STATEMENT | NONE |  |

## Delimiters and Punctuation

Syntactic punctuation tokens

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| ``` | PARSER_DELIMITER | NONE | Syntactic punctuation tokens |
| `<` | PARSER_DELIMITER | NONE |  |
| `>` | PARSER_DELIMITER | NONE |  |

## Parser Errors

Error nodes from parsing

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Error nodes from parsing |

---

*Generated from `typescript_types.def`*
