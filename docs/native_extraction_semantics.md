# Native Extraction Field Semantics

This document describes the semantic meaning of each native extraction field across different languages and semantic types.

## Native Extraction Fields

| Field | Type | Description |
|-------|------|-------------|
| `name` | VARCHAR | The identifier name of the node (function name, class name, variable name) |
| `signature_type` | VARCHAR | Type information (return type for functions, class type, variable type) |
| `parameters` | VARCHAR[] | Parameter names for functions, argument values for calls |
| `modifiers` | VARCHAR[] | Access modifiers, keywords, inheritance info |
| `annotations` | VARCHAR | Decorator/annotation text |
| `qualified_name` | VARCHAR | Fully qualified name (currently not populated for most languages) |

---

## DEFINITION_FUNCTION (Functions/Methods)

### Field Semantics

| Field | Meaning |
|-------|---------|
| `name` | Function/method name |
| `signature_type` | Return type (language-specific) |
| `parameters` | List of parameter names |
| `modifiers` | Access modifiers (public, static, async, etc.) |

### Cross-Language Comparison

| Language | signature_type | parameters | modifiers | Notes |
|----------|----------------|------------|-----------|-------|
| **Python** | NULL | ✓ param names | [] | No return type in AST without annotations |
| **Rust** | ✓ return type (`u64`, `()`) | ✓ param names | [] | Full return type extraction |
| **C++** | ✓ return type (`int`, `void`) | ✓ param names | [] | Return type from declaration |
| **Java** | ✓ return type (`void`, `BigInteger`) | ✓ param names | ✓ [`public`, `static`] | Full modifier extraction |
| **Go** | ✓ return type (`*big.Int`, `float64`) | ✓ param names | [] | Supports pointer/complex types |
| **JavaScript** | `function` (literal) | ✓ param names | [] | No type system |
| **Lua** | NULL | [] | [] | Minimal extraction |

### Examples

```
┌──────────┬─────────────────────┬────────────────┬────────────┬──────────────────┬────────────────────────────────────────┐
│ Language │ name                │ signature_type │ parameters │ modifiers        │ peek                                   │
├──────────┼─────────────────────┼────────────────┼────────────┼──────────────────┼────────────────────────────────────────┤
│ python   │ factorial           │ NULL           │ [n]        │ []               │ def factorial(n):                      │
│ rust     │ factorial_recursive │ u64            │ [n]        │ []               │ fn factorial_recursive(n: u64) -> u64  │
│ cpp      │ factorial           │ int            │ [n]        │ []               │ int factorial(int n)                   │
│ java     │ main                │ void           │ [args]     │ [public, static] │ public static void main(String[] args) │
│ go       │ factorial           │ *big.Int       │ [n]        │ []               │ func factorial(n int64) *big.Int       │
│ js       │ factorial           │ function       │ [n]        │ []               │ function factorial(n)                  │
└──────────┴─────────────────────┴────────────────┴────────────┴──────────────────┴────────────────────────────────────────┘
```

---

## DEFINITION_CLASS (Classes/Types)

### Field Semantics

| Field | Meaning |
|-------|---------|
| `name` | Class/type name |
| `signature_type` | Class kind (`class`, `interface`, `abstract_class`, `trait`, `enum`) |
| `parameters` | [] (unused for classes) |
| `modifiers` | Inheritance info (`extends X`, `implements Y`), access modifiers |

### Cross-Language Comparison

| Language | signature_type | modifiers | Notes |
|----------|----------------|-----------|-------|
| **Python** | `class`, `abstract_class` | ✓ inheritance, `has_dunder_methods` | Detects ABC subclasses |
| **Java** | `class`, `interface`, `abstract_class` | ✓ `extends`, `implements`, access | Full OOP support |
| **C++** | NULL | [] | Limited extraction |
| **Rust** | `trait`, `struct`, `enum` | [] | Trait detection works |
| **Go** | `struct`, `interface` | [] | Basic type detection |

### Examples

```
┌──────────┬───────────────────────┬────────────────┬──────────────────────────────────────┬────────────────────────────────────────┐
│ Language │ type                  │ signature_type │ modifiers                            │ peek                                   │
├──────────┼───────────────────────┼────────────────┼──────────────────────────────────────┼────────────────────────────────────────┤
│ python   │ class_definition      │ class          │ [extends_object, has_dunder_methods] │ class BaseQueue(object):               │
│ python   │ class_definition      │ abstract_class │ [abstract, has_dunder_methods]       │ class BaseQueue():                     │
│ java     │ interface_declaration │ interface      │ [interface]                          │ interface Example {                    │
│ java     │ class_declaration     │ class          │ [implements Example]                 │ class ExampleImpl implements Example { │
│ java     │ class_declaration     │ abstract_class │ [abstract]                           │ abstract class Example {               │
│ rust     │ trait_item            │ trait          │ []                                   │ trait Shape { fn area(self) -> i32; }  │
└──────────┴───────────────────────┴────────────────┴──────────────────────────────────────┴────────────────────────────────────────┘
```

