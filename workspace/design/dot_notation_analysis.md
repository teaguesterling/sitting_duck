# Dot Notation Analysis for DuckDB AST Extension

## Summary

Dot notation (`object.method()`) does not work with DuckDB SQL macros because macros are text substitution functions, not object methods. This is a fundamental limitation of how DuckDB implements macros.

## Why Dot Notation Fails

1. **Macros are not methods**: SQL macros in DuckDB are global functions, not methods bound to objects
2. **Text substitution**: Macros work by replacing the macro call with the macro body at parse time
3. **No object context**: There's no way to bind a macro to a specific column or value

## What Works vs What Doesn't

### ❌ Doesn't Work (Dot Notation)
```sql
SELECT nodes.ast_find_type('function_definition')
FROM read_ast_objects('file.py', 'python');
-- Error: Binder Error: Referenced column "nodes" not found
```

### ✅ Works (Function Call)
```sql
SELECT ast_find_type(nodes, 'function_definition')
FROM read_ast_objects('file.py', 'python');
```

### ✅ Works (Dot Notation for Structs)
```sql
-- Dot notation DOES work for struct fields
SELECT node.type, node.start.line
FROM (SELECT {'type': 'function', 'start': {'line': 10}} as node);
```

### ✅ Works (Arrow Notation for JSON)
```sql
-- Arrow notation works for JSON navigation
SELECT nodes->'$[0]'->>'type'
FROM read_ast_objects('file.py', 'python');
```

## Design Implications

1. **API Consistency**: All our macros must use function call syntax: `macro_name(param1, param2)`
2. **Documentation**: Clear examples showing the correct syntax
3. **Error Messages**: Users attempting dot notation will get confusing errors
4. **Alternative**: Consider creating scalar functions instead of macros if method-like syntax is critical

## Potential Workarounds

### Option 1: Scalar Functions (C++)
We could implement actual scalar functions in C++ that could potentially support different syntax, but this would:
- Require more complex implementation
- Lose the simplicity of SQL macros
- Still not support true dot notation

### Option 2: JSON Methods
We could leverage DuckDB's JSON functions which do support arrow notation:
```sql
-- This works because -> is a JSON operator, not a method call
SELECT nodes->'$.functions'
```

### Option 3: Clear Documentation
The best approach is to:
1. Use consistent function-call syntax for all macros
2. Document this clearly in examples
3. Provide helpful error messages where possible

## Recommendation

Keep the current macro-based approach with function call syntax. The benefits of SQL macros (simplicity, transparency, easy modification) outweigh the syntax limitation. Users familiar with SQL will expect function call syntax anyway.

## Test Coverage

We should add tests that:
1. ✅ Verify macros work with function syntax
2. ✅ Verify dot notation fails with clear error
3. ✅ Show alternatives (struct fields, JSON operators)
4. ✅ Demonstrate the differences clearly