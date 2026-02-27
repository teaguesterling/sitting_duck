# Name Extraction Audit -- Master Report

**Date:** 2026-02-27
**Scope:** All 17 languages with test data, plus TypeScript (shared JS config)
**Total definition nodes audited:** ~600 (post `is_construct` filtering)

---

## Executive Summary

The name extraction system is fundamentally sound. Keyword filtering works correctly (269 keyword-named definitions reduced to 4 legitimate survivors), root nodes are properly handled, and many languages have good extraction coverage. However, there are **~220 definition nodes producing NULL names** that should have names, spread across 14 languages. The root causes fall into three categories:

1. **FIND_IDENTIFIER strategy cannot find identifiers** because tree-sitter child node types do not match the generic lookup list (affects Swift properties, Bash variables, Dart locals, Kotlin properties, Lua variables, HCL blocks/objects, C# variables, PHP parameters).
2. **FIND_IDENTIFIER searches only immediate children** but the identifier is nested deeper in the tree (affects C++ `function_definition` via `function_declarator`, Bash `declaration_command` where the variable_name is inside a nested `variable_assignment`).
3. **CUSTOM strategy has incomplete handling** for certain node types (Swift `init_declaration`/`deinit_declaration` missing from custom switch, HCL `block` with no labels like `locals {}`).
4. **Over-extraction** in Bash: `heredoc_redirect` and `file_redirect` use NODE_TEXT which dumps the entire redirect content as the name.

## Methodology

1. Query results from `read_ast()` across all test data files, filtering for DEFINITION_* semantic types
2. Post-filter through `is_construct()` to match what users see
3. Cross-reference NULL-named definitions against:
   - `*_types.def` configs (extraction strategy configured)
   - `*_adapter.cpp` custom logic (CUSTOM strategy handlers)
   - `language_adapter.cpp` generic `ExtractByStrategy()` (FIND_IDENTIFIER fallback chain)
4. Identify root cause per node type by tracing the extraction path

---

## Findings by Severity

### Critical: Definitions with NULL Names

#### Swift (96 NULL-named definitions, 82 properly named)

| Node Type | Semantic Type | Strategy | Count | Root Cause | Suggested Fix |
|-----------|--------------|----------|-------|------------|---------------|
| `property_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 75 | Swift properties use `pattern` child for the variable name (e.g., `var x: Int`), not `identifier` or `simple_identifier`. FIND_IDENTIFIER searches `simple_identifier` and `identifier` but the actual name is inside a `pattern` node. The custom handler in the adapter (line 74) checks for `property_declaration` but only under the CUSTOM branch -- however the config says FIND_IDENTIFIER, not CUSTOM. | Change strategy to CUSTOM, or add `pattern` handling to FIND_IDENTIFIER, or add custom extraction for pattern-based name lookup in the adapter. |
| `init_declaration` | DEFINITION_FUNCTION | CUSTOM | 8 | The CUSTOM handler at line 67 searches for `simple_identifier`, but Swift `init` declarations do not have a `simple_identifier` child -- the name is the keyword `init` itself. Convention should return `"init"`. | Return literal `"init"` when node type is `init_declaration` and no `simple_identifier` is found. |
| `deinit_declaration` | DEFINITION_FUNCTION | CUSTOM | 0-1 | Same as `init` -- should return `"deinit"`. | Return literal `"deinit"` for `deinit_declaration`. |
| `subscript_declaration` | DEFINITION_FUNCTION | CUSTOM | 0-1 | Should return `"subscript"` as the conventional name. | Return literal `"subscript"` for `subscript_declaration`. |

**Root Cause Analysis (Swift `property_declaration`):** The config at line 172 uses `FIND_IDENTIFIER`, which in `ExtractByStrategy()` tries child types in order: `identifier`, `property_identifier`, `field_identifier`, `qualified_identifier`, `name`, `simple_identifier`, `type_identifier`. However, Swift's tree-sitter grammar wraps the property name inside a `pattern` node, not any of these types. The adapter has dead code at line 74 that handles `property_declaration` under the CUSTOM branch, but the config says FIND_IDENTIFIER, so that code is never reached.

**Proposed Fix:** Change `property_declaration` strategy from `FIND_IDENTIFIER` to `CUSTOM` in `swift_types.def`. This activates the existing adapter code at line 74. Alternatively, the CUSTOM handler needs to be updated to look deeper into `pattern` children for a `simple_identifier`.

---

#### Bash (51 NULL-named definitions, 15 properly named)

| Node Type | Semantic Type | Strategy | Count | Root Cause | Suggested Fix |
|-----------|--------------|----------|-------|------------|---------------|
| `variable_assignment` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 28 | Bash variable assignments use `variable_name` child, not `identifier`. FIND_IDENTIFIER searches for `identifier` first, but Bash's tree-sitter grammar uses `variable_name` as the child type. The adapter fallback (line 73-83) handles this correctly, but only when `config` is NULL (unknown type). Since the config IS found, it uses `ExtractByStrategy` which fails. | Add `variable_name` to the FIND_IDENTIFIER fallback chain, OR change strategy to CUSTOM and implement handler. |
| `declaration_command` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 18 | `declare -a FILES_PROCESSED` -- the variable name is inside a nested `variable_assignment` child, not a direct `identifier` child. FIND_IDENTIFIER only searches immediate children. | Needs CUSTOM strategy that looks inside child `variable_assignment` for `variable_name`, or recurses one level. |
| `function_definition` | DEFINITION_FUNCTION | FIND_IDENTIFIER | 5 | Bash function names use `word` child type (e.g., `function name {}`). FIND_IDENTIFIER does not search for `word`. The adapter fallback at line 69-72 handles this correctly for unconfigured types, but since there IS a config, the fallback is never reached. | Add `word` to FIND_IDENTIFIER chain, or change to CUSTOM. |

**Over-extraction (Bash):**

| Node Type | Semantic Type | Strategy | Count | Issue |
|-----------|--------------|----------|-------|-------|
| `heredoc_redirect` | EXTERNAL_IMPORT | NODE_TEXT | 1 | Returns entire heredoc content as name: `"<< EOF\nUsage: $0..."`. Should extract just the delimiter or be set to NONE. |
| `file_redirect` | EXTERNAL_IMPORT | NODE_TEXT | 1 | Returns redirect syntax as name: `'< "$file_path"'`. Should extract the file path or be set to NONE. |

**Semantic Type Issue:** `heredoc_redirect` and `file_redirect` are classified as `EXTERNAL_IMPORT`, which is semantically incorrect. These are I/O operations, not imports. Should be `EXECUTION_STATEMENT` or similar.

---

#### Dart (45 NULL-named definitions, 88 properly named)

| Node Type | Semantic Type | Strategy | Count | Root Cause | Suggested Fix |
|-----------|--------------|----------|-------|------------|---------------|
| `local_variable_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 23 | Dart local variables wrap the name in a nested structure. `final dx = x - other.x;` has tree structure where `identifier` is not an immediate child of `local_variable_declaration`. | Needs investigation of Dart tree-sitter grammar structure; likely requires CUSTOM or deeper traversal. |
| `method_signature` | DEFINITION_FUNCTION | FIND_IDENTIFIER | 12 | `factory Point.fromMap(...)` -- the name may be in a `constructor_signature` or `method_signature` wrapper, and FIND_IDENTIFIER cannot find it at the immediate child level. | Needs CUSTOM extraction for Dart method signatures. |
| `declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 5 | Generic `declaration` node where identifier is nested. | Investigate Dart grammar tree structure. |
| `formal_parameter` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 5 | Parameters where `identifier` is not immediate child. | Check if `simple_identifier` or `name` is the right child type. |
| `record_field` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 4 | Dart 3.0+ record fields. | Needs investigation of record grammar. |

---

#### Kotlin (15 NULL-named definitions, 29 properly named)

| Node Type | Semantic Type | Strategy | Count | Root Cause | Suggested Fix |
|-----------|--------------|----------|-------|------------|---------------|
| `property_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 12 | FIND_IDENTIFIER searches for `identifier`, `simple_identifier` etc. but Kotlin `property_declaration` wraps the name in a `variable_declaration` child which then contains `simple_identifier`. The search is only one level deep. | Change to CUSTOM (the existing custom handler at line 66 matches `*declaration*` patterns and would handle this) or recursively search. |
| `primary_constructor` | DEFINITION_FUNCTION | NONE | 3 | Strategy is explicitly NONE. The constructor has no independent name, but the parent class name could be used. | Arguably correct -- constructor name IS the class name. Consider returning parent class name, or leave as legitimate NULL. |

**Note:** The Kotlin adapter's CUSTOM handler (line 66) uses `node_type.find("declaration")` which would match `property_declaration`, but the config uses `FIND_IDENTIFIER` not `CUSTOM`, so the custom handler is never reached. Changing the strategy would fix this.

---

#### Lua (12 NULL-named definitions, 13 properly named)

| Node Type | Semantic Type | Strategy | Count | Root Cause | Suggested Fix |
|-----------|--------------|----------|-------|------------|---------------|
| `variable_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 6 | Lua global variable declarations (`M = {}`) have the identifier inside a `variable_list` child, not as a direct child. FIND_IDENTIFIER only checks immediate children. | Add CUSTOM handling or recursive search. |
| `local_variable_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 6 | Same issue: `local M = {}` wraps identifier in a nested structure. | Same fix needed. |

---

#### C++ (11 NULL-named definitions, 46 properly named)

| Node Type | Semantic Type | Strategy | Count | Root Cause | Suggested Fix |
|-----------|--------------|----------|-------|------------|---------------|
| `function_definition` | DEFINITION_FUNCTION | FIND_QUALIFIED_IDENTIFIER | 6 | The FIND_QUALIFIED_IDENTIFIER strategy calls `ExtractQualifiedIdentifierName()` which looks for `function_declarator` children. Likely failing for certain function patterns (templated, operator overloads, or destructor definitions). | Investigate which specific C++ functions fail. The existing `ExtractCppCustomName` has good logic -- consider routing through CUSTOM. |
| `declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 5 | C++ `declaration` nodes can have deeply nested structures (e.g., `const auto& x = expr;` where identifier is inside an `init_declarator` inside a `declarator`). FIND_IDENTIFIER only checks immediate children. | The FIND_IN_DECLARATOR strategy exists but is not used for `declaration`. Consider switching. |

---

#### HCL (18 NULL-named definitions, 51 properly named)

| Node Type | Semantic Type | Strategy | Count | Root Cause | Suggested Fix |
|-----------|--------------|----------|-------|------------|---------------|
| `object_elem` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 14 | HCL `object_elem` uses an `expression` child for the key, which wraps a `variable_expr` which wraps an `identifier`. Three levels deep -- FIND_IDENTIFIER only checks one. | Needs CUSTOM extraction or multi-level search. |
| `block` | DEFINITION_CLASS | CUSTOM | 4 | Blocks like `locals {}` have no labels -- the CUSTOM handler correctly returns empty string for unlabeled blocks. This is partially legitimate (no label = no name), but could return the block type (`locals`, `terraform`, etc.) as a fallback. | Consider returning the block type identifier when no labels exist. |

---

#### Other Languages (lower count)

| Language | Node Type | Semantic Type | Strategy | Count | Root Cause |
|----------|-----------|--------------|----------|-------|------------|
| C# | `variable_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 8 | C# `variable_declaration` nests the name inside a `variable_declarator` child. Same depth issue. |
| C# | `using_directive` | EXTERNAL_IMPORT | FIND_IDENTIFIER | 3 | Using directives use `qualified_name` or `name` child, not bare `identifier`. |
| PHP | `simple_parameter` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 8 | PHP parameters use `variable_name` (with `$` prefix) child, not `identifier`. FIND_IDENTIFIER searches `name` (which PHP uses for class/function names) but `variable_name` for variables. |
| PHP | `property_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 4 | Same issue: PHP property names are `variable_name` nodes, not `identifier`. |
| JS | `lexical_declaration` | DEFINITION_VARIABLE | FIND_IDENTIFIER | 7 | `const calc = new Calculator();` -- the name `calc` is inside a `variable_declarator` child, not a direct `identifier` child. |
| Zig | `struct_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | 2 | Zig uses `const S = struct {}` pattern. The name `S` is in the parent `variable_declaration`, not inside `struct_declaration` itself. |
| Zig | `test_declaration` | DEFINITION_FUNCTION | FIND_IDENTIFIER | 2 | `test "name" {}` -- the name is a `string` child, not an `identifier`. |
| Zig | `enum_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | 1 | Same as struct: name is in parent variable_declaration. |
| Zig | `union_declaration` | DEFINITION_CLASS | FIND_IDENTIFIER | 1 | Same pattern. |

---

### Low: Over-Extracted Names

Only 2 cases found, both in Bash:

1. **`heredoc_redirect`** with `NODE_TEXT` strategy: Returns entire heredoc content including multi-line body
2. **`file_redirect`** with `NODE_TEXT` strategy: Returns redirect operator plus filename

Both should either use `NONE` or extract only the specific file/delimiter token.

Additionally, both are semantically misclassified as `EXTERNAL_IMPORT` when they are actually I/O operations.

---

### Informational: Legitimate NULL Names

The following produce NULL names by design (anonymous/unnamed constructs):

| Language | Node Type | Count | Reason |
|----------|-----------|-------|--------|
| Swift | `lambda_literal` | 13 | Anonymous closures `{ params in body }` |
| Kotlin | `lambda_literal`, `anonymous_function` | 3+ | Anonymous lambdas and functions |
| JS | `arrow_function` | 3+ | Anonymous arrow functions `() => {}` |
| JS | `function_expression` | 2+ | Anonymous function expressions |
| C# | `lambda_expression` | 2+ | Anonymous lambdas `(x) => x * 2` |
| Lua | `function_definition` | 2+ | Anonymous function values |
| Kotlin | `primary_constructor` | 3 | Constructor name is implicitly the class name |

These are correctly producing empty names via `FIND_ASSIGNMENT_TARGET` (which returns empty when not in an assignment context) or via `NONE` strategy.

---

### Informational: Working Correctly

**Keyword filtering:** 269 keyword-typed definition nodes in raw data, only 4 pass `is_construct()` filtering. All 4 are legitimate:
- HCL `type` attribute (a valid HCL attribute named "type")
- Lua `init` method (a legitimate method name that happens to be a keyword in other contexts)

**Root nodes:** All 17 languages produce root nodes with `DEFINITION_MODULE` semantic type and empty names, which is correct behavior. R uses `ORGANIZATION_CONTAINER` instead -- minor inconsistency but not a bug.

**Languages with strong extraction:**
- Dart: 88 properly named definitions
- Swift: 82 properly named definitions
- HCL: 51 properly named definitions
- C++: 46 properly named definitions
- Zig: 46 properly named definitions
- C#: 39 properly named definitions
- Rust: 32 properly named definitions
- Kotlin: 29 properly named definitions

---

## Root Cause Analysis: Systematic Patterns

### Pattern 1: FIND_IDENTIFIER Depth Limitation (Most Common)

`FIND_IDENTIFIER` only searches **immediate children** via `FindChildByType()`. Many tree-sitter grammars wrap identifiers inside intermediate nodes:

```
variable_declaration          <-- FIND_IDENTIFIER searches here
  variable_declarator         <-- but identifier is here
    identifier "myVar"        <-- actual name
```

**Affected:** JS `lexical_declaration`, C# `variable_declaration`, Kotlin `property_declaration`, Lua `variable_declaration`/`local_variable_declaration`, HCL `object_elem`, C++ `declaration`

**Potential Fix:** Create a `FIND_IDENTIFIER_RECURSIVE` strategy or add common intermediate node traversal to `FIND_IDENTIFIER`.

### Pattern 2: Language-Specific Child Type Names

Different tree-sitter grammars use different node type names for identifiers:

| Grammar | Identifier Node Type |
|---------|---------------------|
| Most languages | `identifier` |
| Swift | `simple_identifier`, `pattern` |
| Bash | `variable_name`, `word` |
| PHP | `variable_name`, `name` |
| Kotlin | `simple_identifier` |
| Zig | `identifier` (but names are in parent) |

`FIND_IDENTIFIER` covers `identifier`, `simple_identifier`, `name`, `property_identifier`, `field_identifier`, `qualified_identifier`, `type_identifier`, `operator`, `constant`, `setter`. Missing: `variable_name`, `word`, `pattern`.

**Potential Fix:** Add `variable_name`, `word` to `FIND_IDENTIFIER`. For `pattern`, consider adding or using CUSTOM.

### Pattern 3: Names in Parent Nodes (Zig-specific)

Zig types (`struct_declaration`, `enum_declaration`, `union_declaration`) are assigned as values: `const MyStruct = struct {}`. The name `MyStruct` is in the parent `variable_declaration`, not in `struct_declaration` itself. `FIND_IDENTIFIER` looks inside the struct, not above it.

**Potential Fix:** Use `FIND_ASSIGNMENT_TARGET` strategy for these Zig types, or create a CUSTOM handler that looks at the parent node.

### Pattern 4: Dead Custom Code

Several adapters have custom extraction code that is unreachable because the config uses a generic strategy instead of CUSTOM:

- **Swift adapter** lines 74-75: Handles `property_declaration` and `variable_declaration` with `FindChildByType(node, content, "pattern")` -- but config says `FIND_IDENTIFIER`
- **Bash adapter** lines 69-83: Handles `function_definition` (word), `variable_assignment` (variable_name) -- but config says `FIND_IDENTIFIER`
- **Kotlin adapter** lines 66-74: Matches any `*declaration*` pattern -- but config says `FIND_IDENTIFIER`

---

## Suggested GitHub Issues

### Issue 1: Fix FIND_IDENTIFIER to support additional child types
**Priority:** High
**Scope:** `src/language_adapter.cpp` `ExtractByStrategy()`
**Estimate:** Small change

Add `variable_name` and `word` to the `FIND_IDENTIFIER` fallback chain. This directly fixes:
- Bash `variable_assignment` (28 instances)
- Bash `function_definition` (5 instances)
- PHP `simple_parameter` (8 instances)
- PHP `property_declaration` (4 instances)

### Issue 2: Activate dead custom extraction code in Swift, Bash, Kotlin adapters
**Priority:** High
**Scope:** `swift_types.def`, `kotlin_types.def`
**Estimate:** Small config change + testing

Change extraction strategy from `FIND_IDENTIFIER` to `CUSTOM` for:
- Swift `property_declaration` (75 instances) -- existing custom code handles this
- Kotlin `property_declaration` (12 instances) -- existing custom code handles this

For Swift `init_declaration` (8 instances), add `return "init"` fallback when `simple_identifier` is not found in the CUSTOM handler.

### Issue 3: Handle nested identifier patterns (depth > 1)
**Priority:** Medium
**Scope:** Multiple languages
**Estimate:** Medium effort

Create a `FIND_IDENTIFIER_DEEP` strategy or enhance FIND_IDENTIFIER to optionally search common intermediate wrapper nodes (`variable_declarator`, `init_declarator`, `variable_list`, `variable_assignment`). This fixes:
- JS `lexical_declaration` (7 instances)
- C# `variable_declaration` (8 instances)
- Lua `variable_declaration` / `local_variable_declaration` (12 instances)
- HCL `object_elem` (14 instances)
- Bash `declaration_command` (18 instances)
- Dart `local_variable_declaration` (23 instances)
- C++ `declaration` (5 instances)

### Issue 4: Fix Zig type definitions to extract names from parent
**Priority:** Medium
**Scope:** `zig_types.def`
**Estimate:** Small

Zig `struct_declaration`, `enum_declaration`, `union_declaration` should use `FIND_ASSIGNMENT_TARGET` instead of `FIND_IDENTIFIER`, since names are assigned via `const Name = struct {}`.

Also fix `test_declaration` -- its name is a string literal child, not an identifier. Needs CUSTOM handling.

### Issue 5: Fix Bash redirect semantic types and over-extraction
**Priority:** Low
**Scope:** `bash_types.def`
**Estimate:** Small

- Change `heredoc_redirect` and `file_redirect` from `EXTERNAL_IMPORT` to `EXECUTION_STATEMENT` or `COMPUTATION_CALL`
- Change their extraction strategy from `NODE_TEXT` to `NONE` (or extract just the delimiter/path)

---

## Remediation Plan

### Phase 1: Quick Wins (fixes ~120 NULL names)
1. **Add `variable_name` and `word` to FIND_IDENTIFIER** in `language_adapter.cpp`
2. **Switch Swift `property_declaration` to CUSTOM** in `swift_types.def`
3. **Add `init`/`deinit`/`subscript` literal name fallbacks** in `swift_adapter.cpp`
4. **Switch Zig type declarations to FIND_ASSIGNMENT_TARGET** in `zig_types.def`
5. **Fix Bash redirect semantics and over-extraction** in `bash_types.def`

### Phase 2: Depth Enhancement (fixes ~80 more NULL names)
1. **Implement FIND_IDENTIFIER_DEEP or enhance FIND_IDENTIFIER** with common intermediate node traversal
2. **Apply to:** JS `lexical_declaration`, C# `variable_declaration`, Lua variables, HCL `object_elem`, Bash `declaration_command`, Dart `local_variable_declaration`, C++ `declaration`

### Phase 3: Language-Specific Custom Handlers (remaining ~20)
1. **Dart method_signature, formal_parameter, record_field** -- investigate grammar structure, add CUSTOM handlers
2. **HCL labelless blocks** -- decide whether to return block type as name
3. **C++ complex function definitions** -- investigate which patterns fail FIND_QUALIFIED_IDENTIFIER
4. **C# using_directive** -- may need to search for `qualified_name` in addition to current search set

---

## Appendix: Extraction Strategy Reference

| Strategy | Behavior | Used By |
|----------|----------|---------|
| `NONE` | Returns empty string | Root nodes, anonymous constructs |
| `NODE_TEXT` | Returns full text of node | Literals, keywords |
| `FIND_IDENTIFIER` | Searches immediate children for common identifier types | Most definition nodes |
| `FIND_QUALIFIED_IDENTIFIER` | Looks for qualified names (C++ `Ns::Name`) | C++ function_definition |
| `FIND_IN_DECLARATOR` | Looks inside declarator children | C/C++ specific |
| `FIND_ASSIGNMENT_TARGET` | Looks at parent for assignment LHS | Lambdas, anonymous functions |
| `FIND_CALL_TARGET` | Extracts function/method name from call | Call expressions |
| `FIND_PROPERTY` | Searches for property_identifier child | Property access |
| `CUSTOM` | Routes to adapter-specific `ExtractNodeName` | Language-specific patterns |
