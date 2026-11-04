# 042 — Declared native strategies that deliver nothing, or deliver wrong

**Status:** open. Scoped to **v1.18.0** by Teague on 2026-10-30; deliberately NOT
hotfixed as v1.17.1, so the class gets fixed rather than the four instances.

Found while writing the module ABI spec (`044` M1 deliverable 1), then widened by
three pilot reviews (python, zig, lua — reports in `workspace/reviews/`).
Published writeup: <https://claude.ai/artifact/EHdRUuk79AEyX3CQk7GShg>.

## The mechanism

Native context extraction is declared by C++ **template specialization**. A
missing specialization is not a compile error, a link error or a warning:
`ExtractNativeContextTemplated` falls back to an empty default, the `.def` still
declares the strategy, the build still succeeds, and the column comes back blank.
That is #220, which shipped in kotlin, lua and sql.

The same defect has five levels, each with a finer unit of judgment. Two are now
gated; three are not.

| level | shape | scale | gated |
|---|---|---|---|
| L1 | no `NativeExtractionTraits` mapping at all | 8 languages (4 compiled in) | `verify_module_abi.py` check 4 |
| L2 | mapping exists, no specialization for the declared strategy | 8 pairs, all compiled in | `verify_module_abi.py` check 6 |
| L3 | specialization exists, but not for every **declaring node type** | typescript declares `FUNCTION_WITH_PARAMS` on 11 node types, `VARIABLE_WITH_TYPE` on 17; lua declares `FUNCTION_CALL` on `function_call` **and** `method_index_expression` | **no** |
| L4 | specialization **nothing declares** | python ships 3 (`ASYNC_FUNCTION`, `FUNCTION_WITH_DECORATORS`, `IMPORT_STATEMENT` — the last is a `reserved` strategy, so the dispatcher has no case either) | **no** |
| L5 | declaration the strategy structurally **cannot serve** | python declares `VARIABLE_WITH_TYPE` on `block`, `return_statement`, `expression_statement`, `identifier`, `attribute`, `binary_operator` | **no** |

L1 and L2 are recorded as baselines in `spec/abi/module-abi.yaml`
(`native_extraction.unmapped`, `native_extraction.unimplemented`), so the gate is
green today and a ninth case fails it.

## The four that are WRONG, not merely empty

A blank column is visible. These are populated and wrong, so they satisfy every
non-emptiness check in the repository, the conformance kit included. All measured
on `build/release/duckdb` with the built extension loaded.

### 1. python never reads a type annotation

`python_types.def:89` declares `VARIABLE_WITH_TYPE` on `assignment`. The
extractor's `annotated_assignment` branch is **dead** — that node has zero
occurrences in tree-sitter-python's `ts_symbol_names[]` — so every annotated
assignment falls through to a helper that infers from the value after `=`.

```
source            signature_type     should be
d: float = 1      int                float
x: str = None     None               str
b: list = []      list               list   (right by coincidence)
y: int            assignment         int    <- the NODE TYPE string, in the type column
z = 3             int                int    (no annotation; correct)
```

### 2. swift returns one blob where arguments belong

`LANGUAGE_FUNCTION_CALL_TYPES["swift"]` names its argument container
`argument_list`, which is **not in the swift grammar's symbol table**. The
grammar nests `call_expression > call_suffix > value_arguments`. Finding no
container, the extractor takes the whole suffix as a single argument.

```
swift   add(a: 1, b: 2)  ->  [{'name': '', 'type': '(a: 1, b: 2)'}]
python  foo(1, 2)        ->  [{'name': '', 'type': 1}, {'name': '', 'type': 2}]   <- the oracle
```

### 3. lua leaks a modifier into nested functions

`ExtractModifiers`' four-deep ancestor walk gives an anonymous function merely
*nested* inside a `local function` its parent's visibility. The #220 test
contains only the direct `local anon = function()` form, which the walk handles
correctly — so the test passes and the bug ships.

