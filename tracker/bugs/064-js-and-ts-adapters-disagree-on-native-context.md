# 064 — JavaScriptAdapter and TypeScriptAdapter return different `modifiers` and `signature_type` for byte-identical source

**Status:** Not fixed. Measured, recorded, and deliberately left alone: it
predates feature 058 and fixing it means changing one adapter's native context
extraction, which is a behaviour change for every `.js` or `.ts` file.
**Found by:** feature 058 (the JSX node type declarations), 2026-11-16. The new
test compares two byte-identical fixtures across the two languages, and the
unrestricted comparison returned 72 differing rows. None of them were jsx
nodes.
**Related:** `tracker/features/058-jsx-node-types.md` explains why the two
adapters are siblings. `test/sql/languages/jsx_node_types.test` pins the
divergence so it cannot grow silently.

## What is wrong

`test/data/jsx/shapes.jsx` and `test/data/tsx/shapes.tsx` hold the same bytes.
`read_ast` gives them the same 149 nodes, the same tree, the same types, the
same names, the same semantic types and the same flags. Two columns differ:

| node type | rows differing |
|---|---|
| `identifier` | 22 |
| `member_expression` | 2 |
| `property_identifier` | 6 |
| `variable_declarator` | 6 |

36 rows, in both directions, 72 in all. The columns are `modifiers` and
`signature_type`, which come from each adapter's own native context extraction.

## The measurement

Measured against `build/release/duckdb` at `4fb1e21`.

    type                 js_mod  tsx_mod                    js_sig  tsx_sig
    variable_declarator  []      []                         const   NULL
    identifier(a)        []      [in_variable_declarator,   NULL    NULL
                                  variable_name]
    identifier(div)      []      [in_jsx_opening_element]   NULL    identifier
    property_identifier  []      [property]                 NULL    property

TypeScript's extractor fills both columns and JavaScript's leaves them empty.
`variable_declarator.signature_type` runs the OTHER way: javascript reports
`const`, tsx reports NULL.

## Who this hurts

A query that filters on `modifiers` or `signature_type` answers differently for
`.js` and `.jsx` than for `.ts` and `.tsx`, on the same code. The
`variable_declarator` case is the worse half, because javascript carries the
richer value there and tsx loses it: a `const`/`let` distinction that is
available in one language and not in its superset.

## Why it is not fixed here

Feature 058 is constrained to declaring node types. Every candidate fix changes
what one adapter reports for files that have nothing to do with JSX:

  1. Give JavaScriptAdapter TypeScript's extractor behaviour — changes
     `modifiers` for every `.js` identifier in the corpus.
  2. Give TypeScriptAdapter javascript's `variable_declarator` signature_type —
     changes `.ts` output.
  3. Hoist the shared parts into one place, which is the real fix and is a
     refactor of two native extractor headers.

The right scope for this is the native context work, not a `.def` edit.
