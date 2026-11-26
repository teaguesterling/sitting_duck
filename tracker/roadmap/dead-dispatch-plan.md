# The dead-dispatch worklist: verdict and ordered plan

Status: **approved by Teague on 2026-11-16**, all three decisions as
recommended. Written 2026-11-09 by Claude (Opus 5.5) from
`docs/briefs/2026-11-08-dead-dispatch-worklist.md`.

Progress:

- Change 1 (kotlin). The first attempt was on `fix/323-kotlin-class-dispatch`
  (parked as `fcd5003`, based on `821af42`). Meanwhile `7e74ca4` on main fixed
  the parent-types half of #323, plus #319 (java) and #320 (typescript).
  The rest (interface / enum class kind, the dead `"type"` literal) was
  ported onto main on 2026-11-23, branch `fix/kotlin-class-kind-dead-dispatch`,
  test `test/sql/bugs/323_kotlin_class_kinds.test`. Merged as #362
  (`7ad56b6`), baseline 68 -> 64.
- Filed: #259 body gained section (c), the strategy-dispatch gap. The C++
  return-type defect is #342.

## Outcome, 2026-11-26 (main `89820bd`)

Written by Claude (Opus 5.5). Every change of the worklist is merged. The
baseline count is the number of `{check` lines in `spec/abi/module-abi.yaml`
at each merge commit (measured with `git show <sha>:spec/abi/module-abi.yaml`).

| change | PR | merge | baseline after |
|---|---|---|---|
| 1 kotlin | #362 | `7ad56b6` | 64 |
| re-measure (docs) | #366 | `b2ed8cc` | 64 |
| 2 typescript | #367 | `ef8f886` | 63 |
| 3 php | #369 | `6fffca0` | 60 |
| 4 sql | #370 | `f3afe24` | 51 |
| 8 css | #371 | `82e0e3d` | 49 |
| 7 r | #372 | `e08dcc3` | 45 |
| deletions, decision 1 (c cpp csharp go java lua python ruby rust) | #373 | `e57f5db` | 32 |
| 6 swift | #374 | `89820bd` | 30 |

The plan expected 21 entries at the end. The 30 on main are those 21 (9
NO-STRATEGY, 5 HIDDEN, 3 DELIBERATE, 2 DEFECT-NEARBY, 2 BLOCKED), plus 9 more:

- python `import_list`. It is NO-STRATEGY, not DEFECT (see the corrections below).
- 8 swift REDUNDANT entries: `access_level_modifier`, `enum_declaration`,
  `generic_type`, `init_expression` (table), `rethrows`, `struct_declaration`,
  `type_inheritance_clause`, `variable_declaration`. The table above gives
  change 6 eight deletions, but #374 removed only its two DEFECT entries. I
  (Claude) have no record of a decision to keep them. They are open.

### Where the plan was wrong

1. **python (change 5) was NO-STRATEGY, not DEFECT.** The `import_list` branch
   (`python_native_extractors.hpp:1373`) can only run for an
   `import_from_statement`, and `python_types.def:260` gives that node the
   native strategy NONE. So no dispatch reaches it, and a fix would declare a
   strategy: a feature, by decision 2. Change 5 did not ship. #373 deleted
   python's two REDUNDANT entries; `import_list` stays in the baseline.
2. **sql `text` had a second site.** The literal was a real defect in two
   places, not one. #370 fixed both.
3. **The r call name came from FIND_CALL_TARGET, not the call table.**
   Correcting `namespace_get` in `function_call_extractor.hpp` did not name
   `base::paste()`. The name comes from FIND_CALL_TARGET in
   `core/language_adapter.cpp`. #372 fixed both places.

### Found after the worklist

Findings 1-3 are measured on build `c4a5dc69bf`, whose tree is the same as
main `89820bd`. Each has a test-first fix on a local branch. None is merged.

1. **typescript: the declared type leaks onto the initializer.** In
   `let x: number = y`, identifier `y` reports `signature_type` 'number'.
   Branch `fix/ts-declarator-type-leak`.
