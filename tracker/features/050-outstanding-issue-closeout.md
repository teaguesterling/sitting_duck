# 050 — Outstanding issue close-out program

**Status:** proposed
**Created:** 2026-10-15

**Scope.** 35 issues were open when v1.16.0 shipped. Triage on 2026-10-15
re-measured 19 of them, closed 3 (#152, #165, #246) and filed 4 (#258, #259,
#260, #261); `gh issue list` reports **35** open as of writing. #220 closes
when PRs #254/#256/#257 land, making it 34.

## Why this is a program and not a sprint

Thirty-five issues is not a list to be worked top to bottom. They fall into
seven families, one of which is a third of the backlog. More importantly,
**a significant fraction were no longer true as written**, so the first cost of
"finishing the outstanding issues" is not implementation — it is knowing which
ones are still real.

### The method, and the evidence for it

**Re-measure before working.** #139 was filed with eleven failing cases at
`7322cd6`. Re-running every one of its queries on a current build:

| | |
|---|---|
| already fixed | js `.import`, c `.import`, cpp `.import`, py `.str`, js `.str`, java `.catch`, bash `.call` — **7**, plus cpp `.import` partially |
| still open | cpp `.fn`, go `.mod`, py `.self` — **3** |

Nobody fixed #139. `IS_CONSTITUENT` was applied to those node types for other
reasons and the class filter already excludes constituents, so eight of eleven
cases closed as a side effect. Starting this program by implementing #139 as
written would have re-done work already done.

The inverse holds too: #147, #148, #186, #149 and #166 reproduce *exactly* as
filed. Re-measurement is not a way of talking the backlog down — it tells you
which half is real.

## Triage yield, 2026-10-15

Nineteen issues re-measured against a current build in one sitting:

| outcome | issues |
|---|---|
| **closed** — fixed or moot, verified | #152, #165, #246 |
| **narrowed** | #139 (8 of 11 cases fixed), #203 (2 of 5 gaps closed), #150 (example runs; residue split out) |
| **widened** | #202 (23 live tautologies across 12 files, not 6 in 1) |
| **confirmed, cause newly identified** | #147, #148, #166, #131, #158 |
| **confirmed as filed** | #186, #149, #136, #140, #223, #255, #250 |
| **filed new, from what the measuring turned up** | #258, #259, #260, #261 |

Three closed, three shrank, one grew, twelve stood — and five of those twelve
now have a verified mechanism instead of a reported symptom.

**Two closures were invisible without testing behaviour rather than code.**
#152's mechanism (code 241 ≠ the literal) is still real, but the comparison it
broke no longer exists anywhere in the macros, so the symptom is gone. #165's
repro still returns 0 — but 0 became the *correct* answer once `.import`
stopped matching constituents. **Both would read as "still broken" to anyone
re-running the issue's own table.** That is the failure mode this program
exists to avoid, and it cuts the other way too: an unchanged observable can
hide a changed meaning.

## The one cross-cutting theme

Four issues across two families are the same question — **which node owns a
construct's identity?** The node carrying the construct and the node carrying
its name are different nodes, and different parts of the system pick different
ones:

| issue | construct node | where the name actually is |
|---|---|---|
| #139 (cpp) | `function_definition` | `function_declarator` also matches `.fn` — 12 and 12 |
| #139 (go) | `source_file` | `package_clause` also matches `.mod` — 4 and 4 |
| #158 | `function_definition` under `template_declaration` | `template_function` use sites also claim `DEFINITION_FUNCTION` |
| #140 (go) | `struct_type` / `interface_type` — **0 of 4 named** | `type_spec` — **4 of 4 named** |
| #186 | `module` — **19 of 19 unnamed** | nowhere |

`IS_CONSTITUENT` is the existing lever for the first three. #140 and #186 need
the opposite move — a name where there is none. **Settling one rule about which
node owns a construct's name, and applying it in both directions, is the real
leverage here.** Four independent patches is how you get four inconsistent
answers.

Note what this theme is *not*: it is not family A's root cause. My first pass
claimed #139/#147/#148/#186/#146/#166 shared two root causes; reading the
implementations showed they do not (see family A below). The shared-root reading
survived one plausible paragraph and died on first contact with the source.

## Families, with provenance for each status

`measured` = a query was run against a current build on 2026-10-15 and the
result recorded. `reported` = the issue's own measurement, not re-checked.

### A — Selector semantics & scope (12 listed, 2 now closed)

The most user-visible family: these are the queries the docs teach.

| # | status | one line |
|---|---|---|
| 147 | **measured** — open, **mechanism found** | `:not(:is-referenced)` returns 0 rows because a definition's own name token is a separate node flagged `is_name_reference`, and the predicate excludes only `ref.node_id != a.node_id` (`css_selectors.sql:1865`) |
| 148 | **measured** — open, **root cause found** | `:exported` returns 7 incl. methods and nested `inner`. It is a one-line flag read (`:1877`); the defect is `IS_EXPORTED` at `unified_ast_backend_impl.hpp:325`, which tests only `IsPublicNode` — the leading-underscore convention — with **no scope-level component at all** |
| 166 | **measured** — open, **cause verified at source** | `.var:in-scope(.class#users)` = 0 vs ground truth 2; `.class:is-scope` = 0 because `IS_SCOPE` occurs **0 times** in `sql_adapter.cpp` and all four container types declare flags `0` |
| 139 | **measured** — 3 of 11 cases left | cpp `.fn` also matches `function_declarator`; go `.mod` also `package_clause`; py `.self` matches 0 of 85 |
| 186 | **measured** — open | all 19 `module` nodes have `name = ''`, so `:is-scope(module#x)` can never match |
| 149 | **measured** — open | `[receiver=db]` matches 4, `[receiver="self.db"]` matches 0: the chain collapses to its last segment |
| 258 | **measured** — open, filed today | 12 `error()`-raising guards in one unordered `AND`, so which message surfaces is planner-dependent; guard-message tests are only incidentally green |
| 150 | **measured** — mostly fixed | the capstone example now runs (2 rows); the unsupported `:has([attr])` term was removed from the tutorial. Residue — `peek` comes back NULL — split out as #261. Keep open only for the feature (attribute filters inside `:has`) |
| 191 | reported | `:calls()` / `:called-by()` silently discard a selector or empty argument |
| 146 | reported | `:calls(name)` is containment, not scope-aware as documented |
| ~~152~~ | **measured** — ✅ **CLOSED** | `:called-by(inner)` → `helper2`; `:calls(helper2)` → the `lambda` node. Both directions attribute to the immediate scope and recognise lambdas |
| ~~165~~ | **measured** — ✅ **CLOSED** | `.class:has(.fn)` = `.class:has(function_definition)` = 18; `.fn:has(.call)` = `.fn:has(call)` = 43 |

**This family has no single root.** #147 is a macro defect, #148 is a C++ flag
computation, #166 is an unset flag for one language, #139's remainder is node
identity. Four distinct fixes in four different places.

### B — Classification correctness (7)

| # | status | one line |
|---|---|---|
| 131 | **measured** — open, **fix is larger than it looks** | all 5 `@` are decorators (parent `decorator`), 12 of 13 `in` are loop syntax (`for_statement` 8, `for_in_clause` 4), 1 is a real comparison. **Not** the #231 one-line pattern: python reuses one node type across roles, so this needs parent-aware classification, which `.def` cannot express |
| 158 | **measured** — open, **no fixture can catch it** | `make_unique`, `static_cast`, `twice<int>`, `b.get<int>()` all `DEFINITION_FUNCTION`. And `test/data/cpp/**` contains **0** `template_function`/`template_method` nodes, so no existing test could regress |
| 250 | **measured** | dart declares only constructor forms (`const_object_expression`, `constructor_invocation`, `new_expression`); graphql only `fragment_spread`. Neither classifies an ordinary call |
| 136 | **measured** | `DEFINITION_TYPE` appears **0×** in `taxonomy.yaml`; `is_type_definition(flags)` matches **0 of 3474** nodes |
| 140 | **measured** | bash `command`: 95 nodes, **0 named**. go `struct_type`/`interface_type`: 0 named; `type_spec`: 4 of 4. Note bash `.call` now matches all 95 — so they are selectable by type but unreachable by name |
| 200 | reported | the anonymous-token-with-variable-text class (ruby `'`, lua `--[[`) is not derivable |
| 64 | reported | `NAME_ROLE` audit across all `.def` files |

### C — API surface & schema (5)

| # | status | one line |
|---|---|---|
| 260 | **measured**, filed today | `ast_type_map()` returns `semantic_type` as VARCHAR, `read_ast()` as `SEMANTIC_TYPE`: `= 'DEFINITION_FUNCTION'` is string equality in one and enum equality in the other, and refinements — **711 of 3659 declarations** — are unrecoverable |
| 261 | **measured**, filed today | `ast_select()` always returns NULL in `peek` (0 of 20, 0 of 22, 0 of 25 across py/js/cpp) while `read_ast()` populates it for the same nodes (20 of 20). The `[peek*=]` filter works, so only the projection is lost |
| 255 | **measured** | `parameters` is `STRUCT(name VARCHAR, type VARCHAR)[]` — 2 of `ParameterInfo`'s 6 fields; `is_variadic` and `default` unreachable from SQL |
| 223 | **measured** | `ast_type_map()` has no `named` column — confirmed against its 9 columns |
| 176 | reported | `duckdb_functions()` drops a TABLE function's parameter names on the v2.0 line |
| 117 | reported | make `ast_select_from` generic: dynamic attribute allowlist + published `SEMANTIC_TYPE` |

#260, #261 and #223 are all one question — **does the published schema tell the
truth about what the engine knows?** In all three the engine has the value and
the surface drops it. Worth fixing as a set.

### D — Performance & scale (3)

| # | status | one line |
|---|---|---|
| 96 | reported, mechanism credible | suite enumerates every file twice via the `/home` ↔ `/srv` mount mirror — every local run is 2× |
| 160 | reported | `ast_select_from` spends ~6 s planning per call regardless of table size |
| 164 | reported | call-graph pseudo-classes exhaust memory on a 78k-node table and kill the process |

### E — Hygiene & dead code (5 listed, 1 now closed)

| # | status | one line |
|---|---|---|
| 202 | **measured** — wider than filed | **23** live `COUNT(*) >= 0` assertions across **12** files, not 6 in 1 (raw 32, less 3 comment mentions and 6 in `.disabled` files). Six sit in per-language support files, where "does this language work" is supposed to be answered |
| 203 | **measured** — 2 of 5 now covered | `ast_callers`/`ast_callees` have full row-set assertions in `callgraph_macros.test`; `ast_pattern_list`, `parse_ast_hierarchical`, `read_ast_hierarchical_new` still have **0** test mentions. The last is suspected residue — decide delete-vs-test |
| 226 | reported | 5 components the build never compiles (1331 lines) |
| 233 | reported | yaml's grammar cannot be regenerated; `grammar.js` post-processes `grammar()` output |
| ~~246~~ | **measured** — ✅ **CLOSED**, superseded by #226 | compiled in **none** of the four build configurations (506/456/498/507 objects each) and neither symbol in the binary, so the `ifstream` is unreachable |

### F — New features (1)

| # | one line |
|---|---|
| 159 | `:templated` pseudo-class for C++ templates — note #158 must be fixed first, or it would select use sites |

### G — Architectural, explicitly NOT in this program (2)

| # | why deferred |
|---|---|
| 87 | grammar packs as extensions → `sitting_duckling`. A build-system and distribution change, not an issue fix. |
| 71 | extract SQL macros to Fledgling. Cross-repo; changes what this repo *is*. |

Listing these as deferred is the point: a close-out program that implicitly
promises them never ends.

## Proposed order

1. **#96 first — it is an accelerator, not a fix.** Everything below is verified
   by running the suite, and the suite currently runs twice. Halving it pays for
   itself inside this program.
2. **Build the derivation gate (#259).** #220 was three silent failures — a
   missing traits specialization, dispatch on node types the grammar never
   emits, and a dispatch omitting the one node type that declares the strategy —
   and **none produced a compile error.** Asserting (a) every `.def`-declared
   function-shaped strategy has a traits specialization and (b) every node type
   an extractor dispatches on exists in that grammar's symbol table would have
   caught all three. (b) is the interesting half: it generalises to every
   extractor in the repo, and kotlin's dead branches were probably dead from the
   day they were written.
3. **The node-identity rule** (#139 remainder, #158, #140, #186) — the one place
   with real leverage, and it spans families A and B.
4. **Family E hygiene.** #202 should be re-scoped to its measured 23 first;
   fixing only the 6 it names leaves 17 that pass for every possible result.
   #203's residue question (delete vs test) is a decision, not work.
5. **Family C as a set** (#260, #261, #223, then #255) — one theme, and #260 is
   a breaking column-type change that wants a release note.
6. **Family A's remaining four** (#147, #148, #166, #149), each a separate fix.
7. **Family B's remainder**, then **D**. **F** last. **G** not in this program.

## Debts this triage created — BOTH RETRACTED, they did not exist

This section asserted that #152 and #165 were closed on measurements no test
repeated. **Both claims were wrong**, and the retraction is kept rather than
deleted because the error is more instructive than the original note.

- **#152 was already pinned**, and had been for a month.
  `test/sql/ast_select_pseudo_classes.test:329-358` asserts
  `:called-by(outer)` = 1, `:called-by(inner)` = 1 and
  `#outer:calls(helper2)` = 0 — the exact lambda-attribution results I closed
  the issue on. They landed 2026-09-16 in `e9b8456`,
  *"bound :called-by/:calls via scope.function; lambda-direct (#164, #152)"*.
  So #152 was fixed **and** pinned in September; it had simply not been closed.
- **#165 was already pinned too**, by `:has()` assertions in
  `css_selectors_multilang.test` — 63 uses of `:has(`, of which 16 use the
  class form `:has(.class)` — including the class-vs-bare-type equivalence at
  `:1027` — *"`type:has(.class)` … Must return the same set as
  `.function:has(.call)` above"* — asserted **by name**, not merely by count.

  (Counts corrected before merge. This bullet first said "45 assertions", a
  figure I never measured: `grep -cE ':has\(' ` gives 63 and
  `grep -cE ':has\(\.'` gives 16. Nothing supports 45. A retraction carrying
  its own invented number is the exact failure it was written about, which is
  why the correction stays visible instead of being edited away.)

**How I got it wrong:** I searched for tests naming the *issues* and found none,
and concluded the *behaviour* was unpinned. Those are different questions. The
coverage existed under the names of the selectors, which is where it belongs —
a regression test should be named for the behaviour it protects, not for the
ticket that prompted it. My search was shaped by the ticket, so it missed it.

The real lesson is the inverse of the one originally recorded here: before
declaring a coverage gap, grep for the *behaviour*, not the issue number.

## Caveats

- The three remaining #139 cases *look* like the one-line `IS_CONSTITUENT` fix
  the other eight got, but `function_declarator` and `package_clause` both carry
  name-extraction duty (`FIND_IN_DECLARATOR`), so the flag must be checked
  against name binding, `:has()` and `:in-scope()` before that is assumed.
- py `.self` is **not** a constituent problem — nothing is classified as a self
  reference at all, so it needs a classification rule.
- #148's fix belongs at the **one** call site, not in the 30 adapter files that
  implement `IsPublicNode` (29 live; one is dead code per #226). "Is this at
  module scope" is structural, not a naming convention — and `TSNode` already
  supports the ancestor walk, so no signature change is needed.
- Seven issues above still rest on their own reported measurements. Those
  statuses are the issues' claims, not this document's.
- `tracker/PRIORITIES.md` is stale (last updated 2026-04-26; still says YAML is
  unsupported, which v1.16.0 contradicts). It needs a pass, but not here.