```
local function outer(a)
  return function(b) return a + b end
end

function_declaration  outer   modifiers=[local]   correct
function_definition   (anon)  modifiers=[local]   WRONG
```

### 4. lua never consults `local` for visibility

`IsPublicNode` dispatches on three node types absent from tree-sitter-lua.

```
g_global = 1              is_exported = false    wrong
local l_local = 2         is_exported = TRUE     wrong
function g_func()         is_exported = true     correct
local function l_func()   is_exported = TRUE     wrong
```

Note this is **not** the clean inversion the lua review summarised: global
*functions* are exported correctly. The defect is that `local` is not consulted
on declaration nodes at all. Three of four cases wrong.

## Why no existing gate caught any of them

| check | in a workflow | limit |
|---|---|---|
| `verify_module_abi.py` | yes | unit is `(language, strategy)`; L3–L5 invisible |
| `check_def_type_tables.py` | yes | formatting, and duplicate raw types |
| `generate_taxonomy.py --check` | yes | taxonomy table sync only |
| `verify_native_dispatch.py` | **yes, since step 2** | was: matched `strcmp(<var with "type">, "lit")` only, so python's `==` idiom left 95 of 132 literals unexamined; globbed `*_native_extractors.hpp`, so the shared tables and the adapters were unread; printed **no line** for a language with no extractor header. Now: 1,374 literals in 27 languages, 191 in the shared table, 70 baselined. Remaining limit: 103 comparison sites whose operand is source text, which it names by operand |
| `conformance_kit.sh` | **no** | `NATIVE-DECL` covers 5 of 10 declarable strategies and aggregates per language, so it prints `PASS: every declared capability was delivered` for python and zig while a declared capability of each delivers nothing. **And nothing proves it can fail** — see below |
| the three `audit_*.py` | **no** | — |

Only an **oracle language** caught the swift defect. That is now rubric item 1.7.

### `NATIVE-DECL` is the one kit check with no negative control

Measured 2026-11-03, from `scripts/conformance_kit.sh` and from a clean run.
The kit's step 5 says what it intends:

> Every check above reports its happy answer both when it is right AND when it
> compared nothing. These plant a fault and require each check to notice.

It plants **8** faults. `control_note` is called for `PARSE`, `CALL-NAMED`,
`NATIVE-ABST`, `RAW-JOIN`, `NO-EMPTY`, `DECL-UNIQUE`, `DECL-UNIQ-FLAGS` and
`DECL-RUNTIME`. The matrix also has 8 columns, so the two counts match and the
gap reads as closed — but the sets differ by one in each direction:

| | in the matrix | has a control |
|---|---|---|
| `NATIVE-DECL` | yes | **no** |
| `DECL-UNIQ-FLAGS` | no (folded into `DECL-UNIQUE`) | yes |

There is no `# --- NATIVE-DECL` block and no documented exception, so this is an
omission rather than a decision. `044:72`'s "all 9 negative controls" is
corrected there.

**Why it matters for step 3.** `NATIVE-DECL` is exactly the check step 3 makes
finer, and it is the check this document already blames for printing `PASS:
every declared capability was delivered` over python and zig. Making an
unproven check stricter produces a finding count nobody can calibrate: if the
check cannot detect a planted fault today, a quiet new result after step 3 is
not evidence of anything. So step 3 writes the control FIRST, against the
current per-language check, and only then changes the unit of judgment.

This is also the precondition for the baselining decision below. A baseline is
only as good as the gate that fills it.

## Also found: 11 dead literals in the shared extractor

`function_call_extractor.hpp` holds per-language node-name tables for 15
languages, 191 literals in all. It was in no gate's glob; it is now check B of
`verify_native_dispatch.py`.

