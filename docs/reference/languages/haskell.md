# Haskell Node Types

> Haskell language node type mappings for AST semantic extraction

## Language Characteristics

- **Pure functional**: No side effects, referential transparency
- **Lazy evaluation**: Expressions evaluated only when needed
- **Strong static typing**: Hindley-Milner type inference
- **Type classes**: Ad-hoc polymorphism (like traits/interfaces)
- **Algebraic data types**: Sum and product types
- **Pattern matching**: Exhaustive matching on constructors
- **Monads**: IO, Maybe, Either for effects and control
- **Higher-order functions**: First-class functions, currying
- **List comprehensions**: Declarative list construction
- **Do notation**: Syntactic sugar for monadic operations

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
- [Import Statements](#import-statements)
- [Function Definitions](#function-definitions)
- [Type Definitions](#type-definitions)
- [Function Calls and Expressions](#function-calls-and-expressions)
- [Control Flow](#control-flow)
- [Lambda Expressions](#lambda-expressions)
- [Identifiers and Literals](#identifiers-and-literals)
- [Structured Literals](#structured-literals)
- [Comments](#comments)
- [Parser Error Handling](#parser-error-handling)

## Program Structure

Top-level file and module organization

Haskell file organization: - Each file is a module (or Main for executables) - `module Name where` at top of file - Explicit exports: `module Name (export1, export2) where`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `haskell` | DEFINITION_MODULE | NONE | Haskell root - top-level compilation unit |
| `module` | DEFINITION_MODULE | FIND_IDENTIFIER | Module declaration - `module Name where` |

## Import Statements

Module import declarations

Haskell import features: - `import Module` - import all exports - `import qualified Module` - require qualified names - `import Module (x, y)` - selective import - `import Module hiding (x)` - import all except x - `import qualified Module as M` - alias

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `import` | EXTERNAL_IMPORT | FIND_IDENTIFIER | Import declaration - `import Module` |

## Function Definitions

Haskell function declarations

Haskell function features: - `name :: Type` - type signature (optional but recommended) - `name arg1 arg2 = body` - function definition - Pattern matching: `factorial 0 = 1; factorial n = n * factorial (n-1)` - Guards: `abs n | n < 0 = -n | otherwise = n` - Where clauses: local definitions

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `function` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Function definition - `name args = body` |
| `bind_pattern` | DEFINITION_FUNCTION | FIND_IDENTIFIER | Bind pattern - pattern binding `(a, b) = pair` |

## Type Definitions

Haskell type declarations

Haskell type system: - `data Name = ...` - algebraic data type - `newtype Name = ...` - single-constructor wrapper (zero cost) - `type Name = ...` - type alias - `class Name where` - type class definition - `instance Class Type where` - type class instance

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `data_type` | DEFINITION_CLASS | FIND_IDENTIFIER | Data type declaration - `data List a = Nil | Cons a (List a)` |
| `newtype` | DEFINITION_CLASS | FIND_IDENTIFIER | Newtype declaration - `newtype Age = Age Int` |
| `type_alias` | DEFINITION_CLASS | FIND_IDENTIFIER | Type alias - `type String = [Char]` |
| `class_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | Class declaration - `class Eq a where (==) :: a -> a -> Bool` |
| `instance_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | Instance declaration - `instance Eq Int where ...` |

## Function Calls and Expressions

Function applications

Haskell application syntax: - `f x y` - function application (left-associative) - `x `op` y` - infix operator application - `($)` - low-precedence application operator - `(.)` - function composition

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `apply` | COMPUTATION_CALL | FIND_CALL_TARGET | Apply expression - function application `f x` |
| `expression` | COMPUTATION_CALL | FIND_CALL_TARGET | Expression - general expression (may be application) |

## Control Flow

Conditionals and pattern matching

Haskell control flow: - `if cond then a else b` - conditional (always requires else) - `case expr of pattern -> body` - pattern matching - Guards: `| cond = expr` - guarded expressions - No loops (use recursion, map, fold, etc.)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `case` | FLOW_CONDITIONAL | NONE | Case expression - `case expr of { pattern -> body }` |
| `if` | FLOW_CONDITIONAL | NONE | If expression - `if cond then a else b` |
| `guard` | FLOW_CONDITIONAL | NONE | Guard - `| condition = expression` |

## Lambda Expressions

Anonymous functions

Haskell lambda syntax: - `\x -> x + 1` - single parameter - `\x y -> x + y` - multiple parameters - Pattern matching: `\(a, b) -> a + b`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `lambda` | DEFINITION_FUNCTION | NONE | Lambda expression - `\x -> body` |

## Identifiers and Literals

Names and literal values

Haskell naming conventions: - `variable` - starts with lowercase (values, functions) - `Constructor` - starts with uppercase (data constructors, types) - Qualified: `Module.name`, `Module.Constructor` Haskell literals: - Integers: `42`, `0xFF` - Floats: `3.14`, `1e10` - Characters: `'a'`, `'\n'` - Strings: `"string"` (list of Char)

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `variable` | NAME_IDENTIFIER | NODE_TEXT | Variable - lowercase identifier (value or function) |
| `constructor` | NAME_IDENTIFIER | NODE_TEXT | Constructor - uppercase identifier (data constructor) |
| `qualified_variable` | NAME_QUALIFIED | NODE_TEXT | Qualified variable - `Module.name` |
| `qualified_constructor` | NAME_QUALIFIED | NODE_TEXT | Qualified constructor - `Module.Constructor` |
| `integer` | LITERAL_NUMBER | NODE_TEXT | Integer literal |
| `float` | LITERAL_NUMBER | NODE_TEXT | Floating-point literal |
| `char` | LITERAL_STRING | NODE_TEXT | Character literal - `'a'` |
| `string` | LITERAL_STRING | NODE_TEXT | String literal - `"string"` |

## Structured Literals

Lists and tuples

Haskell structured types: - Lists: `[1, 2, 3]`, `[1..10]`, `[x | x <- xs, x > 0]` - Tuples: `(1, "hello", True)`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `list` | LITERAL_STRUCTURED | NONE | List literal - `[1, 2, 3]` |
| `tuple` | LITERAL_STRUCTURED | NONE | Tuple literal - `(1, "hello")` |

## Comments

Documentation and annotation

Haskell comment styles: - `-- line comment` - `{- block comment -}` - `{-| Haddock documentation -}` - `-- | Haddock line documentation`

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `comment` | METADATA_COMMENT | NODE_TEXT | Comment |

## Parser Error Handling

Parser error nodes

| Node Type | Semantic Type | Name Extraction | Description |
|-----------|---------------|-----------------|-------------|
| `ERROR` | PARSER_SYNTAX | NODE_TEXT | Parse error node |

---

*Generated from `haskell_types.def`*
