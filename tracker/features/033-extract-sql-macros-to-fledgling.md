# Feature #033: Extract SQL macros to Fledgling

**Status:** planned
**Priority:** medium
**Target:** v1.9.0 or v2.0.0 (breaking change — needs deprecation cycle)

## Summary

Move the pure-SQL analysis macros out of sitting_duck into Fledgling (source-sextant). sitting_duck becomes a focused parsing engine; Fledgling owns the query/analysis layer.

## Motivation

- `css_selectors.sql` is 1,205 lines — 28% of all SQL macros — originally intended as a C++ prototype that turned out fast enough to ship as-is. There's no technical reason for it to be compiled into the C++ extension binary.
- All analysis macros are pure SQL that only depend on `read_ast`, `parse_ast`, and `is_semantic_type`. Fledgling already loads sitting_duck, so the dependency is satisfied.
- Moving to Fledgling enables SQL hot-reload iteration vs C++ recompile, which matters for actively evolving features (multi-rule dispatch WIP, custom predicates new in v1.8.0).
- pluckit has a parallel Python-side selector implementation that duplicates and drifts from `ast_select`. Centralizing in Fledgling gives both pluckit and the MCP tools a single canonical source.

## What stays in sitting_duck (~390 lines)

- C++ functions: `read_ast`, `parse_ast`, `parse_ast_flat`, semantic type system, extraction config, prune/max_depth
- `semantic_predicates.sql` — thin wrappers around C++ (`is_function_definition`, `is_function_call`, etc.)
- `file_utilities.sql` — `read_text`
- `parse_ast_list_table.sql` — utility wrapper

## What moves to Fledgling (~3,900 lines)

| File | Lines | Key macros |
|------|-------|------------|
| `css_selectors.sql` | 1,205 | `ast_select`, `ast_select_from` |
| `ast_select_rules.sql` | 184 | `ast_select_rules`, `ast_select_list` |
| `tree_navigation.sql` | 850 | `ast_children`, `ast_descendants`, `ast_get_calls`, `ast_call_graph`, etc. |
| `scope_resolution.sql` | 530 | `ast_exports`, `ast_imports`, `ast_resolve`, `ast_find_references`, `ast_callees`, `ast_callers` |
| `pattern_matching.sql` | 1,005 | `ast_match` |
| `relational_operators.sql` | 150 | `ast_has`, `ast_inside`, `ast_precedes`, `ast_follows` |

## Migration plan

1. **v1.9.0 (deprecation):** Ship macros in both sitting_duck and Fledgling. sitting_duck macros emit a one-time warning on first use (if DuckDB supports it) or document the deprecation in release notes.
2. **v2.0.0 (removal):** Remove macros from sitting_duck. Users who need them without Fledgling can `source` the SQL files directly.

## Breaking change impact

- Standalone sitting_duck users (no Fledgling) lose `ast_select` and all analysis macros.
- pluckit depends on sitting_duck directly — would need to either depend on Fledgling or vendor the SQL files.
- Fledgling MCP tools and workflows: no change (already load sitting_duck).

## Side benefit

Resolves the pluckit selector duplication: pluckit can delegate to Fledgling's `ast_select` instead of maintaining its own Python-side selector compiler in `_sql.py` and `selectors.py`.
