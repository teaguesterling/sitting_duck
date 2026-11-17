# 065 — `tsx` is the only built-in language with no generated reference page

**Status:** Not fixed. Measured and recorded. The fix is a generator change or
a `languages/tsx/` table, and both are out of feature 058's scope.
**Found by:** feature 058 (the JSX node type declarations), 2026-11-16, while
regenerating `docs/reference/languages/` after adding seven entries.
**Related:** #329 added the `tsx` language. `tracker/bugs/063` is the other
loose end from that pull request.

## What is wrong

`docs/scripts/generate_language_reference.py` writes one page per
`languages/<lang>/*_types.def` table. `tsx` owns no table: `languages/tsx/`
holds only `module.cmake` and `tsx_adapter.cpp`, and `TSXAdapter` takes
TypeScript's node configs at run time.

So a reader looking up tsx in the reference finds nothing.

## The measurement

Measured at `4fb1e21`:

    $ ls docs/reference/languages/ | wc -l
    31                      # 30 language pages plus index.md
    $ ls docs/reference/languages/tsx.md
    ls: cannot access 'docs/reference/languages/tsx.md': No such file or directory
    $ grep -c jsx docs/reference/languages/typescript.md
    0

The seven new JSX entries appear on `javascript.md` alone, although all three
of javascript, typescript and tsx declare them at run time
(`ast_type_map()` reports 8 jsx rows for each).

`scripts/audit_unreachable_declarations.py` has the same blind spot, from the
same cause:

    tsx               0            0

Zero declarations, because the auditor also reads the `.def` files rather than
the runtime. That row is not a result about tsx.

## Who this hurts

Anyone reading the published reference for tsx. It also means a future JSX
change shows up in one generated page while three languages change.

## Candidate fixes

  1. Have the generator follow the `#include` chain and the adapter's config
     source, and emit a page per REGISTERED language rather than per table.
     This is the fix that also repairs the auditor's `tsx 0 0`.
  2. Emit a short `tsx.md` that states it inherits typescript's table and links
     to it. Cheap, and leaves the auditor wrong.
  3. Generate both pages from `ast_type_map()` at run time instead of from the
     source tables. Measures the artifact rather than its description, and
     costs a built binary at docs time.