```
php     argument_name            the grammar's rule is hidden: _argument_name
r       namespace_get, namespace_get_internal      vs namespace_operator, ::, :::
r       ",", ";"                 tree-sitter-r has no anon_sym_COMMA at all;
                                 its comma node is named `comma`, so the
                                 argument-punctuation skip never fires and
                                 commas come back as operands
ruby    method_call, chained_call                  vs call
rust    method_call_expression                     vs call_expression
swift   init_expression, argument_list, labeled_argument
                                 vs value_arguments, value_argument,
                                 value_argument_label -- the same defect as
                                 swift's one-blob arguments
```

**This supersedes the earlier figure of 9 dead literals plus 5 benign dead
punctuation.** That probe was wrong twice, and both faults were in its own
input, not in the code it judged:

  - It decoded only `\"` and `\\` from `ts_symbol_names[]`. Measured across
    every generated parser, the table also carries `\?` (63 names), `\\` (8),
    `\n` (3) and `\0` (1). So tree-sitter-dart's `?.`, written `"\?."` in
    parser.c, read as absent when the grammar emits it.
  - Its map-end test was `startswith("};")`, but the map closes with an
    indented `    }}};`. It therefore ran past the end of the table and blamed
    seven of the header's own generic helpers on dart.

**One earlier claim does not reproduce:** go and rust do *not* list `", "` with
a trailing space. No literal in the map has leading or trailing whitespace,
checked exactly rather than by grep.

The file still contains **zero** depth bounds and **one** `ts_node_is_null`
guard in 650 lines, which is why the review rubric no longer treats delegation
to it as satisfying its robustness items.

## v1.18.0 scope

**NARROWED BY TEAGUE, 2026-11-03: steps 3 and 4 only.** Step 5 and the
25-language fan-out below move to v1.19.0. The release is held until steps 3
and 4 land, and "green" means **baselined, not fixed**: each known case step 3
surfaces is recorded in a declared baseline with its measurement and its
reason, exactly as `spec/abi/module-abi.yaml` already handles L1, L2, L4 and
the 70 dead literals. The gate is then green today and a NEW case fails it, in
both directions — an unlisted case and a listed case that stops reproducing
both exit 1.

The reasoning for stopping at 4: the four values in step 4 are the only ones
that are *populated and wrong*, so they satisfy every non-emptiness check in
this repository, the kit included. A user sees a plausible wrong answer rather
than a blank. Step 5's 12 live decisions are mostly retractions, and the
fan-out is a column-wise campaign; neither is a reason to hold a tag.

Ordered so that each step is provable, and so the class is closed before the
instances are chased.

1. **Gate L3, L4 and L5** — ~~extend `verify_module_abi.py` so its unit is
   `(language, strategy, raw_type)`~~, add a reachability check for
   specializations nothing declares, and baseline whatever that turns up.
   Without this, fixing the four instances leaves the class open and the next
   drift silent.

   **Revised 2026-11-02, after measuring.** L4 is done — `verify_module_abi.py`
   check 7, 45 specializations baselined under `native_extraction.unreachable`,
   of which 16 are real implementations totalling ~1,642 lines that no input can
   reach. python's `IMPORT_STATEMENT` is 271 of them and is unreachable *twice*,
   since that strategy is also `reserved`.

   **L3 and L5 cannot be gated statically, and they are the same check.** A
   specialization's handled-set is not a property of its source:
   `LuaNativeExtractor<FUNCTION_WITH_PARAMS>` has zero node-type comparisons and
   serves any declaring node that has a `parameters` child. So the real question
   is whether each declaring node owns the child the extractor looks for — which
   needs per-node field data from `grammars/<g>/src/node-types.json`, a
   **submodule** file rather than a committed one. Reading it would couple the
   gate to the 1.2 GB grammar clone and cost `verify_module_abi.py` its
   bare-checkout property.

   So L3/L5 move to step 3, the conformance kit, which already has a binary and
   a corpus and can probe instead of derive. That makes step 3 load-bearing
   rather than cosmetic.
