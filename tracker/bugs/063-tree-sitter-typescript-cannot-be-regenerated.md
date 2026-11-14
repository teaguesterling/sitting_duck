# 063 — `tree-sitter generate` failed for BOTH dialects of `tree-sitter-typescript`, because the javascript pin moved and the overlay did not

**Status:** FIXED on `feat/grammar-ts-using-conflict` by
`patches/fix-typescript-using-conflict.patch`. Both dialects regenerate, and
`generated_parsers/tree-sitter-typescript/{typescript,tsx}` now hold 0.26.0
output like every other language.
**Found by:** reviewing PR #329 (the tsx language). The PR compiled tsx's parser
straight out of the `grammars/` submodule. Moving it into `generated_parsers/`
the house way meant regenerating it, and the regeneration did not run.
**Related:** `#233` is a DIFFERENT cannot-regenerate state, for yaml, whose
`grammar.js` post-processes `grammar()`'s output and throws under this CLI. That
one is still open. This one looked like it and was not.

## What was wrong

Both dialects failed identically:

    $ cd grammars/tree-sitter-typescript/tsx       # and .../typescript
    $ ../../../third_party/tree-sitter/target/release/tree-sitter generate
    Error when generating parser
    Caused by:
        Unresolved conflict for symbol sequence:
          'using'  identifier  •  '!'  …

**The toolchain was not at fault, and neither was tsx.** The CLI binary was
built 2026-08-31 19:26 from the pin that is still checked out (`eaa10b27`,
version 0.26.0, clean `git submodule status`), node is v18.19.1, and the
typescript grammar pin had not moved since 2025-06-09.

What moved was the grammar underneath it. `grammars/tree-sitter-javascript` was
bumped on **2026-08-31** to a 2025-09-01 upstream that added a first-class
Explicit Resource Management rule:

    grammars/tree-sitter-javascript/grammar.js:335
        using_declaration: $ => seq('using', ...)

The typescript overlay is pinned to 2025-01-30 and approximated the same
construct its own older way, from before javascript had the rule:

    grammars/tree-sitter-typescript/common/define-grammar.js:199
        assignment_expression: $ => prec.right('assign', seq(
            optional('using'), ...

TypeScript also allows `!` (definite assignment) on a `variable_declarator`, so
`using x!` had two parses and the generator refused.

**There was nothing to bump to.** `75b3874` IS upstream HEAD for
tree-sitter-typescript: its last commit is 2025-01-30, its last release 0.23.2
(2024-11-11), and upstream's own overlay still reads `optional('using')`. The
grammar is unmaintained while its dependency moved on.

## Why a `conflicts:` entry was the wrong fix

The first attempt added the entry the CLI itself suggests, which the overlay's
`conflicts: ($, previous) => previous.concat([...])` accepts cleanly:

    [$.variable_declarator, $.primary_expression]

It resolved that conflict and exposed the next one:

    'using'  identifier  '='  expression  •  ','
      1:  'using' identifier (_initializer '=' expression)
      2:  (assignment_expression 'using' identifier '=' expression)

Those are not two ambiguities. They are one construct with two spellings inside
one grammar, so conflicts entries multiply without end.

## The fix

Delete the overlay's `optional('using')`. It exists only because javascript had
none of its own when it was written; javascript's `using_declaration`
supersedes it.

    typescript   ABI 15 -> 15,  STATE_COUNT 5870 -> 5834,  symbols 359 -> 360
    tsx          ABI 14 -> 15,  STATE_COUNT 5986 -> 5897,  symbols 372 -> 373
    added: using_declaration (both)        removed: nothing (both)

No declaration was orphaned, which is the measurement that made this safe to
take.

## What it changes for users

`.ts` and `.tsx` stop disagreeing with `.js` about what a `using` declaration
is. Measured on `using f = open();`:

    before, 'javascript'   program > using_declaration > using > variable_declarator > identifier > = > call_expression
    before, 'typescript'   program > expression_statement > assignment_expression > using > identifier > = > call_expression
    after,  all three      the javascript shape

Neither parse errored, so nothing signalled the divergence. The consequence was
that `f` was a `DEFINITION_VARIABLE` in `.js` and merely a `NAME_IDENTIFIER` in
`.ts` — `is_definition` false — so a query for definitions found it in one file
type and silently missed it in the other.

`using_declaration` was undeclared for javascript too, so it carried
`PARSER_CONSTRUCT` and no name even there. It is now declared in
`javascript_types.def` beside `lexical_declaration`, which it is a kind of.

## What this says about the pins

The two grammar families with a declared dependency both have the dependency
bumped ahead of the dependent:

    javascript  2026-08-31  ->  typescript/tsx  2025-06-09   (this bug)
    c           2026-08-31  ->  cpp             2025-05-26   (measured clean)

cpp regenerates byte-identically, so the skew is not automatically a defect —
but nothing checks it either. Measured 2026-11-13: **25 of 28 committed parsers
are byte-identical to a fresh regeneration from their own pin**. The exceptions
were typescript and tsx (this bug, now fixed) and yaml (#233). A gate over that
property would have caught this on the day the javascript pin moved, rather
than eleven weeks later through an unrelated PR.
