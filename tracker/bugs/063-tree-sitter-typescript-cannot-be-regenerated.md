# 063 — `tree-sitter generate` fails for BOTH dialects of `tree-sitter-typescript`, so `generated_parsers/` cannot be rebuilt for typescript or tsx

**Status:** Not fixed. Measured 2026-11-13. Recorded as a KNOWN LIMITATION of
the parser-generation path, not a user-facing defect: the committed parsers in
`generated_parsers/` are what every build compiles, and they work. What is
broken is the ability to REPRODUCE them.
**Found by:** reviewing PR #329 (the tsx language). The PR compiled tsx's parser
straight out of the `grammars/` submodule, which broke the submodule-free build
contract. Moving it into `generated_parsers/` the house way meant regenerating
it, and the regeneration does not run.
**Related:** `#233` is the same shape for yaml, whose grammar.js throws under
tree-sitter 0.26 / Node 18 and whose committed parser is therefore ABI 13. This
is the second grammar family in that state, and the first one that holds a
dialect the project added itself.

## What is wrong

Both dialects fail, identically:

    $ cd grammars/tree-sitter-typescript/tsx
    $ ../../../third_party/tree-sitter/target/release/tree-sitter generate
    Error when generating parser
    Caused by:
        Unresolved conflict for symbol sequence:
          'using'  identifier  •  '!'  …
        Possible interpretations:
          1:  'using'  (primary_expression  identifier)  •  '!'  …
          2:  'using'  (variable_declarator  identifier  •  '!'  type_annotation)
              (precedence: 'declaration')

`grammars/tree-sitter-typescript/typescript` gives the same error, with the
same two interpretations. The patch `patches/fix-typescript-grammar-require.patch`
was applied first (it edits `common/define-grammar.js`, which both dialects
`require`), so this is not the require-path failure that patch exists for.

`using` declarations come from tree-sitter-javascript's grammar, which both
dialects extend, so the conflict is in shared rules rather than in either
dialect's overlay.

## Why the committed parsers still work

    generated_parsers/tree-sitter-typescript/typescript/src/parser.c  LANGUAGE_VERSION 15  STATE_COUNT 5870
    grammars/tree-sitter-typescript/typescript/src/parser.c           LANGUAGE_VERSION 14  STATE_COUNT 5870
    generated_parsers/tree-sitter-typescript/tsx/src/parser.c         LANGUAGE_VERSION 14  STATE_COUNT 5986

`generated_parsers/MANIFEST` says it was built on 2026-08-31 with tree-sitter
0.26.0. The same CLI refuses the same grammar today. Neither submodule pin has
moved since: `grammars/tree-sitter-typescript` was pinned on 2025-06-09 (upstream
`75b3874`, 2025-01-30) and `third_party/tree-sitter` on 2025-06-17. The two
parsers have the SAME `STATE_COUNT`, so the grammar itself did not change
between the regeneration and the committed upstream file.

**The cause is therefore not established.** The candidates not ruled out: the
CLI binary in `third_party/tree-sitter/target/release/` was built from a
checkout other than the pin, or the Node version that evaluates `grammar.js`
differs from the one used on 2026-08-31. Settling it needs the 2026-08-31
toolchain, which is not recorded anywhere.

## Who this hurts

Nobody at runtime. It hurts the next person who needs to change these grammars:

  - `scripts/generate_all_parsers.sh` fails at its `typescript` entry.
  - `make generate-parsers` (the `USE_PREGENERATED_PARSERS=OFF` path) fails the
    same way, and `typescript` is generated before every independent grammar, so
    an unpregenerated build stops there.
  - tsx cannot be given a 0.26.0 parser, which is why
    `generated_parsers/tree-sitter-typescript/tsx/` holds ABI 14 beside 28
    languages at ABI 15.

## What was done instead

`generated_parsers/tree-sitter-typescript/tsx/src/{parser.c,scanner.c}` are
byte-identical copies of the submodule's own files — the exact bytes the build
compiled while `languages/tsx/module.cmake` reached into `grammars/`. So moving
them changes where the build reads from and nothing about the binary.

## What would fix it

1. Record the toolchain that produced `generated_parsers/` (a CLI version line
   in the MANIFEST is there, a Node version is not, and the CLI's provenance is
   not), so this question is answerable next time.
2. Then either bump `tree-sitter-typescript` to a pin whose grammar generates,
   or add the `conflicts:` entry the CLI asks for, as a patch in `patches/`.

Both change the shipped typescript parser, so both need their own before/after
on the typescript suite. Neither belongs in a build-contract fix.
