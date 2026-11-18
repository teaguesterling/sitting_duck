# 064 — JavaScriptAdapter and TypeScriptAdapter return different `modifiers` and `signature_type` for byte-identical source

**Status:** Not fixed. Filed as **#346** on 2026-11-17. Measured, recorded, and
deliberately left alone: it predates feature 058 and fixing it means changing one
adapter's native context extraction, which is a behaviour change for every
`.js` or `.ts` file.
**Re-measured 2026-11-17 on a current release build.** Every figure below still
holds. The CAUSE below was understated: see §6.
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

---

## 6. The cause, sharpened (added 2026-11-17, while filing #346)

The sections above describe this as two adapters that "legitimately disagree".
That is too generous, and it hid the real finding. Measured:

**There is exactly one declaration.** `variable_declarator` is declared once, at
`languages/javascript/javascript_types.def:194`, and `typescript_types.def`
`#include`s that file. #230 deleted typescript's own dead entry. So all three
languages read the same row, with the same declared strategy:

    SELECT language, node_type, native_strategy FROM ast_type_map()
    WHERE node_type = 'variable_declarator'
      AND language IN ('javascript','typescript','tsx');
    -- javascript | variable_declarator | variable_with_type
    -- tsx        | variable_declarator | variable_with_type
    -- typescript | variable_declarator | variable_with_type

**The two implementations of that one strategy return different KINDS of fact.**
`src/include/javascript_native_extractors.hpp` implements
`VARIABLE_WITH_TYPE` as `ExtractDeclarationType`, and its own comment says what
it is doing:

    // JavaScript variables don't have explicit types (pre-TypeScript)
    // But we can detect declaration patterns
    context.signature_type = ExtractDeclarationType(node, content);

It returns `const`, `let` or `var` — the declaration KEYWORD.
`src/include/typescript_native_extractors.hpp` implements the same strategy as
`ExtractVariableType`, which returns the declared TYPE. `tracker/bugs/061`'s
sibling issue #221 measured that value (`const g: Greeting` → `Greeting`) and
its outcome kept it.

So `signature_type` holds the declaration keyword in one language and the
declared type in the other, under one declared strategy. A consumer cannot tell
which it has without knowing the language.

**Why this matters for the fix.** §"Why it is not fixed here" lists three
candidates and prefers none. With the cause stated correctly there is a fourth,
and it is the only one that keeps both facts:

  4. `const`/`let`/`var` is a MODIFIER, not a type, and `modifiers` is where
     every other language puts it. Move it there and leave `signature_type` for
     the declared type.

Whichever is chosen, `VARIABLE_WITH_TYPE` needs a written contract naming which
of the two it returns. Today both readings satisfy the strategy's name, which is
why two adapters implemented it two ways with no test failing.