2. **Widen `verify_native_dispatch.py`** — DONE. Three checks now, because a
   node-type literal reaches the grammar three ways: a comparison in a
   per-language extractor or adapter (`dispatch`), an entry in
   `LANGUAGE_FUNCTION_CALL_TYPES` (`table`), and the language name a
   `LanguageTag` hands that table's lookup (`key`). Check `key` exists because
   `GetNodeTypes()` falls back to `DEFAULT_FUNCTION_CALL_TYPES` on a miss, so a
   tag returning `"c_sharp"` against a key `"csharp"` would take the default in
   silence. Measured: 15 tags, 15 keys, all matching, 15 instantiations.

   The variable test changed from a **name** to a **binding**. `"type" in name`
   is wrong in both directions: `type_text` in `cpp_native_extractors.hpp:255`
   is `content.substr()`, and `value_type` in `duckdb_adapter.cpp:748` is a
   DuckDB `LogicalType`, while every `node_type == "lit"` comparison was
   invisible. A variable now qualifies if its nearest preceding assignment
   resolves, through `string(...)`, to `ts_node_type(...)`. A string parameter
   qualifies only when **every** call site of its function passes a node type —
   which is what keeps helpers like `IsTypeNode(const char *child_type)` in
   scope; a purely lexical rule dropped 13 literals the old gate did judge.

   | | old | new |
   |---|---|---|
   | dispatch literals judged | 765 in 20 headers | **1,374 in 27 languages** |
   | distinct literals judged | 413 | **606** |
   | shared-table literals | 0 | **191** |
   | python | 37 | **136** |
   | sql | 4 | **107** |
   | css, html | 0, 0 | **35, 14** |

   The oracle was the old gate itself: every literal it judged must still be
   judged. The first lexical version failed that, by 13.

   70 dead literals are now baselined in `spec/abi/module-abi.yaml` under
   `native_extraction.dead_dispatch`, generated by `--emit-baseline` rather
   than typed. The baseline is bidirectional and both directions are measured:
   an unlisted dead literal exits 1, and a listed one the run cannot reproduce
   also exits 1. The gate runs in CI behind its own planted-fault self-test.

   What the 70 are not is a surprise. In most of them the correct name is live
   in the same symbol table, which is the oracle that proves each finding:
   typescript dispatches on `parameter` while the grammar emits
   `required_parameter`; cpp on `class_definition` against `class_specifier`;
   c on `function_specifier` against `storage_class_specifier`; css on
   `media_feature_name` against `feature_name`; sql on `AND`/`OR` against
   `keyword_and`/`keyword_or`. sql's case is the worst shape, because those two
   dead literals sit in a `!=` filter at `sql_native_extractors.hpp:529`: the
   skip never fires, so `AND` and `OR` are returned as operands. That is
   populated-but-wrong, not blank. (A third sql literal, `"text"` at `:174`, is
   an `==` and so merely misses.)

   **One finding is a known defect, reproduced.** Check `table` flags swift's
   `argument_list`, which is the wrong value recorded in §2 of this document —
   found again, from the source side, by a gate that knew nothing about it.
   Swift's other findings are **new and separate**: the adapter trio
   `struct_declaration` / `enum_declaration` / `variable_declaration` at
   `swift_adapter.cpp:81-85` is about declarations, not call arguments, and no
   wrong output has been traced to it yet.
3. **Make `NATIVE-DECL` per `(raw_type, strategy)`** and stop the kit printing
   `PASS: every declared capability was delivered` when it has only checked the
   function-shaped ones.
4. **The four wrong values**, expectation first, one language per change:
   python's annotation path, swift's container name, lua's modifier walk, lua's
   visibility dispatch.
5. **The L1/L2 backlog** — 8 unmapped languages and 8 unimplemented pairs, each
   answered `implement` / `retract` / `retract and reclassify` per the rubric's
   §4, not fixed by reflex.

Steps 1–3 are gate work and change no output. Steps 4–5 change output and each
needs a failing test first. A **retraction** has no such test — its expectation
lives in the gate, per the rubric's §5.

