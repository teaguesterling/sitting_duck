# Feature: Distinguish namespace from module in the semantic type taxonomy

**Status:** Open (design)
**Priority:** Low — current naming convention covers the common case
**Filed:** 2026-04-14

## Background

The current 8-bit semantic type byte is laid out as `[ ss kk tt ll ]`:
- `ss` (bits 6-7): super_kind (4 values)
- `kk` (bits 4-5): kind within super_kind (4 values)
- `tt` (bits 2-3): super_type within kind (4 values)
- `ll` (bits 0-1): language-specific refinement (4 values, marked unused)

The `DEFINITION` kind has all four super_type slots taken:
- `DEFINITION_FUNCTION` (0xF0)
- `DEFINITION_VARIABLE` (0xF4)
- `DEFINITION_CLASS` (0xF8)
- `DEFINITION_MODULE` (0xFC)

`DEFINITION_MODULE` currently covers anything that defines a named
container — Python modules, C++ namespaces, Rust mods, Java package
declarations, etc. The `scope.module` field on every node points to
the nearest such ancestor regardless of which flavor.

## Question

Should we distinguish "module" from "namespace" at the semantic type
level so callers can ask, e.g., "find calls inside any C++ namespace"
vs "find calls inside any Python module"?

## Constraints

- All four DEFINITION super_type slots are used; we can't add a
  DEFINITION_NAMESPACE without breaking the byte layout.
- The `ll` refinement bits (bits 0-1) are documented as language-specific
  but currently unused. We could repurpose them for cross-language
  module-flavor distinctions: e.g., DEFINITION_MODULE_NAMESPACE = 0xFD,
  DEFINITION_MODULE_PACKAGE = 0xFE, DEFINITION_MODULE_MOD = 0xFF.
- That would change the meaning of `ll` from "language-specific" to
  "shared semantic refinement," which is a documentation/design shift
  worth thinking through carefully.

## Options

### A. Keep DEFINITION_MODULE as one bucket, document the dual role

What we have today. `scope.module` covers both. NAMESPACE/NS are aliases
for MODULE in `is_semantic_type`. Cross-language queries treat module
and namespace as the same thing. Not technically wrong — most cross-
language analyses don't need to distinguish them.

### B. Refine DEFINITION_MODULE via the `ll` bits

Add three sub-types using the language-specific refinement bits:
```cpp
constexpr uint8_t DEFINITION_MODULE         = DEFINITION | 0x0C; // 0xFC base
constexpr uint8_t DEFINITION_MODULE_NAMESPACE = DEFINITION | 0x0D; // 0xFD
constexpr uint8_t DEFINITION_MODULE_PACKAGE   = DEFINITION | 0x0E; // 0xFE
constexpr uint8_t DEFINITION_MODULE_MOD       = DEFINITION | 0x0F; // 0xFF
```

Update each language adapter to emit the right refinement (e.g., C++
adapter emits `DEFINITION_MODULE_NAMESPACE` for `namespace_definition`).

`is_semantic_type(st, 'NAMESPACE')` would then mean "exactly the
namespace flavor", while `is_semantic_type(st, 'MODULE')` (using the
existing `& 0xFC` mask) continues to match all module-flavors.

`scope.module_scope` could remain "any module-flavor" or be split into
`scope.module_scope` + `scope.namespace_scope` for slot-level access.

### C. Reorganize the byte layout

Steal a slot from elsewhere in DEFINITION (impossible — all 4 are core)
or move modules to a different kind entirely. Big architectural change,
not warranted.

## Recommendation

**Stay with (A) until a real workload motivates (B).** The cross-language
usefulness of distinguishing namespace from module is mostly about
naming clarity for callers in C++/Rust contexts — the NAMESPACE/NS
aliases address that without any taxonomy change.

If/when we get a concrete need (e.g., a tool that wants to lint C++
namespace usage specifically), revisit with option B.

## Related

- `src/include/semantic_types.hpp` — taxonomy definition
- `src/semantic_type_functions.cpp` — `is_semantic_type` cascade with
  the existing aliases
- `src/include/ast_type.hpp` — `ScopeInfo` struct (would gain
  `namespace_scope` field if we go with B)
