# Feature: Structural Clone Detection (Subtree Fingerprinting)

**Priority:** P2
**Complexity:** Low-Medium
**Status:** Proposed (nominated 2026-08-18)

## Summary

A structural hash per subtree so copy-paste and near-duplicate code fall out of a
`GROUP BY`. The highest leverage-to-effort feature on this list, and pure to the
"code as data" thesis.

## Motivation

Clone/duplication detection is a staple of code health — DRY, refactoring
candidates, "this bug pattern recurs in 12 places," license/plagiarism. No AST
scanner does it by *aggregation*, but sitting_duck can, because structure is just
a column.

## Proposed API

- `ast_fingerprint(source, normalize := 'type')` → adds a `fingerprint UBIGINT` per node.
  - `normalize := 'type'` → Type-1 (exact) + Type-2 (identifier-insensitive), blanking `name`
  - `normalize := 'type+literal'` → near-clones (blank literals too)
- Recipe:
  ```sql
  SELECT fingerprint, count(*) AS copies,
         list(file_path || ':' || start_line) AS sites
  FROM ast_fingerprint('src/**')
  WHERE descendant_count > 20
  GROUP BY 1 HAVING count(*) > 1
  ORDER BY copies DESC;
  ```

## Design sketch

Merkle-style: `hash(node) = combine(semantic_type, normalized_name?, ordered
child hashes)`, folded bottom-up in the DFS pass already made (one extra pass
over the tree). `descendant_count` gives a free size filter to suppress trivial
matches. Feasible as a pure-SQL prototype today via recursive aggregation over
`read_ast` before promoting to native.

## Relationship to tracker

Independent of the semantic-tier features (034/035); complements
`ast_function_metrics`. Cheapest win — good early-nomination candidate.