## Not in scope

- The four dormant languages (`fsharp`, `haskell`, `julia`, `scala`, 47
  declarations) stay dormant. Their defects are inferred from their tables, not
  measured, because the build excludes them.
- The M2 repo split. Every fix here edits
  `src/include/native_context_extraction.hpp`, which is exactly the shared file
  M2 breaks up — so doing this work first is cheaper, not a reason to reorder M2.

## 25-language fan-out, 2026-10-31 — and what it reframed

All 25 remaining compiled-in languages were reviewed against the rubric, one
agent each, 0 errors. Results: **25 of 25 rated `defective` on at least one
surface; no language rated `sound` on node definitions or on the extractor.**
140 wrong-value claims, 117 blanks, 94 under-declaration candidates, 157 rubric
defects, 254 explicit "could not check" notes. The per-language plans are in
`workspace/reviews/`; the clustering is in
`workspace/reviews/00-clustered-work-list.md`.

**The reframe: these are not 25 language defects.** They cluster into six output
columns, and four of those are produced by ONE code path each.

| output column | claims | languages | one code path? |
|---|---:|---:|---|
| `signature_type` | 47 | 16 | yes |
| `modifiers` | 31 | 14 | yes |
| `parameters` | 30 | 17 | mostly |
| `is_exported` | 23 | 15 | **yes** — per-adapter `IsPublicNode`, OR'd at `unified_ast_backend_impl.hpp:326` |
| `semantic_type` | 15 | 11 | no — `.def` choices, per language |
| `name_strategy` | 9 | 7 | no — per language |

So step 5's backlog should be worked **per column**, by four agents, not per
language by twenty-five. A per-language fleet would have each agent re-derive
the same four defects and then contend on one shared header.

**Sampled verification: 11 of 11 claims reproduced**, across 9 languages, chosen
for testability rather than plausibility — the table is in the clustered work
list. The brief that produced the claims *led* the agents, so I expected the
sample to deflate the count; it did not. 11 of 140 verified is the honest
figure.

**Scale correction.** An earlier heuristic pass attributed 47 claims to
`IsPublicNode` by searching each review near its claims; counting only claims
that name it in their own text gives **23**. The window was picking up the
rubric's own item 2.5/2.6 prose. Code-site counts in the clustered work list are
leads, not attributions, and the fan-out schema's missing `cause` field is why.

**Three claims are about the checkers, not a language** —
`verify_native_dispatch.py` (3) and `check_def_type_tables.py` (1). The
three on the dispatch gate are answered by step 2 above.

## Implementation plan for steps 3 and 4 — AWAITING APPROVAL (2026-11-04)

Written before any code, per `CLAUDE.md`. Nothing below is implemented. Every
number in it is measured on `build/release/duckdb` and
`build/release/test/unittest` at `dec3d3b`, except where it says inferred.

### What step 3 can and cannot find

A per-pair **non-emptiness** check finds L3 and L5: a declaring node type that
delivers nothing. It does **not** find the four populated-and-wrong values in
§"The four that are WRONG". Those satisfy every non-emptiness check in this
repository, which is the whole reason they are a separate step. So a quiet
result from step 3 is not evidence about step 4, and this document says so
before the work starts rather than after the result arrives.

### 3a. The missing negative control, against the CURRENT aggregate check

`check_native_declared()` (`scripts/conformance_kit.sh:713`) takes one
argument and reads the precomputed `cap_native_def`. Give it a second argument
`table_override`, exactly as `check_native_absent()` and `check_decl_unique()`
already have, and resolve the capability from that table when it is given.

The plant: copy `def_table`, and change zig's `function_declaration` row from
native strategy `NONE` to `FUNCTION_WITH_PARAMS`. Then run the real check
twice, the two-sided shape the `DECL-UNIQUE` control already uses — baseline
must be `n/a`, planted must be `FAIL`.

**Feasibility measured today**, with the override flag that already exists and
no edit to the script:

