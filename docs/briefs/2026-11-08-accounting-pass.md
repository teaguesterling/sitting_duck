# Brief — the ast_definitions() accounting pass (#288 with #276)

**Status: approved in principle, NOT specced.** The release order that reached
this point put "#288 with #276, together" as the next step, and Teague approved
that order. The *approach* is open. Run `superpowers:brainstorming` first, then
get a plan approved before writing code — this repo's CLAUDE.md requires an
approved plan, and the two issues interact, so a plan that treats them
separately is probably wrong.

---

## Situation

`sitting_duck` is a DuckDB extension that parses source code with tree-sitter
and exposes the syntax tree as SQL tables. Its main user-facing surfaces are
`read_ast(path)` / `parse_ast(source, language)`, which return one row per AST
node, and a set of SQL macros over them — of which `ast_definitions()` is the
one that matters here. `ast_definitions()` is meant to answer "what does this
file define": every function, class, variable, parameter and field, once each.

Release v1.18.3 has just finished fixing nine issues that all shared one
symptom: a name a reader can see in the source either did not reach the `name`
column or reached it wrong. Those are all landed and CI-confirmed on branch
`fix/phase2-name-binding` (pull request #312), with the last commit `694ebef`
measuring 10028 assertions in 187 test cases in CI.

## Task

`ast_definitions()` is wrong in two opposite directions at once, and both are
filed with measurements:

- **#288 — it MISSES names.** The macro filters on a node's *semantic type*.
  Several kinds of name are bound by a per-adapter "bare-name hook" that sets
  only a *name-role flag* and leaves the semantic type as `NAME_IDENTIFIER`. So
  those names are flagged as definitions and are still absent from the macro's
  output. This affects already-merged work: bare parameters, for-loop targets,
  catch/rescue variables, match captures and destructuring targets.
- **#276 — it DUPLICATES names.** For a variable or field declaration, the
  container node and the inner declarator node both carry the
  `NAME_DEFINITION` flag, so the same name comes back two or three times.

They interact: the natural fix for one changes the counts the other is measured
against, which is why they were scheduled together rather than in sequence.

## Action

Nothing has been implemented. This brief exists to hand the work over with its
measurements intact so they are not rediscovered.

## Result

The nine name-binding fixes are in and green. `ast_definitions()` still
under-reports the names bound by the hook and over-reports the ones with a
wrapper node. Nothing downstream has been corrected for either, so any count
anywhere in the test suite that comes from `ast_definitions()` is currently
measured against the buggy behaviour.

---

## Your prompt

Read the two issues (`gh issue view 288`, `gh issue view 276`) and the release
record at `tracker/roadmap/v1.18.3-name-binding.md`. Then brainstorm and
propose a plan for fixing both together, and get it approved before you write
code.

Three extra items were deliberately queued into this step while v1.18.3 was
landing. Fold them into your plan or argue them out of it, but do not silently
drop them:

1. **#186 — every root node is unnamed, and Teague has decided what the name
   should be.** All 27 languages' root nodes (`module` in python, `program` in
   ten languages, `source_file` in six, `translation_unit`, `document`,
   `chunk`, `stylesheet`, `config_file`, `compilation_unit`, `stream`) report
   `name = ''`. Zero are NULL — that was measured, so the issue's "`''` vs
   NULL" question is already settled. **The decision: name a root node by its
   file path, exactly as given.** Teague chose this over a basename, over NULL,
   and over a dotted import path, because a basename would make many files
   indistinguishable and `:is-scope(module#utils)` would silently match two
   different `utils.py` files.

   It is cheap: the name is assigned at `src/include/unified_ast_backend_impl.hpp:288`,
   where `file_path` is already in scope (it is used at :114, :216 and :519), so
   this needs no change to any of the 27 `ExtractNodeName` overrides and no
   `.def` change. It is here rather than done already because it gives **411
   root rows in `test/data/**` a name they did not have** — one per file — which
   is content churn on every `read_ast` in the suite, and that is what this step
   is for.

   One sub-point left open, flagged not decided: `parse_ast` has no path and
   already carries the sentinel `file_path = '<inline>'`, so taking the path
   literally names every inline parse's root `<inline>`. Teague was told; he has
   not objected.

2. **Should a C++ function present one function row or two?** As of #315,
   `function_declarator` delivers its parameters, because the node declares the
   `FUNCTION_WITH_PARAMS` strategy and was returning an empty list. But
   `function_declarator` is a `DEFINITION_FUNCTION` *child* of
   `function_definition`, so every C++ function now yields two rows carrying the
   same name, the same return type and the same parameter list. Measured on
   `test/data/cpp/simple.cpp`:

   ```
   function_definition   add    np=1  st=void
   function_declarator   add    np=1  st=void
   ```

   The duplicate row is not new — those rows already shared the name and the
   return type, and `parameters` was the one field left empty. Two shipped SQL
   macros read `parameters` and select rows by *semantic type*: an
   `[arity=N]` attribute selector at `src/sql_macros/css_selectors.sql:1791`,
   and a block-heading builder at `src/sql_macros/duck_blocks.sql:186`. Measured:
   **no test exercises either surface on C++**, so this is currently a
   disclosure rather than a regression. It is the same question #276 asks, in
   C++'s vocabulary.

