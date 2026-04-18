# Architecture

## How Sitting Duck turns code into data

When you write `SELECT * FROM read_ast('src/**/*.py')`, several things happen:

1. **File discovery** — glob patterns expand to a file list, deduplicated and sorted
2. **Language detection** — each file's extension maps to a tree-sitter grammar
3. **Parsing** — tree-sitter produces a concrete syntax tree (zero-copy, error-recovering)
4. **DFS traversal** — the tree is walked in pre-order, producing one row per node with `node_id` values that encode the traversal order
5. **Semantic enrichment** — each node gets a `semantic_type` from the language adapter's type map, plus `name`, `scope`, `qualified_name`, and optional native context (`signature_type`, `parameters`, `modifiers`, `annotations`)
6. **Streaming output** — rows stream to DuckDB in 2048-row chunks, so large codebases never require the full AST in memory

The result is a flat SQL table where tree structure is encoded in three columns: `parent_id` (immediate parent), `depth` (tree level), and `descendant_count` (subtree size). This encoding makes subtree membership a simple integer comparison — `d.node_id > a.node_id AND d.node_id <= a.node_id + a.descendant_count` — which is what powers `:has()`, combinators, and scope resolution without recursive CTEs.

## Component overview

```
┌──────────────────────────────────────────────────────┐
│                   DuckDB Interface                    │
│  read_ast() · parse_ast() · ast_select() · macros    │
└──────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────┐
│                 Unified AST Backend                   │
│  File handling · streaming · context extraction       │
└──────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────┐
│               Language Adapter Registry               │
│  27 adapters · auto-detection · type maps             │
└──────────────────────────────────────────────────────┘
                          │
                          ▼
┌──────────────────────────────────────────────────────┐
│                 Tree-sitter Parsers                    │
│  Grammar-specific parsing · error recovery            │
└──────────────────────────────────────────────────────┘
```

## Why tree-sitter?

Tree-sitter parsers are **incremental** (fast re-parse on edits), **error-recovering** (produce a tree even for broken syntax), and **grammar-driven** (adding a language is adding a grammar, not writing a parser). Sitting Duck ships 27 grammars as git submodules, each producing the same `TSTree` interface regardless of language.

## Why a flat table?

An AST is a tree, but SQL is relational. Sitting Duck flattens the tree into a table where tree structure is encoded via DFS pre-order numbering:

- **`parent_id`** — the direct parent's `node_id`
- **`depth`** — distance from the root (0 for root)
- **`descendant_count`** — number of nodes in this node's subtree

This gives you O(1) subtree membership (`node_id` range check), O(depth) ancestor walks, and composability with standard SQL joins — without recursive CTEs or graph queries.

## The semantic type system

Every AST node type from every language maps to a universal 8-bit `semantic_type`:

```
Byte layout: [ ss kk tt ll ]
  ss (bits 6-7): super_kind — 4 top-level categories
  kk (bits 4-5): kind       — 4 sub-categories per super_kind
  tt (bits 2-3): super_type — 4 variants per kind
  ll (bits 0-1): refinement — language-specific (reserved)
```

This lets you write `WHERE semantic_type = 'DEFINITION_FUNCTION'` and match Python `def`, JavaScript `function`, Rust `fn`, Go `func`, Java methods, and C++ function definitions — all with the same predicate. The CSS selector alias `.func` maps to the same check.

See [Semantic Types Reference](../reference/semantic-types.md) for the full type table.

## The scope system

Every node carries a `scope` STRUCT with precomputed shortcuts:

```
scope.current   — nearest enclosing scope's node_id
scope.function  — nearest enclosing function's node_id
scope.class     — nearest enclosing class's node_id
scope.module    — nearest enclosing module/namespace's node_id
scope.stack     — full ancestor chain with semantic kinds (scope nodes only)
```

These are computed during the DFS traversal by maintaining a stack of scope-creating nodes. When a node has `IS_SCOPE` in its flags, it pushes onto the stack; when traversal leaves its subtree, it pops. Every node reads the stack to fill its scope fields.

This eliminates the range-join pattern that would otherwise be needed to answer "what function is this inside?" — a question that comes up in call-graph queries, scope resolution, and the `:scope()` CSS pseudo-class.

## The CSS selector engine

`ast_select` parses CSS selectors using tree-sitter's own CSS grammar (sitting duck querying itself), then translates the parsed selector AST into SQL conditions. The engine is implemented entirely in SQL macros — no C++ selector logic.

The dispatch splits by selector root type:
- **Simple selectors** (type, class, id, attribute) — scan the AST with filter predicates
- **Combinators** (descendant, child, sibling, adjacent) — join the AST against itself using the DFS range check
- **Pseudo-classes** (`:has`, `:not`, `:scope`, `:calls`, etc.) — correlated subqueries against the AST
- **Pseudo-elements** (`::callers`, `::callees`, `::parent`) — post-filter navigation from matched nodes

See [CSS Selector Syntax](../reference/css-selectors.md) for the full selector reference.

## For contributors

Implementation details for people working on the codebase:

| Area | Key files |
|------|-----------|
| Extension entry point | `src/sitting_duck_extension.cpp` |
| AST backend + output | `src/unified_ast_backend.cpp`, `src/include/unified_ast_backend_impl.hpp` |
| Language adapters | `src/language_adapters/*.cpp`, `src/include/*_native_extractors.hpp` |
| Type definitions | `src/language_configs/*.def` |
| Semantic type system | `src/include/semantic_types.hpp`, `src/semantic_type_functions.cpp` |
| CSS selector engine | `src/sql_macros/css_selectors.sql` |
| Scope resolution | `src/sql_macros/scope_resolution.sql` |
| Grammar patching | `scripts/apply_grammar_patches.sh` |

See [Adding Languages](../development/adding-languages.md) for the full language-addition workflow and [Contributing](../development/contributing.md) for development setup.
