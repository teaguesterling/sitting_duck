# Scope Resolution Test Fixtures

Test files for progressively harder name resolution scenarios.

## Levels

| Level | File | What resolves | Difficulty |
|---|---|---|---|
| 1 | `level1_local.py` | Local variables in same scope | Trivial |
| 2 | `level2_parameter.py` | Function parameters | Easy |
| 3 | `level3_enclosing.py` | Closures, global/nonlocal | Medium |
| 4 | `level4_class_attr.py` | `self.x` through `__init__` | Medium |
| 5 | `level5_method_resolution.py` | `obj.method()` via type | Hard |
| 6 | `level6_cross_file.py` | Imports from other files | Hard |
| 7 | `level7_inheritance.py` | Method resolution order (MRO) | Very Hard |
| 8 | `level8_dynamic.py` | Dynamic dispatch (unresolvable) | Impossible |

## What sitting_duck can resolve today

With `scope_id` + `scope_stack`:
- **Level 1-3**: Direct scope chain resolution. Walk the scope stack, find the nearest definition with the same name.
- **Level 4**: Partial. `self.path` resolves if we track `self.x = y` assignments within the class scope. Requires understanding that `self` refers to the enclosing class instance.

## What requires additional infrastructure

- **Level 5**: Type inference. Need to know that `Processor()` returns a `Processor` instance, then look up methods on that class.
- **Level 6**: Import resolution. Need to parse the imported file, build its symbol table, and connect import names to their definitions.
- **Level 7**: MRO computation. Need to resolve the class hierarchy and walk it in Python's C3 linearization order.
- **Level 8**: Not statically resolvable. These require runtime information.

## Expected resolution approach

```
Level 1-3: scope_stack walk (pure sitting_duck)
Level 4:   scope_stack + self-assignment tracking (sitting_duck + heuristic)
Level 5:   type inference (fledgling / pluckit)
Level 6:   multi-file symbol tables (fledgling / pluckit)
Level 7:   class hierarchy + MRO (fledgling / pluckit)
Level 8:   not resolvable statically
```