---

## COMPUTATION_CALL (Function/Method Calls)

### Field Semantics

| Field | Meaning |
|-------|---------|
| `name` | Function name (for simple calls) OR empty for method calls |
| `signature_type` | Full call expression (e.g., `obj.method`, `pkg.func`) |
| `parameters` | Argument values/expressions |
| `modifiers` | [] |

### Important Note: Method Calls

For method calls like `obj.method()`:
- `name` is **empty** (FIND_IDENTIFIER doesn't traverse member expressions)
- `signature_type` contains the full call: `obj.method`
- Use `signature_type LIKE '%.methodname'` to find method calls

### Cross-Language Comparison

| Language | name (simple call) | signature_type | parameters | Notes |
|----------|-------------------|----------------|------------|-------|
| **Python** | ✓ `print` | ✓ `sys.stdout.write` | ✓ arg values | Method calls have empty name |
| **Java** | ✓ `println` | ✓ `System.out` | ✓ arg values | Method invocations captured |
| **C++** | ✓ `std::print` | ✓ full qualified | ✓ arg values | Namespace-qualified calls |
| **Go** | empty for pkg calls | ✓ `fmt.Println` | ✓ arg values | Package calls use signature_type |
| **Rust** | ✓ macro names | ✓ macro name | [] | Macros captured separately |

### Examples

```
┌──────────┬───────────────────┬────────────┬──────────────────┬────────────┬────────────────────────────────────┐
│ Language │ type              │ name       │ signature_type   │ parameters │ peek                               │
├──────────┼───────────────────┼────────────┼──────────────────┼────────────┼────────────────────────────────────┤
│ python   │ call              │ print      │ print            │ ['']       │ print("Hello world!")              │
│ python   │ call              │            │ sys.stdout.write │ ['']       │ sys.stdout.write("Hello world!\n") │
│ java     │ method_invocation │ println    │ System.out       │ ['']       │ System.out.println("Hello world!") │
│ cpp      │ call_expression   │ std::print │ std::print       │ ['']       │ std::print("Hello world!\n")       │
│ go       │ call_expression   │            │ fmt.Println      │ ['']       │ fmt.Println("Hello world!")        │
│ rust     │ macro_invocation  │ println    │ println          │ []         │ println!("Hello world!")           │
└──────────┴───────────────────┴────────────┴──────────────────┴────────────┴────────────────────────────────────┘
```

### Finding Method Calls

```sql
-- Find all calls to a method named 'empty' (works across all languages)
SELECT file_path, start_line, signature_type, peek
FROM read_ast('src/**/*.cpp', context := 'native')
WHERE semantic_type_to_string(semantic_type) = 'COMPUTATION_CALL'
  AND (
    name = 'empty'                    -- Simple function call
    OR signature_type LIKE '%.empty'  -- Method call via dot
    OR signature_type LIKE '%->empty' -- C++ arrow notation
  );
```

---

## DEFINITION_VARIABLE (Variables/Fields)

### Field Semantics

| Field | Meaning |
|-------|---------|
| `name` | Variable name |
| `signature_type` | Variable type (when available) |
| `parameters` | [] |
| `modifiers` | Declaration keywords (`var`, `let`, `const`, `final`) |

### Cross-Language Comparison

| Language | signature_type | modifiers | Notes |
|----------|----------------|-----------|-------|
| **Go** | ✓ type (`int`, etc.) | ✓ `[var]` | var_spec has best extraction |
| **Java** | ✓ type (`boolean[]`) | [] | local_variable_declaration |
| **Rust** | ✓ type | ✓ `[let]`, `[mut]` | Pattern-based |
| **Python** | NULL | [] | Dynamic typing |
| **JavaScript** | NULL | ✓ `[const]`, `[let]`, `[var]` | Declaration keyword captured |

### Examples

```
┌──────────┬────────────────────────────┬─────────────┬────────────────┬───────────┬─────────────────────────────────────┐
│ Language │ type                       │ name        │ signature_type │ modifiers │ peek                                │
├──────────┼────────────────────────────┼─────────────┼────────────────┼───────────┼─────────────────────────────────────┤
│ go       │ var_spec                   │ door        │ int            │ [var]     │ door int = 1                        │
│ go       │ var_spec                   │ incrementer │ NULL           │ [var]     │ incrementer = 0                     │
│ java     │ local_variable_declaration │             │ boolean[]      │ []        │ boolean[] doors = new boolean[101]; │
│ java     │ variable_declarator        │ doors       │ NULL           │ []        │ doors = new boolean[101]            │
└──────────┴────────────────────────────┴─────────────┴────────────────┴───────────┴─────────────────────────────────────┘
```

---

---

## Known Inconsistencies

### 1. Call Parameters Contain Empty Strings

For `COMPUTATION_CALL`, the `parameters` field contains `['']` (array with empty string) instead of:
- Empty array `[]` when no arguments, OR
- Actual argument expressions when there are arguments

**Current behavior:**
```
│ go   │ call_expression   │ fmt.Println │ [''] │ fmt.Println("Hello world!") │
│ java │ method_invocation │ println     │ [''] │ System.out.println("Hello") │
```

**Expected:** `parameters` should contain `['"Hello world!"']` or `[]`

### 2. Method Call Names Are Empty

For method calls like `obj.method()`:
- `name` is empty across ALL languages
- `signature_type` contains the full expression `obj.method`

**Workaround:** Use `signature_type LIKE '%.methodname'`

### 3. Lua Parameter Extraction Missing

Lua functions show `parameters = []` even when they have parameters:

```
│ lua │ function_declaration │ fact │ [] │ function fact(n)      │
│ lua │ function_declaration │ fact │ [] │ function fact(n, acc) │
```

**Expected:** `parameters` should be `['n']` and `['n', 'acc']`

### 4. qualified_name Never Populated

The `qualified_name` field is always NULL/empty across all languages.

**Expected semantics by language:**

| Language | Expected qualified_name Format | Example |
|----------|-------------------------------|---------|
| Python | `module.ClassName.method` | `mymodule.MyClass.__init__` |
| Java | `package.ClassName.method` | `com.example.MyClass.doSomething` |
| C++ | `namespace::Class::method` | `std::vector::push_back` |
| Rust | `crate::module::function` | `std::io::read` |
| Go | `package.Function` | `fmt.Println` |
| JavaScript | `Class.method` or `module.export` | `Array.prototype.map` |

**Implementation approach:**
- Track parent class/module during AST traversal
- Build qualified name by concatenating parent names with language-appropriate separator
- For calls, use `signature_type` as fallback (already contains `obj.method` pattern)

---

## Summary: Extraction Quality by Language

| Language | Functions | Classes | Calls | Variables | Overall |
|----------|-----------|---------|-------|-----------|---------|
| **Java** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | Excellent |
| **Rust** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐ | Very Good |
| **Go** | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ | Very Good |
| **C++** | ⭐⭐⭐ | ⭐ | ⭐⭐⭐ | ⭐⭐ | Good |
| **Python** | ⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐ | Good |
| **JavaScript** | ⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐ | Good |
| **Dart** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | Very Good |
| **Kotlin** | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | Very Good |
| **Lua** | ⭐ | ⭐ | ⭐ | ⭐ | Needs Work |

---

## Common Query Patterns

### Find a specific function definition
```sql
SELECT name, signature_type, parameters, start_line, peek
FROM read_ast('src/**/*.py', context := 'native', peek := 'full')
WHERE semantic_type_to_string(semantic_type) = 'DEFINITION_FUNCTION'
  AND name = 'my_function';
```

### Find a method within a class (Python)
```sql
WITH class_blocks AS (
    SELECT c.name as class_name, c.node_id as class_id, b.node_id as block_id
    FROM read_ast('myfile.py', context := 'native') c
    JOIN read_ast('myfile.py', context := 'native') b ON b.parent_id = c.node_id
    WHERE c.type = 'class_definition' AND b.type = 'block'
)
SELECT
    cb.class_name || '.' || m.name as qualified_name,
    m.signature_type,
    m.parameters,
    m.start_line
FROM class_blocks cb
JOIN read_ast('myfile.py', context := 'native') m ON m.parent_id = cb.block_id
WHERE m.type = 'function_definition'
  AND m.name = 'my_method';
```

### Find all calls to a method (any object)
```sql
SELECT file_path, start_line, signature_type, peek
FROM read_ast('src/**/*.cpp', context := 'native', peek := 60)
WHERE semantic_type_to_string(semantic_type) = 'COMPUTATION_CALL'
  AND (
    name = 'size'                    -- Simple function call
    OR signature_type LIKE '%.size'  -- obj.size()
    OR signature_type LIKE '%->size' -- ptr->size()
  );
```

### Compare function signatures across languages
```sql
SELECT language, name, signature_type, parameters
FROM read_ast([
    'src/main.py',
    'src/main.rs',
    'src/main.go'
], context := 'native')
WHERE semantic_type_to_string(semantic_type) = 'DEFINITION_FUNCTION'
ORDER BY name, language;
```
