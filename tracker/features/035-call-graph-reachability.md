# Feature: Transitive Call-Graph Reachability

**Priority:** P2
**Complexity:** Medium
**Status:** Proposed (nominated 2026-08-18)

## Summary

Transitive closure over the call graph: who transitively calls X, what X
transitively reaches, and whether one function can reach a given target. The
inter-procedural companion to def-use, and a stepping stone toward taint/impact
analysis.

## Motivation

`ast_call_graph` gives direct caller→callee edges. Reachability answers the
questions people actually ask:

- **Impact analysis:** "what breaks if I change `parse_config`?" (transitive callers)
- **Reachability-based dead code:** functions unreachable from any entry point
- **Taint-lite:** does any path from a user-input source reach a dangerous sink?

## DuckDB 2.0 note

sitting_duck deliberately avoids recursive CTEs for *tree* traversal (O(1)
`descendant_count` range checks win). But the call graph is a genuine cyclic
graph, and DuckDB 2.0's ~40× recursive-CTE speedup makes transitive closure over
it cheap — so the one v2.0 headline that looks irrelevant to sitting_duck
actually enables this feature. Use recursive-CTE-with-aggregation (`USING KEY`)
for cycle-safe closure.

## Proposed API

- `ast_reachable_from(source, fn)` → functions fn transitively calls
- `ast_callers_transitive(source, fn)` → transitive callers
- `ast_can_reach(source, fn, target)` → BOOLEAN + shortest path
- Selector: `:reaches(name)`, `:reachable-from(name)`

## Honest limits

Method **names** resolve on calls (verified live, cross-language: `calc.add()` →
name `add`), so edges bind by name — better than the stale docs imply. The
remaining imprecision is (a) no receiver-*type* resolution (which `add` when
several classes define one), and (b) receiver/method aren't yet split into clean
queryable fields (#86). Reachability therefore overapproximates on overloaded /
same-named methods until typing lands — but it is far more useful than
"no method edges."

## Relationship to tracker

Layers on `017-ast-get-calls` / `ast_call_graph`; complements the semantic-tier
direction (034). Gated on the DuckDB 2.0 bump (`bump-duckdb-v1.5-variegata` → 2.0).
