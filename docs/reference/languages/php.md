# Php Node Types

> PHP language node type mappings for AST semantic extraction

## Language Characteristics

- **Server-side scripting**: Originally designed for web development
- **Dynamic typing**: Variables have no type declarations (type hints optional)
- **OOP support**: Classes, interfaces, traits, abstract classes
- **Namespaces**: `namespace` and `use` for code organization
- **Traits**: Horizontal code reuse (composition over inheritance)
- **Type system**: Union types, intersection types, nullable (PHP 8+)
- **Attributes**: `#[Attribute]` for metadata (PHP 8+)
- **Match expression**: Pattern matching expression (PHP 8+)
- **Arrow functions**: Short closures `fn($x) => $x * 2`

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

- [Program Structure and Imports](#program-structure-and-imports)
- [Class and Interface Definitions](#class-and-interface-definitions)
- [Function and Method Definitions](#function-and-method-definitions)
- [Variable and Property Declarations](#variable-and-property-declarations)
- [Type System](#type-system)
- [Expressions and Calls](#expressions-and-calls)
- [Identifiers and Literals](#identifiers-and-literals)
- [Control Flow](#control-flow)
- [Exception Handling](#exception-handling)
- [Comments and Metadata](#comments-and-metadata)
- [Special PHP Constructs](#special-php-constructs)
- [Modifiers](#modifiers)
- [Error Handling](#error-handling)

## Program Structure and Imports

File structure, namespaces, and includes

PHP file structure: - `<?php` opens PHP mode (required for pure PHP files) - `namespace` declares package for the file - `use` imports classes/functions/constants - `require`/`include` includes other PHP files

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `program` | DEFINITION_MODULE | NONE | Program root - represents the entire PHP file |
| `php_tag` | PARSER_DELIMITER | NODE_TEXT | PHP opening/closing tags - `<?php`, `?>`, `<?=` |
| `require_expression` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT | Require - includes file, fatal error if not found |
| `require_once_expression` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT | Require once - includes file only once |
| `include_expression` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT | Include - includes file, warning if not found |
| `include_once_expression` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT | Include once - includes file only once |
| `namespace_definition` | DEFINITION_MODULE | CUSTOM | Namespace definition - `namespace App\Models;` |
| `namespace_use_declaration` | EXTERNAL_IMPORT | Import::MODULE | NODE_TEXT | Use declaration - imports classes, functions, or constants |

## Class and Interface Definitions

OOP constructs in PHP

PHP OOP features: - Classes with single inheritance - Interfaces for contracts (can extend multiple interfaces) - Traits for horizontal code reuse - Abstract classes (cannot be instantiated) - Final classes (cannot be extended) - Enums (PHP 8.1+) with optional backing type

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `class_declaration` | DEFINITION_CLASS | Class::REGULAR | CUSTOM | Class declaration - `class Name extends Parent implements Interface` |
| `interface_declaration` | DEFINITION_CLASS | Class::ABSTRACT | CUSTOM | Interface declaration - `interface Name extends Other` |
| `trait_declaration` | DEFINITION_CLASS | Class::ABSTRACT | FIND_IDENTIFIER | Trait declaration - reusable method collections |
| `enum_declaration` | DEFINITION_CLASS | Class::ENUM | FIND_IDENTIFIER | Enum declaration - `enum Status: string { case Active = 'active'; }` |

## Function and Method Definitions

Function declarations in PHP

PHP function features: - `function name(Type $param): ReturnType` - Optional parameters with default values - Variadic parameters: `...$args` - Named arguments: `func(name: $value)` (PHP 8+) - Arrow functions: `fn($x) => $x * 2` - Anonymous functions (closures) with `use` for captured variables

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_definition` | DEFINITION_FUNCTION | Function::REGULAR | CUSTOM | Function definition - `function name($params) { }` |
| `function` | DEFINITION_FUNCTION | NONE | Function keyword token |
| `method_declaration` | DEFINITION_FUNCTION | Function::REGULAR | CUSTOM | Method declaration - function inside class/trait |
| `constructor_declaration` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | NODE_TEXT | Constructor - `__construct()` |
| `destructor_declaration` | DEFINITION_FUNCTION | Function::CONSTRUCTOR | NODE_TEXT | Destructor - `__destruct()` |
| `anonymous_function_creation_expression` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Anonymous function - `function($x) use ($y) { }` |
| `arrow_function` | DEFINITION_FUNCTION | Function::LAMBDA | FIND_ASSIGNMENT_TARGET | Arrow function - `fn($x) => $x * 2` (PHP 7.4+) |

## Variable and Property Declarations

Variables, properties, and constants

PHP variable characteristics: - All variables prefixed with `$` - No explicit declaration needed for local variables - Properties require visibility modifier - Constructor property promotion (PHP 8+): `public function __construct(public $name)` - Readonly properties (PHP 8.1+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `property_declaration` | DEFINITION_VARIABLE | Variable::FIELD | FIND_IDENTIFIER | Property declaration - class member variable |
| `const_declaration` | DEFINITION_VARIABLE | Variable::IMMUTABLE | FIND_IDENTIFIER | Constant declaration - `const NAME = value;` |
| `global_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Global variable declaration - `global $var;` |
| `static_variable_declaration` | DEFINITION_VARIABLE | Variable::MUTABLE | FIND_IDENTIFIER | Static variable - `static $var = value;` |
| `formal_parameters` | ORGANIZATION_LIST | Organization::COLLECTION | NONE | Formal parameters list |
| `simple_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Simple parameter - `Type $name` |
| `variadic_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Variadic parameter - `...$args` |
| `property_promotion_parameter` | DEFINITION_VARIABLE | Variable::PARAMETER | FIND_IDENTIFIER | Property promotion parameter - `public $name` in constructor |

## Type System

PHP type hints and declarations

PHP type system (evolved significantly in PHP 7-8): - Scalar types: `int`, `float`, `string`, `bool` - Compound: `array`, `object`, `callable`, `iterable` - Special: `void`, `never`, `mixed`, `null` - Nullable: `?Type` or `Type|null` - Union types: `Type1|Type2` (PHP 8+) - Intersection types: `Type1&Type2` (PHP 8.1+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `named_type` | TYPE_REFERENCE | NODE_TEXT | Named type reference - class, interface, or built-in type |
| `optional_type` | TYPE_COMPOSITE | NONE | Optional/nullable type - `?Type` |
| `union_type` | TYPE_COMPOSITE | NONE | Union type - `Type1|Type2` |
| `intersection_type` | TYPE_COMPOSITE | NONE | Intersection type - `Type1&Type2` |
| `primitive_type` | TYPE_PRIMITIVE | NODE_TEXT | Primitive type - `int`, `string`, `bool`, etc. |
| `array_type` | TYPE_COMPOSITE | NODE_TEXT | Array type hint - `array` |

## Expressions and Calls

Function/method invocations and expressions

PHP call types: - Function call: `function_name($args)` - Method call: `$obj->method($args)` - Static call: `ClassName::method($args)` - Object creation: `new ClassName($args)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function_call_expression` | COMPUTATION_CALL | Call::FUNCTION | FIND_CALL_TARGET | Function call - `function_name($args)` |
| `member_call_expression` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET | Method call - `$obj->method($args)` |
| `scoped_call_expression` | COMPUTATION_CALL | Call::METHOD | FIND_CALL_TARGET | Static call - `Class::method($args)` |
| `object_creation_expression` | COMPUTATION_CALL | Call::CONSTRUCTOR | FIND_CALL_TARGET | Object creation - `new ClassName($args)` |
| `member_access_expression` | COMPUTATION_ACCESS | FIND_IDENTIFIER | Member access - `$obj->property` |
| `subscript_expression` | COMPUTATION_ACCESS | NONE | Array subscript - `$arr[index]` |
| `assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::SIMPLE | NONE | Assignment - `$var = value` |
| `augmented_assignment_expression` | OPERATOR_ASSIGNMENT | Assignment::COMPOUND | NONE | Augmented assignment - `$var += value` |
| `binary_expression` | OPERATOR_ARITHMETIC | Arithmetic::BINARY | NONE | Binary expression - arithmetic, comparison, logical |
| `unary_op_expression` | OPERATOR_ARITHMETIC | Arithmetic::UNARY | NONE | Unary expression - `!`, `-`, `++`, `--` |
| `conditional_expression` | FLOW_CONDITIONAL | Conditional::TERNARY | NONE | Conditional/ternary expression - `$a ? $b : $c` |
| `match_expression` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Match expression - pattern matching (PHP 8+) |

## Identifiers and Literals

Names, variables, and literal values

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `name` | NAME_IDENTIFIER | NODE_TEXT | Name - class, function, or constant name |
| `variable_name` | NAME_IDENTIFIER | NODE_TEXT | Variable name - `$varname` |
| `dynamic_variable_name` | NAME_IDENTIFIER | NODE_TEXT | Dynamic variable - `$$varname` or `${expr}` |

## Control Flow

Conditionals, loops, and jumps

PHP control structures support alternative syntax for templates: - `if (...): ... endif;` - `foreach (...): ... endforeach;`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `if_statement` | FLOW_CONDITIONAL | Conditional::BINARY | NONE | If statement - `if (...) { } elseif { } else { }` |
| `switch_statement` | FLOW_CONDITIONAL | Conditional::MULTIWAY | NONE | Switch statement - `switch ($x) { case 1: ... }` |
| `while_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | While loop - `while (...) { }` |
| `do_statement` | FLOW_LOOP | Loop::CONDITIONAL | NONE | Do-while loop - `do { } while (...);` |
| `for_statement` | FLOW_LOOP | Loop::COUNTER | NONE | For loop - `for ($i = 0; $i < $n; $i++) { }` |
| `foreach_statement` | FLOW_LOOP | Loop::ITERATOR | NONE | Foreach loop - `foreach ($arr as $key => $value) { }` |
| `break_statement` | FLOW_JUMP | Jump::BREAK | NONE | Break statement - exits loop or switch |
| `continue_statement` | FLOW_JUMP | Jump::CONTINUE | NONE | Continue statement - skips to next iteration |
| `return_statement` | FLOW_JUMP | Jump::RETURN | NONE | Return statement - exits function with value |
| `goto_statement` | FLOW_JUMP | Jump::GOTO | FIND_IDENTIFIER | Goto statement - jumps to label |
| `yield_expression` | FLOW_SYNC | NONE | Yield expression - generator yield |

## Exception Handling

Try/catch/finally constructs

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `try_statement` | ERROR_TRY | NONE | Try statement - begins exception handling block |
| `catch_clause` | ERROR_CATCH | FIND_IDENTIFIER | Catch clause - handles specific exception types |
| `finally_clause` | ERROR_FINALLY | NONE | Finally clause - always executed |
| `throw_expression` | ERROR_THROW | NONE | Throw expression - raises exception |

## Comments and Metadata

Documentation and annotations

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment - `//`, `/* */`, or `/** */` (PHPDoc) |
| `attribute_list` | METADATA_ANNOTATION | NONE | Attribute list - `#[Attribute, Another]` (PHP 8+) |
| `attribute` | METADATA_ANNOTATION | FIND_IDENTIFIER | Single attribute - `#[Attribute(...)]` |

## Special PHP Constructs

PHP-specific statements and expressions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `echo_statement` | EXECUTION_STATEMENT | NONE | Echo statement - `echo "text";` |
| `print_intrinsic` | EXECUTION_STATEMENT | NONE | Print intrinsic - `print "text"` |
| `unset_statement` | EXECUTION_STATEMENT | NONE | Unset statement - `unset($var)` |
| `declare_statement` | METADATA_DIRECTIVE | NONE | Declare statement - `declare(strict_types=1)` |

## Modifiers

Visibility and behavior modifiers

PHP modifiers: - Visibility: `public`, `protected`, `private` - Inheritance: `abstract`, `final` - Static: `static` for class-level members - Readonly: `readonly` (PHP 8.1+)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `visibility_modifier` | METADATA_ANNOTATION | NODE_TEXT | Visibility modifier - `public`, `protected`, `private` |
| `static_modifier` | METADATA_ANNOTATION | NODE_TEXT | Static modifier - class-level method/property |
| `final_modifier` | METADATA_ANNOTATION | NODE_TEXT | Final modifier - prevents override/extension |
| `abstract_modifier` | METADATA_ANNOTATION | NODE_TEXT | Abstract modifier - requires implementation |

## Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

## Other Node Types

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `integer` | LITERAL_NUMBER | Number::INTEGER | NODE_TEXT | Integer literal |
| `float` | LITERAL_NUMBER | Number::FLOAT | NODE_TEXT | Floating-point literal |
| `string` | LITERAL_STRING | String::LITERAL | NODE_TEXT | String literal - single or double quoted |
| `encapsed_string` | LITERAL_STRING | String::TEMPLATE | NODE_TEXT | Encapsed string - double quoted with interpolation |
| `heredoc` | LITERAL_STRING | String::RAW | NODE_TEXT | Heredoc string - `<<<EOF` |
| `nowdoc` | LITERAL_STRING | String::RAW | NODE_TEXT | Nowdoc string - `<<<'EOF'` (no interpolation) |
| `shell_command_expression` | LITERAL_STRING | String::TEMPLATE | NODE_TEXT | Shell command - backtick execution |
| `boolean` | LITERAL_ATOMIC | NODE_TEXT | Boolean literal - `true` or `false` |
| `null` | LITERAL_ATOMIC | NODE_TEXT | Null literal |
| `array_creation_expression` | LITERAL_STRUCTURED | Structured::SEQUENCE | NONE | Array creation - `[1, 2, 3]` or `array(...)` |

---

*Generated from `php_types.def`*
