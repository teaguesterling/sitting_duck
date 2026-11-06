# 056 — the #148 IS_EXPORTED guard overreaches and depends on config

**Status:** implemented on `fix/148-exported-includes-methods-and-nested-functions`,
awaiting a rebuild and a full-suite run. Blocks the v1.18.2 tag (PR #311).
The strict-flag option was chosen by Teague on 2026-11-05.
The Dart part is NOT fixed and is split out as its own bug.
**Found by:** code review of PR #311, 2026-11-05. All figures re-measured by
hand against `build/release/duckdb`, not taken from the review.

## What is wrong

`aab2457` added a guard in `src/include/unified_ast_backend_impl.hpp:334` so
that a definition inside a function body does not get `IS_EXPORTED`. The intent
was nested functions. The guard applies to **every** name-binding node.

`ShouldPruneNode:84` subtree-prunes `BINDS_NAME && !IS_EXPORTED`, so
`prune := ['internal']` now deletes the bodies of functions it keeps.

Measured on `test/data/python/sample_app.py`:

| | control | `prune := ['internal']` |
|---|---|---|
| all nodes | 1246 | 841 |
| `assignment` | 26 | 2 |
| `call` | 45 | 30 |

Node kinds that lose the flag inside a function body:

    identifier           37   parameters
    assignment           24   locals
    default_parameter     5
    as_pattern_target     4
    function_definition   3   <-- the only intended target

3 of 73. Every one of the five kinds carries name bits `6`, so they are all
name-*definitions*: narrowing declaration-versus-definition cannot fix this.
The separating axis is the semantic base — 240 `DEFINITION_FUNCTION` is
importable by name, 244 `DEFINITION_VARIABLE` and 80 `NAME_IDENTIFIER` are not.

`148_exported_scope.test` section 7 counts only `function_definition`, so it
passes while this is live. A green test pinning the intended subset.

## Three more defects, one root cause

The guard reads `scope_node_stack`, which is:

- pushed only under `if (config.structure >= StructureLevel::FULL)` (guard
  :405, push :433), so at `structure := 'minimal'` the guard is inert and
  `IS_EXPORTED` silently depends on `structure :=`, which it never did before;
- pushed only for `IS_SCOPE` nodes;
- holding only nodes that survived pruning, so `IS_EXPORTED` depends on
  `prune :=` too. Under `prune := ['unnamed']` an anonymous lambda is
  node-pruned, never pushed, and a name inside it flips to exported.

A node-pruned or subtree-pruned node does `stack.pop_back(); continue;` at
:366/:378 and never gets a second visit, so the pop at :533 never runs for it.
That rules out any fix that pushes a second stack before the prune check.

## Dart is NOT fixable this way, and is not a regression

Dart's `function_signature` carries `DEFINITION_FUNCTION` and the name, and is
a **sibling** of `function_body`, both children of `program`:

    function_signature|topLevel|DEFINITION_FUNCTION  is_scope=false
    function_body||ORGANIZATION_BLOCK
      block
        local_function_declaration|DEFINITION_FUNCTION  is_exported=true
        local_variable_declaration|localSecret          is_exported=true

So no ancestor walk from a `DEFINITION_FUNCTION` node reaches a Dart function
body. Dropping the `IS_SCOPE` requirement does not rescue Dart, and it breaks
C++: `function_declarator` is itself a `DEFINITION_FUNCTION` child of
`function_definition`. `IS_SCOPE` is load-bearing and stays.

Dart's `is_exported=true` here equals `main`'s behaviour, so it is an
incompleteness, not a regression. What IS a defect is the sentence `aab2457`
added to `docs/reference/css-pseudo-classes.md`: "A definition inside a
function body never matches. No supported language can import it." Dart
refutes it. This is the second false documentation claim from this commit's
reasoning; the first was the five-language `:exported` table, corrected in
`a4cbc3f`.

## The plan

Teague chose the strict-flag option on 2026-11-05: keep `IS_EXPORTED`
semantically correct everywhere, and change what `prune := ['internal']`
prunes.

1. **Keep the flag strict.** A definition inside a function body does not get
   `IS_EXPORTED`, whatever its kind. A function-local name is genuinely not
   importable, so this is the correct answer, and it is what `aab2457`
   intended. No narrowing by semantic base.
2. **Make the ancestor test config-independent.** Stop reading
   `scope_node_stack`. Carry the state on the traversal entry, the way `depth`
   already is: add a field to the stack entry struct, set each child's value to
   `entry.inside_function || (this node is IS_SCOPE and base
   DEFINITION_FUNCTION)`, and propagate at **both** child-push sites — the
   normal one at :503 and the node-prune re-parent loop at :372. No pop is
   needed, so the prune control flow at :366/:378 stops mattering. Independent
   of `structure :=` and of `prune :=` by construction.
3. **Change the `prune_internal` predicate.** This is what stops the bodies
   disappearing. `ShouldPruneNode` takes the new `inside_function` state and
   subtree-prunes on:

       BINDS_NAME && !IS_EXPORTED && (importable_kind || !inside_function)

   where `importable_kind` is semantic base `DEFINITION_FUNCTION` or
   `DEFINITION_CLASS`. Each clause is load-bearing, and no simpler predicate
   works:

   | case | kind | inside fn | before | after | why |
   |---|---|---|---|---|---|
   | `_CACHE = {}` at module level | variable | no | pruned | pruned | `!inside_function` |
   | `_helper()` method in a class | function | no | pruned | pruned | both clauses |
   | nested `def` in a public `def` | function | yes | pruned | pruned | `importable_kind` |
   | local `x = 1` in a public `def` | variable | yes | pruned | **kept** | the fix |
   | parameter of a public `def` | identifier | yes | pruned | **kept** | the fix |

   Dropping `importable_kind` would stop pruning nested functions and break the
   documented 11 -> 8. Dropping `!inside_function` would stop pruning private
   module-level variables, which is a regression against `main`.
4. **Correct the documentation.** Remove the "no supported language" claim.
   State the measured rule and name Dart as not covered. `prune := ['internal']`
   gains a sentence saying it prunes unexported definitions, and that inside a
   function body it prunes only nested functions and classes, not locals.
5. **Record Dart as its own bug** for a later release. Doing it properly needs
   each adapter to say which node opens a function body — 28 adapters, a large
   refactor, out of scope here.

## Tests, written and verified red before step 1

Golden rule: tests first, each confirmed failing against the current binary.

1. A parameter and a local inside an exported function keep `is_exported`.
2. `prune := ['internal']` keeps the body of a function it keeps: node and
   `assignment` counts on `sample_app.py`.
3. A nested function reports the same `is_exported` at `structure := 'minimal'`
   as at `'full'`.
4. `prune := ['unnamed']` does not change `is_exported` for a name inside a
   lambda.
5. The nested-function exclusion still holds, and `ast_exports` on C++ still
   returns one row for a top-level function.
6. Dart: pinned as the CURRENT measured behaviour with a comment naming this
   bug, so the limitation is recorded rather than silently expected.

## Cost

A full extension rebuild and a fresh full-suite run. The suite needs about
2.5 hours and is serial; `make test_release` invokes the unittest binary
directly, so a truncated run yields no assertion count at all.

## What was done

- `StackEntry` carries `inside_function`, propagated at the root push and at
  **both** child-push sites, including the node-prune re-parent loop. No pop,
  so the prune control flow cannot desynchronise it.
- The `IS_EXPORTED` guard reads `entry.inside_function` instead of scanning
  `scope_node_stack`. `IS_SCOPE` is still required, via the propagation
  condition, so C++ `function_declarator` cannot make a top-level function look
  nested inside itself.
- `ShouldPruneNode` takes `inside_function` and subtree-prunes on
  `BINDS_NAME && !IS_EXPORTED && (importable_kind || !inside_function)`.
- `test/sql/bugs/148_prune_internal_body_and_config.test`, 6 sections.
  Confirmed RED before the implementation: section 1 gave `2|30` against the
  expected `20|45`. Section 4 was also red — `structure := 'minimal'` gave 11
  function definitions and `'none'` gave 11, against 8 at `'full'`.
- Expected values came from simulating the new predicate over the unpruned tree
  BEFORE the code was written, not from the fixed binary. First attempt used
  236 for `DEFINITION_CLASS`; the constant is 248. Recomputed, the numbers were
  identical, because no fixture defines a class inside a function body.
- `g++ -fsyntax-only` on `src/unified_ast_backend.cpp`: exit 0, 0 errors, 9
  pre-existing deprecation warnings.
- Docs corrected: the false "no supported language can import it" sentence, and
  `prune := ['internal']` now has a stated rule in `docs/reference/parameters.md`.

## Still to do

- Rebuild and run the new test file plus the full suite. Blocked while an
  invalidated pre-fix suite run holds `build/release/test/unittest`.
- File the Dart incompleteness as its own bug.