2. **r: named call arguments are not split.** `f(a = 1, 2)` gives parameters
   `[{name: '', type: 'a = 1'}, {name: '', type: 2}]`. Branch
   `fix/r-named-call-arguments`.
3. **php: a property declaration has no name.** `public $a = 1;` gives
   `property_declaration` the name ''. Branch `fix/php-property-name`.
4. **java: two branches cannot be reached. CANDIDATE DELETION, not done.**
   `java_native_extractors.hpp:710` (`type_identifier`) and `:713`
   (`generic_type`) are in the VARIABLE_WITH_TYPE extractor, but
   `java_types.def:230` and `:242` declare the native strategy NONE for both
   types. The gate does not list them, because both types exist in the
   grammar. Measured: in `class A { List<String> xs; Foo f; }` every
   `type_identifier` and `generic_type` row has `signature_type` NULL and
   modifiers `[]`, so neither branch's `type_reference` / `generic` modifier is
   ever set. Deleting them should change no output. That is inferred from this
   probe, not from a suite run.

Also measured on the same build, and not worked: php `const_declaration` and
`const_element` report the name ''; the typescript initializer identifier also
gets the `variable_name` modifier.

**Re-measured on 2026-11-24, on main `7ad56b6` (extension build `7ad56b6716`).**
The baseline is the old 68 minus the 4 kotlin entries, with nothing added.
Every probe gives the same output as on `94af4e6`, except in three places:

- kotlin: fixed by change 1, as expected.
- typescript `rest_parameter`: `...rest` is now in the parameters (#320,
  via `required_parameter`). The dead arm stays on purpose, with a comment at
  `typescript_native_extractors.hpp:112`. DEFECT -> DELIBERATE.
- markdown: `document` now has the name `<inline>`. No entry depends on it.

The classification doc is regenerated from main. The verdict and the tables
below keep the original 68-entry counts as the record of what was approved;
the change table shows what is left.

Classification: `tracker/roadmap/dead-dispatch-classification.md`, generated by
`workspace/dead_dispatch/classify.py` from the 68 baseline entries. Probes:
`workspace/dead_dispatch/probes.sh`, output in `probes.out`. Every probe ran on
extension build `94af4e6588`, which is HEAD `94af4e6` (rebuilt as blq `make:354`).

## Verdict

The list is worth working. 21 of the 68 entries are measured, user-visible
defects. They group by language into 8 fix changes. Another 27 entries are
dead alternatives beside a live branch. Deleting them changes no output. The
last 20 stay in the baseline, each with a reason.

| bucket | entries | action |
|---|---|---|
| DEFECT | 21 | test first, then fix: 8 changes, one per language |
| REDUNDANT | 27 | delete; no output changes |
| DEFECT-NEARBY | 2 | cpp `int* f()` reports `int`. The `*` is in a `pointer_declarator` (probed). Not caused by the literal; new issue |
| HIDDEN | 5 | sets only `default_value` / `is_optional` / `annotations`, which SQL cannot see. Park behind #255 |
| NO-STRATEGY | 9 | the live node has no native strategy in its `.def`. Your decision (below) |
| DELIBERATE | 2 | lua, `lua_adapter.cpp:161` says so |
| BLOCKED | 2 | markdown `link`/`image`: the inline grammar is not compiled in |
| total | 68 | |

Two checks before any deletion verdict, both measured:

- Each extractor header serves exactly one grammar. `native_context_extraction.hpp:383-503`
  binds one `*NativeExtractor<Strategy>` per language. No adapter loads a
  `tsx`, `php_only` or `markdown_inline` grammar.
- Lua `local_function_declaration` has no intent comment, and the probe names
  `local function f` correctly, so it is REDUNDANT, not DELIBERATE.

## The 8 fix changes, in recommended order

Order: open issues first, then by how often a user meets the defect.