```
$ ./scripts/conformance_kit.sh --languages zig --force-capability zig:native
zig  ok  ok  FAIL*  ok  ok  ok  ok  ok
  FAIL   zig  NATIVE-DECL  7 of 10 function defs take parameters in source, 0 delivered
TOTALS ok=7 FAIL=1 dirty=0 void=0 UNDECL=0 n/a=0 | languages=1 (no fixture: 0)
exit 3
```

All 8 existing controls fired in that same run, so this is also a second clean
control read, now on the stage-2 tree.

zig is the right subject and not an arbitrary one. `languages/zig/zig_types.def:82`
declares `function_declaration` as `DEFINITION_FUNCTION | ... , FIND_IDENTIFIER,
NONE`, and zig's corpus holds 10 function definitions of which 7 take a
parameter in source. So the plant is an **L5-shaped fault**: a declaration the
implementation cannot serve, which is the class step 3 exists to catch.

**Why a table plant and not `--force-capability zig:native`.** The override can
only express a per-LANGUAGE capability. It cannot express a `(raw_type,
strategy)` declaration, so it would have to be rewritten at 3c — and a control
written after the change proves nothing about the change. The planted row fires
under both units.

Commit 1 is the control alone: no change to any verdict. Expected after it:
9 controls, 8 matrix columns. `DECL-UNIQ-FLAGS` still has a control and no
column; that stays recorded, not fixed.

### 3b. Where the per-pair declarations come from — THE ONE OPEN DECISION

The def scan cannot supply them as written, for three measured reasons:

| | measured |
|---|---|
| include chain | `languages/typescript/typescript_types.def:60` is `#include "../javascript/javascript_types.def"`. The kit attributes rows by file basename and never follows it. The kit sees **5** function-shaped pairs in typescript's own file; L3 above records **11** declaring node types for `FUNCTION_WITH_PARAMS` alone. **typescript is the only cross-language include of 28 languages.** |
| shadowing | `typescript_types.def:63` documents first-wins shadowing (#222). A naive union would judge declarations the loader dropped. |
| ambiguity | ruby declares 21 raw_types twice with conflicting strategies. The per-pair check must restrict to unambiguous declarations, as `check_native_absent` already does with `HAVING count(DISTINCT native_strategy) = 1`. |

Census of what the kit can see today, from `languages/*/*_types.def` and the
adapters that inline their table: **107** distinct `(language, raw_type,
function-shaped strategy)` triples, **91** of them in compiled-in languages
(the 4 dormant ones hold 16).

  - **Option A — emulate the loader in awk.** Follow `#include`, apply
    first-wins. No extension change, lands inside v1.18.0's gate-work scope.
    The cost is honest: it is a second definition of the loader, in a second
    language, and it can drift from the real one.
  - **Option B — expose `native_strategy` in `ast_type_map()`.** This is the
    kit's own KNOWN GAP (`conformance_kit.sh:66-76`) and the M1 finding behind
    it. It measures the loaded map, which is the artifact rather than its
    description, and it is what makes the kit out-of-tree capable on this axis.
    It changes extension output, so it needs a failing test first, and it
    widens v1.18.0 past gate work.

**Recommendation was A now, B recorded as the M1 gap it already is.**

**DECIDED BY TEAGUE, 2026-11-04: Option B.** `ast_type_map()` gains
`native_strategy`. The kit then reads the declaration surface from the loaded
map instead of from a source scan, which is the ABI's own answer and not a
second definition of the loader.

### What the measurements changed in this plan, after it was written

Three things in the table above were taken from this document's own older text
rather than from a run, and two of them were wrong.

  - **The ambiguity row was stale.** It said ruby declares 21 raw_types twice
    with conflicting strategies, quoting `class` at `ruby_types.def:71` and
    `:593`. Measured 2026-11-04: ruby declares `class` **once**, there are no
    preprocessor directives in that file, and the clean kit run reports
    `ok ruby DECL-UNIQUE every raw_type declared exactly once`. Across all 28
    registered languages the effective named table has **zero** duplicate
    raw_types: row counts and distinct-key counts are equal for every one. So
    the duplicates were fixed before this plan was written. The comment at
    `conformance_kit.sh` in `check_native_absent()` still describes them, and
    §"The four that are WRONG" above is the only place it matters; both are
    historical, not current.
  - **The shadowing row holds in principle and is empty in fact.** typescript's
    effective table is 293 rows with 293 distinct raw_types, so nothing is
    shadowed today. First-wins still decides what would happen, which is why
    the oracle applies it.
  - **The census was file-local, so it under-counted by exactly typescript.**
    Re-derived with the C preprocessor over each language's declaration source,
    applying the first-wins rule `unordered_map`'s initializer-list constructor
    applies: **99** function-shaped declarations across the 28 registered
    languages, not 91. typescript holds **13**, of which **11** are
    `FUNCTION_WITH_PARAMS` — which reproduces L3's figure above from a second,
    independent direction. A basename scan of `typescript_types.def` sees 5.

The oracle is `workspace/` material rather than a gate: it exists to produce
the expectations, and `ast_type_map()` is what the gate reads afterwards.

**And the cost I put on Option A was overstated.** I argued A meant "a second
definition of the loader, in a second language". `scripts/check_def_type_tables.py`
already contains an include-aware `DEF_TYPE` reader — it reports "checked 3837
DEF_TYPE entries across 31 translation unit roots" and its own self-test plants
a **cross-include duplicate** — so A could have reused that reader instead of
being written in awk. The repo already had the include-aware parse; the kit
simply did not use it. B is still the better answer, for the reason it was
chosen (the loaded map is the artifact, and it works for a module that ships no
sources), but the comparison as I put it gave A a cost it did not have.

### 3b as built

`ast_type_map()` now reports `native_strategy` between `name_strategy` and
`flags` — the third and fourth `DEF_TYPE` arguments, adjacent, with `flags`
last. `scripts/generate_taxonomy.py` owns the label table, so
`spec/taxonomy/taxonomy.yaml` stays the single place a strategy is named and
the `taxonomy tables in sync` gate keeps the C++ honest.

The expectation came first:
`test/sql/conformance/native_strategy_declarations.test`, red before the change
with `count_star()` 0 against 1 for the column's own existence. Its numbers are
the oracle's, not the extension's, and it pins the typescript count at 11 with
a note saying that reading 4 there means the surface has silently become a file
scan again.

Two documents described the old surface and are corrected in the same change:
`docs/reference/functions.md`'s Output Schema listed seven columns, three of
which (`semantic_name`, `selector`, `strategy`) do not exist and two of its
three examples could not run; `docs/reference/module-abi.md` said the
native-context axis is in-tree only, which this change is what fixes.

### 3c. The unit change

For each unambiguous declared pair `(language, raw_type, function-shaped
strategy)`:

  - `ok` — some node of that raw_type in the corpus delivers parameters;
  - `FAIL` — nodes of that raw_type exist, at least one takes a parameter in
    source, and none delivers;
  - `void` — no node of that raw_type in the corpus, or none takes a parameter
    in source.

The source-side guard stays exactly as it is, for the reason already written at
`conformance_kit.sh:719-733`: it may only ever decide `void` vs `FAIL`, never
`ok` vs `FAIL`, so a miss costs coverage and not correctness.

The check prints its **denominator**: `N of M declared pairs exercised`. Most of
the 91 are expected to be unexercised, and a count of findings without the
count of pairs that were looked at is the shape this repo has already been
bitten by: a diagnostic with no denominator goes quiet when its input set is
short, and the silence reads as a clean bill.

The language cell keeps one verdict — the worst of its pairs — so the matrix
stays a matrix. The per-pair rows go in the DETAIL block and in
`--matrix-csv`.

### 3d. Baselining, bidirectionally

Each known case step 3 surfaces is recorded with its measurement and its
reason, the way `spec/abi/module-abi.yaml` already records L1, L2, L4 and the
70 dead literals.

  - an unlisted `FAIL` pair exits 3;
  - a listed pair that stops reproducing exits 3 — and that includes a listed
    pair that becomes **`void`**, because "the corpus stopped exercising it" is
    not "it was fixed";
  - the file is a TSV under `spec/conformance/`, read the way the kit already
    reads `def_table` through `read_csv(..., quote := '')`. The kit is bash and
    depends on nothing but a loaded extension; `module-abi.yaml` is YAML read by
    python. Same discipline, deliberately different file type.

### 3e. The PASS line claims more than it checked

`conformance_kit.sh:1528` prints `PASS: every declared capability was
delivered`. After 3c it must state what ran: pairs exercised, pairs baselined.
This is the sentence §"Why no existing gate caught any of them" quotes against
python and zig. Separate, cheap commit.

### Step 4 — the four populated-and-wrong values

Expectation first, one language per change, and the test commit separate from
the implementation commit in every case (`~/.CLAUDE.md`, the Golden Rule).

| # | change | expectation that must fail first |
|---|---|---|
| 4.1 | python annotation path | `d: float = 1` gives `float`; `y: int` never gives the node-type string `assignment` |
| 4.2 | swift argument container | `add(a: 1, b: 2)` gives two arguments, with python's two-row shape as the oracle |
| 4.3 | lua modifier walk | a function merely NESTED in a `local function` carries no modifier |
| 4.4 | lua visibility dispatch | the four rows of §4 above, three of which are wrong today |

Order: 3a, then the 3b answer, then 3c, 3d, 3e, then 4.1 to 4.4. The kit is
re-run after each step-3 commit; the full suite after step 4.

**Baseline to beat, measured 2026-11-04 on `6fe4943` (the suite's own commit;
`6fe4943..dec3d3b` touches only `tracker/`):**
`All tests passed (5 skipped tests, 9032 assertions in 162 test cases)`, exit 0.

The 9032 reconciles with the 9027 recorded on main `d297e85` on 2026-10-28:
`9bc100f` is the only commit touching `test/` in between, and
`test/sql/ast_unparse_leaf_text.test` measures **35 assertions before it and 40
after**, in one test case either way. 9027 + 5 = 9032, 162 cases unchanged.
Measured by running that one file at both versions, not inferred from the diff.

### 3a and 3b as landed

| commit | what |
|---|---|
| `4c45743` | the `NATIVE-DECL` negative control, planted in the declaration table. 9 of 9 controls fire; the full kit run agrees with the clean baseline on **all 224 cells**, exit 0 |
| `ff47d9a` | the expectation, red on the then-current build at its own first assertion |
| `9343405` | `ast_type_map()` gains `native_strategy`; the label table is generated; two documents corrected |
| `ccbc067` | `capability_declarations.test` retracts the closed half of the M1 finding, with its shape kept and pointing the other way |

Static gates, all measured after `ccbc067`:

```
taxonomy tables are in sync with spec/taxonomy/taxonomy.yaml
clang-format --dry-run -Werror src/ast_type_map_function.cpp      rc=0
check_def_type_tables.py --self-test                              self-test OK
check_def_type_tables.py            3837 entries, 31 TU roots, OK
generate_language_reference.py      30 languages, docs/reference/languages NO DIFF
native_strategy_declarations.test   24 assertions, passed
capability_declarations.test        75 assertions, passed (failed at :166 before ccbc067)
make release                        0 errors, 106 warnings (104 + one SetValue
                                    deprecation per build target, same warning
                                    at a new line)
```

Still to do in step 3: **3c** (the per-pair unit), **3d** (the bidirectional
baseline) and **3e** (the `PASS` line). 3c is now a query against
`native_strategy` rather than a scan, which is what the decision bought.
