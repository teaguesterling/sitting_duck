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
| `check_def_type_single_line.py` | yes | formatting only |
| `generate_taxonomy.py --check` | yes | taxonomy table sync only |
| `verify_native_dispatch.py` | **yes, since step 2** | was: matched `strcmp(<var with "type">, "lit")` only, so python's `==` idiom left 95 of 132 literals unexamined; globbed `*_native_extractors.hpp`, so the shared tables and the adapters were unread; printed **no line** for a language with no extractor header. Now: 1,374 literals in 27 languages, 191 in the shared table, 70 baselined. Remaining limit: 103 comparison sites whose operand is source text, which it names by operand |
| `conformance_kit.sh` | **no** | `NATIVE-DECL` covers 5 of 10 declarable strategies and aggregates per language, so it prints `PASS: every declared capability was delivered` for python and zig while a declared capability of each delivers nothing |
| the three `audit_*.py` | **no** | — |

Only an **oracle language** caught the swift defect. That is now rubric item 1.7.

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
`verify_native_dispatch.py` (3) and `check_def_type_single_line.py` (1). The
three on the dispatch gate are answered by step 2 above.
