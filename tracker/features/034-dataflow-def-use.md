# Feature: Intra-procedural Dataflow (Def-Use Chains)

**Priority:** P1
**Complexity:** High
**Status:** Proposed (nominated 2026-08-18)

## Summary

Link variable/parameter definitions to their reads within a scope, producing
def-use chains as queryable data. The first real step from *structural* analysis
toward *semantic* analysis.

## Motivation

sitting_duck resolves references (`ast_find_references`) and calls
(`ast_call_graph`), but has no dataflow. Def-use chains unlock a class of
analyses reference-resolution alone can't:

- unused variables / dead assignments
- use-before-def
- redundant reassignment
- "is this parameter ever read?"

This is the boundary that separates a structural query engine from a semantic
one — the line where CodeQL's value begins.

## Proposed API

- `ast_def_use(source, language := NULL)` → `(def_node_id, use_node_id, name, scope_id, def_kind, use_kind)`
- Selector integration: `:unused`, `:reassigned`, `:used-before-def`.

## Design sketch

Build within the existing DFS pass using `scope.current`/`scope.function` +
`qualified_name` + node order (`node_id` is pre-order, so within a scope a def
precedes a use ⇔ lower id, modulo hoisting/closures). Definitions =
assignments/params/declarations; uses = identifier references bound to the
nearest enclosing definition of that name (shadowing already handled by
`ast_resolve`).

## Honest limits

Syntactic and intra-procedural: no aliasing, no field-sensitivity, no
interprocedural flow; language-specific hoisting/closure caveats. This is
dataflow-*lite*, not an IFDS/IDE solver — but it is the foundation live-variable,
taint, and constant-propagation would build on.

## Relationship to tracker

Builds on `016-ast-find-references` / `017-ast-get-calls` / the `scope` struct.
Distinct from #86 (receiver resolution) and `planned/005-ast-diff-analysis`.
Prerequisite for any future taint work.