| # | language | defect (probe) | entries fixed | also deleted |
|---|---|---|---|---|
| 1 | kotlin | DONE (#362). `class K : B(), C` has no parents (#323); `interface I` and `enum class E` report `class` | 3 | 1 |
| 2 | typescript | identifier `a` in `(a: string)` has type `identifier`, not `string` (re-measured on `7ad56b6`). The `...rest` half was fixed by #320 | 1 | 0 |
| 3 | php | `Foo $a`, `?Foo $n` -> type `''`; `): Foo` -> NULL; `f(name: 2)` -> name `''` | 3 | 0 |
| 4 | sql | `upper(a, 1)` -> `SQL`, no arguments; `AND`/`OR` listed as operands; every literal `[GENERIC]` | 6 | 3 |
| 5 | python | `from a import b, c` -> no imported names | 1 | 2 |
| 6 | swift | closure `{ (a: Int) in a }` -> no parameters; `foo(label: 1)` -> name `''` | 2 | 8 |
| 7 | r | `base::paste()` and `base:::f()` -> name `''`; `,` listed as an argument | 3 | 1 |
| 8 | css | `--main: red` -> `PROPERTY`, never `CUSTOM_PROPERTY` | 1 | 1 |

Each change follows the brief: write the failing test, confirm it fails where
predicted, fix the dispatch, then remove only that language's entries and
regenerate the baseline.

Deletion-only languages (11 entries): c 1, cpp 2, csharp 2, go 1, java 1, lua 1,
ruby 2, rust 1. How to ship them is decision 1.

After all of this, the baseline holds 21 entries (20 in the original count,
plus typescript `rest_parameter`, now DELIBERATE). Their reasons live in the
classification doc, not in the baseline: `--emit-baseline`
(`verify_native_dispatch.py:744-746`) rewrites every line from scratch, so a
comment added there is lost at the next regeneration. Keeping a `reason:` field
would need a small change to the checker. That is not in this plan.

## Decisions for Teague

1. **An exception to "one language per change", for deletions only.** The 11
   deletion-only entries span 8 languages. Recommended: one PR with one commit
   per language. Bisect stays per language, and it costs one review. Without
   the exception, that is 8 separate PRs.
2. **NO-STRATEGY (9 entries).** TypeScript `namespace`, `module`, class fields
   and literal index keys, and SQL `select`/`insert`/`update`/`delete`.
   Recommended: leave them baselined with a comment. Declaring a strategy
   changes output that is NULL today, so it is a feature, not a fix.
3. **What to file.** The strategy gap belongs in open issue #259 as a third
   assertion, so recommended: edit #259, no new issue. The C++ return type is
   new; recommended: file it. Draft texts are below.

Default if no answer: start change 1 (kotlin) on a branch, test first, and do
not file anything.

## Draft addition to #259: native strategies the dispatch cannot reach

> `native_context_extraction.hpp:516-546` dispatches 11 of the 17
> `NativeExtractionStrategy` enumerators. `kotlin_native_extractors.hpp:798`
> specializes `GENERIC_FUNCTION`, which is not one of the 11. No `.def` declares
> `GENERIC_FUNCTION` today, so no output is wrong yet. A `.def` that declares it
> would get an empty result, with no error: the #220 shape. Fix: dispatch every
> enumerator, or make an undispatched one a build error.

## Draft issue: C++ pointer and reference return types are dropped

> `int* f() {}` and `int& g() {}` report `signature_type = 'int'`. The `*` and
> `&` sit in a `pointer_declarator` / `reference_declarator` that wraps the
> `function_declarator`, so the type nodes the extractor reads never carry
> them. The dead literals `pointer_type` and `reference_type`
> (`cpp_native_extractors.hpp:528`) are not the cause. Probe on build
> `94af4e6588`. Related: #317 (parameter types), #318 (declarator walk).

## Found by the probes, not caused by a dead literal

Out of scope for this list. Each ran on build `94af4e6588`.

- C: `inline static int h(void)` -> modifiers `[]`. C++ gives `[inline, static]`. New.
- C: `(void)` gives a parameter named `param` with type `void`. See #317, which reports C's `param` names.
- Go: `func f() (int, bool)` lists the two results as parameters. New.
- C++: `virtual void v()` has no `virtual` modifier. New.
- `parameters` exposes only name and type (#255), which is why HIDDEN exists.
