# Qualified Name Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.
> **Implementation notes:** See memory file `qualified-name-impl.md` for detailed file locations and code snippets.

**Goal:** Populate `qualified_name` on definition nodes as a typed scope path (e.g., `C/User F/__init__`) to provide a disambiguating join key.

**Architecture:** During the existing DFS tree walk in `unified_ast_backend_impl.hpp`, maintain a scope stack of `<type_code>/<name>` segments. Push when entering a named definition node, pop on second visit. The qualified_name is the space-joined stack. Move the column from inside the `native` struct to a top-level column available at `ContextLevel::NORMALIZED` and above.

**Tech Stack:** C++ (DuckDB extension), DuckDB sqllogictest

---

### Task 1: Test fixture and failing tests

**Files:**
- Create: `test/data/python/qualified_names.py`
- Create: `test/sql/qualified_name.test`

Create Python fixture with: two classes (`User`, `Account`) each with `__init__`, a nested class (`Account.Settings` with its own `__init__`), a top-level function with a nested function. Write sqllogictest covering: parse_ast inline, read_ast file, disambiguation of same-named methods, nested scopes, non-definition nodes get NULL, variable definitions, and the `USING (file_path, qualified_name)` join pattern.

Commit fixture and tests.

---

### Task 2: Core implementation (helper + scope tracking)

**Files:**
- Modify: `src/include/semantic_types.hpp` — add `GetDefinitionTypeCode()` inline function
- Modify: `src/include/unified_ast_backend_impl.hpp` — scope stack in DFS walk

Add `GetDefinitionTypeCode(uint8_t) -> char` mapping DEFINITION_FUNCTION->F, DEFINITION_CLASS->C, DEFINITION_VARIABLE->V, DEFINITION_MODULE->M (mask off refinement bits with `& 0xFC`).

In `ParseToASTResultTemplated`: add `vector<pair<idx_t, string>> scope_stack` before the loop. After semantic fields are populated (after `PopulateSemanticFieldsTemplated`), check if node is a named definition. If so, build qualified_name from scope_stack + new segment, set `ast_node.name_qualified`, push onto scope_stack. On second visit, pop if top matches `entry.node_index`.

Commit.

---

### Task 3: Schema migration (all output paths)

**Files:**
- Modify: `src/unified_ast_backend.cpp` — all schema and projection functions
- Modify: `src/read_ast_streaming_function.cpp` — struct output path
- Modify: `src/include/structured_ast_types.hpp` — remove from NativeContext

This is one atomic task because the build won't succeed with partial changes:

1. **Remove** `string qualified_name` from `NativeContext` struct
2. **Flat table**: Add `qualified_name` VARCHAR column after `flags` in `GetFlatTableSchema/Names`, update `ProjectToTable` column count (17->18) and add output logic
3. **Flat dynamic**: Move `qualified_name` from NATIVE to NORMALIZED in `GetFlatDynamicTableSchema/Names` and `ProjectToDynamicTable`
4. **Hierarchical/struct**: Move from `native_children` to `context_children` in `GetDynamicTableSchema`; update `GetASTStructSchema`; update `read_ast_streaming_function.cpp` struct output
5. **Replace** all `native.qualified_name` references with `name_qualified`

Commit.

---

### Task 4: Build, test, fix

Build the extension. Run `test/sql/qualified_name.test`. Run full test suite. Fix any failures — likely test expectations that reference the native struct schema or column counts. Also update the legacy `ASTType` path in `src/ast_type.cpp` if needed (mirror scope tracking).

Commit fixes.

---

### Task 5: Documentation

Update `docs/api/output-schema.md`, `docs/ai-agent-guide.md`, `AI_AGENT_GUIDE.md` with the new column, format, type codes, and join pattern.

Commit.