3. **Should `argument_list` declare a parameter strategy at all?**
   `languages/cpp/cpp_types.def:779` declares `FUNCTION_WITH_PARAMS` on
   `argument_list` and it returns an empty list, deliberately: a call's
   arguments are not parameters, and they already reach SQL through a different
   extractor. Either the declaration should go, or it should mean something.
   This is a taxonomy question, not a bug.

## Already measured — do not re-derive

From #288, on a build of branch `fix/274-destructuring-targets`:

```python
def f(x, y=1):
    return x
for zz in items:
    pass
```

| node | name-role flag | `semantic_type` | in `ast_definitions()` |
|---|---|---|---|
| `identifier` x | DEFINITION | `NAME_IDENTIFIER` | **no** |
| `default_parameter` y | DEFINITION | `DEFINITION_VARIABLE` | yes |
| `identifier` zz | DEFINITION | `NAME_IDENTIFIER` | **no** |

`ast_definitions()` returns `f | function` and `y | variable` only. The pattern:
a name with its own wrapper node is visible, because the wrapper carries a
`DEFINITION_*` semantic type from the `.def` file; a bare name is not.

From #276, on a build of branch `feat/170-macro-descriptions`:

| language | source | rows carrying NAME_DEFINITION |
|---|---|---|
| JS | `const f = () => {};` | `lexical_declaration` f, `variable_declarator` f, `arrow_function` f |
| JS | `let a = 1, b = 2;` | `lexical_declaration` **a**, `variable_declarator` a, `variable_declarator` b |
| TS | `const x: number = 1;` | `lexical_declaration` x, `variable_declarator` x |
| Java | `private int size;` | `field_declaration` size, `variable_declarator` size |
| C# | `private int size;` | `field_declaration` `<NULL>`, `variable_declaration` size |

Note the JS multi-declarator row: the container reports only the **first** name,
so deduplicating by name alone would lose `b`.

Measured during v1.18.3, still true:

- `ast_definitions('test/data/cpp/simple.cpp')` returns **51** rows.
- `test/data/cpp/*.cpp` has 12 `parameter_declaration` rows, all with non-empty
  names as of #318.
- `test/data/**` has **411** root nodes, one per file.

## Limits agreed up front

- **Change implementation or tests, never both in one step**, unless you say in
  advance what each change is for. The safe order used throughout v1.18.3:
  write the expectation first, confirm it fails where you predicted, commit it
  red, then fix.
- **No new "dummy" behaviour**, and no large refactors — both are explicit
  rules in this repo's CLAUDE.md.
- Run builds and tests through the `blq` tool rather than bare `make`/`unittest`
  where you can, and route git and GitHub work through `jetsam`. Both are MCP
  tool sets already configured.
- **Two traps that cost real time in v1.18.3, both worth inheriting:**
  - A blq call that goes to the background sometimes reports *the previous run's
    output body* with its own metadata. Seven instances. The test runner's own
    `Filters:` line, printed inside the output, is the only thing that catches
    it. When they disagree, read the result from blq's store instead of
    re-running: join `invocations` to `outputs` on the invocation UUID, which is
    the one stable identifier.
  - `test/sql/bugs/*` cannot finish inside one blq call — 46 files, and
    `issue_243` alone takes 1105 s. Run it in chunks. A `[0-9]*` character class
    does not work; the runner only understands `*`, and a non-matching pattern
    exits 0 having run nothing.

## The first question to settle

**Does `ast_definitions()` key on the name-role flag, on the semantic type, or
on both?** #288 is fixable by changing what the macro filters on, or by changing
what the hook sets, and #276 is fixable by deciding which of the container and
the declarator is canonical. Those two choices are not independent: if the macro
moves to the flag, then #276's duplicates are whatever carries the flag, and the
`.def` files decide the answer rather than the macro. Settle that before
designing either fix.

## Links

- Issues: `gh issue view 288`, `gh issue view 276`, `gh issue view 186`
- The release record, including every measurement from the nine fixes:
  `tracker/roadmap/v1.18.3-name-binding.md`
- Branch `fix/phase2-name-binding`, pull request #312, tip `694ebef`
- Originating session: https://claude.ai/code/session_01FVcSpAnvP7udQhjpSvSPTE
