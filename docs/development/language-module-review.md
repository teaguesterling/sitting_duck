# Reviewing a language module

[Adding Languages](adding-languages.md) says how to build one. This page says how
to judge one, and it is a checklist rather than advice for a measured reason.

**The advice already existed and did not hold.** `adding-languages.md` §2.1 is
titled "Native Context Extraction Setup (CRITICAL)" and states, in capitals,
that you MUST update four files with exact class-name consistency. Measured on
2026-10-30: 8 languages declare a native strategy with no
`NativeExtractionTraits` mapping at all, and 8 more (language, strategy) pairs
have a mapping but no specialization for the strategy they declare. Prose asked
for the right thing and 16 cases drifted anyway.

So every item below carries **how to prove it** and **what enforces it**. The
three pilot reviews that calibrated this page (python, zig, lua) each found that
the first version of this table overstated enforcement, so the `Enforced by`
column now has three values and no booleans:

| value | meaning |
|---|---|
| **CI** | a workflow job fails on it. You do not have to run it; you do have to read it. |
| **script** | a checker exists and **nothing runs it**. You must run it yourself, and a reviewer who did not run it has no evidence. |
| **judgment** | no checker exists. This is where your time is worth spending. |

`script` is not a weaker `CI`. Four of the seven checkers this page names run in
no workflow at all, so for those the review *is* the gate.

## How to run the checks

```bash
# enforced in CI — run them to see what CI will say
python3 scripts/verify_module_abi.py                  # the module ABI contract
python3 scripts/verify_module_abi.py --self-test      # proves the gate can fail
python3 scripts/check_def_type_single_line.py         # one DEF_TYPE per line
python3 scripts/generate_taxonomy.py --check          # taxonomy tables in sync

# a checker exists, nothing runs it. If you skip these, say so.
python3 scripts/verify_native_dispatch.py             # dead dispatch literals
python3 scripts/audit_unreachable_declarations.py --binary build/release/duckdb
python3 scripts/audit_shadowed_declarations.py        # declarations hidden by an include
python3 scripts/audit_anon_token_collisions.py --language <lang>
bash scripts/conformance_kit.sh --binary build/release/duckdb --languages <lang>
```

**A verdict vocabulary, borrowed from the kit, because a vacuous pass is not a
pass.** For every gated item record one of `ok`, `n/a` (the check does not apply
— e.g. the shadowed-declarations audit prints nothing for a language with no
`#include` edge), or `void` (the check applies and could not run). The kit's own
rule holds here: **`void` is never read as a pass.**

Two checks have **no per-language denominator**: `verify_module_abi.py` and
`audit_shadowed_declarations.py` print repo-wide aggregates and may never print
your language's name. Quoting their output as evidence *about your language* is a
mistake; quote the aggregate and say what it does and does not cover.

## 1. Node definitions

The declaration table: `src/language_configs/<lang>_types.def`, or an inline
table in `src/language_adapters/<lang>_adapter.cpp` for `sql` and `duckdb`.

| # | Expectation | Proof | Enforced by |
|---|---|---|---|
| 1.1 | Every declared `raw_type` is one the grammar can emit | `audit_unreachable_declarations.py --binary …` | **script** |
| 1.2 | No `raw_type` declared twice with conflicting strategies | kit `DECL-UNIQUE` (the verdict). `DECL-UNIQ-FLAGS` is its **planted-fault control**, not a per-language cell | **script** |
| 1.3 | A grammar with a named rule *and* an anonymous token of the same `raw_type` declares both (#215) | `audit_anon_token_collisions.py --language <lang>`. Without `--language` it prints counts, not names | **script** |
| 1.4 | No declaration is shadowed by an `#include`d sibling table | `audit_shadowed_declarations.py`. Record `n/a` where the language has no include edge | **script** |
| 1.5 | One `DEF_TYPE` per line | `check_def_type_single_line.py` | **CI** |
| 1.6 | Every `semantic_type`, strategy and flag name exists in the taxonomy | `generate_taxonomy.py --check` | **CI** |
| 1.7 | Every declared capability is actually delivered | kit, **for function-shaped strategies only** — see below | **partly / judgment** |
| 1.8 | The `semantic_type` **chosen** is the right one for that node | — | **judgment** |
| 1.9 | `NAME_ROLE` (a 2-bit field, not a flag) and `IS_SCOPE` are set where the language's semantics call for it | — | **judgment** (#64 is the open audit) |
| 1.10 | `name_strategy` finds the name a user would expect, including on wrapper nodes | — | **judgment** |

**1.7 is the item that misled all three pilots.** The kit's `NATIVE-DECL` check
covers only `FUNCTION_NATIVE_STRATEGIES` — 5 of the 10 declarable dispatched
strategies, and its 7-name list includes two the spec marks `reserved`, which no
declaration may use. It is also aggregated **per language**: one node type
delivering masks every other. So the kit prints

```
PASS: every declared capability was delivered
```

for languages where a declared capability delivers nothing, and for one language
where it was the *only* declared capability. `FUNCTION_CALL`,
`VARIABLE_WITH_TYPE`, `CLASS_WITH_*` and `CUSTOM` are uncounted entirely.

**So 1.7 needs one probe per declared strategy, by hand, each paired with an
oracle** — the same shape in a language that implements the strategy, so an
empty or odd result cannot be mistaken for "this strategy yields nothing
anywhere". The oracle is what caught swift's wrong answer; no gate did.

## 2. The adapter

`src/include/language_adapter.hpp` (declaration) and
`src/language_adapters/<lang>_adapter.cpp` (definition).

| # | Expectation | Proof | Enforced by |
|---|---|---|---|
| 2.1 | The 10 pure virtuals are implemented | a successful build | **CI** |
| 2.2 | `GetAnonNodeConfigs` is overridden where 1.3 applies | `audit_anon_token_collisions.py` | **script** |
| 2.3 | Declared in `cmake/BuiltinLanguages.cmake`, which generates `sitting_duck_builtin_languages.def`, which the registry macro-expands. `src/language_adapter_registry_init.cpp` holds **no per-language code** — do not go looking for an entry there | `ast_supported_languages()` lists the language | **script** |
| 2.4 | Extensions and aliases cover what users will pass. They live in **`src/ast_file_utils.cpp`**, in two tables that must agree with each other — not in the adapter | read both tables | **judgment** |
| 2.5 | `IsPublicNode` matches the language's own visibility rule | **Oracle**: query `is_exported(flags)` on a fixture holding one public and one private definition **of each syntactic form the language has**, and confirm they differ | **judgment** |
| 2.6 | Every node-type literal `ExtractNodeName`/`IsPublicNode` dispatches on exists in the grammar | the dispatch gate does **not** read adapters — check by hand against `ts_symbol_names[]` | **judgment** |

**2.5 and 2.6 are one bug in lua.** `IsPublicNode` dispatches on three node
types absent from tree-sitter-lua, so `local` is never consulted and three of
four visibility answers are wrong. `IS_EXPORTED` appears in **zero** `.def`
files tree-wide — it is OR'd in at runtime from `IsPublicNode`
(`unified_ast_backend_impl.hpp:326`), which is why it belongs here and not in
§1.

A language can be absent from the build while having a complete table: `yaml` was
in that state for a long time (#227), and `fsharp`, `haskell`, `julia` and
`scala` are today. **State which it is first** — every other finding changes
meaning.

## 3. The extractor

`src/include/<lang>_native_extractors.hpp`, plus its wiring in
`src/include/native_context_extraction.hpp`.

| # | Expectation | Proof | Enforced by |
|---|---|---|---|
| 3.1 | A `NativeExtractionTraits<<Lang>Adapter>` mapping exists | `verify_module_abi.py` check 4 | **CI, baselined** |
| 3.2 | Every declared strategy has its own specialization | `verify_module_abi.py` check 6 | **CI, baselined** |
| 3.2b | Every **declaring node type** is handled by that specialization | read the specialization against every `DEF_TYPE` line declaring the strategy, and probe one instance of each | **judgment, and not statically gateable** — see below |
| 3.2c | Every specialization is **reachable** — some declaration names its strategy | `verify_module_abi.py` check 7 | **CI, baselined** |
| 3.2d | Every declaration the strategy can structurally serve — the node actually owns the thing being extracted | parse one instance. `grammars/<g>/src/node-types.json` has the field data, but it is a **submodule** file, not committed like `generated_parsers/*/parser.c` | **judgment** — see below |
| 3.3 | Every declared strategy is one the dispatcher has a case for | `verify_module_abi.py` checks 1–3 | **CI** |
| 3.4a | Every node-type literal in the extractor header exists in the grammar's symbol table | `verify_native_dispatch.py` | **script** |
| 3.4b | …and in the **shared** tables the language delegates to | the gate does not read `function_call_extractor.hpp` — check by hand | **judgment** |
| 3.5 | Each specialization's failure path leaves no **plausible** value behind | read it | **judgment** |
| 3.6 | Upward ancestor walks are depth-bounded and guard `ts_node_is_null` | read it | **judgment** |
| 3.7 | No dispatch literal contradicts a claim in the same module's comments | grep the literals, read the comments | **judgment** |
| 3.8 | The extracted values are *correct*, not merely non-empty | **Oracle** (see 1.7), plus probe the boundary of every heuristic | **judgment** |
| 3.9 | Every `NativeContext` field a specialization writes is surfaced by a `read_ast` column | grep the field's consumers in `src/` | **judgment** |

**"CI, baselined" means exit 0 is not clean.** A pair listed under
`native_extraction.unimplemented`, or a language under `unmapped`, makes
`verify_module_abi.py` exit 0 by design. Read its `declared unimplemented:` and
`declared unmapped:` lines and grep `spec/abi/module-abi.yaml` for your language
before concluding anything from the exit code.

**Why 3.2b and 3.2d stay judgment, measured 2026-11-02.** A specialization's
handled-set is not derivable from its own source. `LuaNativeExtractor<FUNCTION_WITH_PARAMS>`
contains **zero** node-type comparisons — it walks for a `parameters` child and
therefore serves any declaring node type that has one. So "which raw_types does
this specialization handle" is not a property of the C++ at all; it is a property
of whether each declaring node owns the child the extractor looks for. 3.2b and
3.2d are the same question from two directions.

Answering it statically needs per-node field data. That lives in
`grammars/<g>/src/node-types.json`, which is a **grammar submodule** file —
unlike `generated_parsers/*/src/parser.c`, which is committed. A gate reading it
would couple to the 1.2 GB submodule clone and lose `verify_module_abi.py`'s
bare-checkout property. The better home is the conformance kit, which already
has a built binary and a corpus and can answer it by probing rather than
deriving.

**3.2b exists because check 6's unit is the strategy and a declaration's unit is
the node type.** typescript declares `FUNCTION_WITH_PARAMS` on 11 node types and
`VARIABLE_WITH_TYPE` on 17; lua declares `FUNCTION_CALL` on both `function_call`
and `method_index_expression`. One specialization greens all of them, and
removes the baseline entry too.

**3.4a's gate is narrower than it looks.** It matches only
`strcmp(<variable whose name contains "type">, "literal")`. A language using
`node_type == "literal"` is largely unexamined — for python that is 37 of 132
literals. It also prints **no line** for a language with no extractor header, so
"examined and clean" and "never examined" are the same output. Get a positive
denominator or record `void`.

**3.5 and 3.6 are narrower than the first version of this page claimed.** The
call site already wraps extraction in `try/catch(...)`
(`unified_ast_backend_impl.hpp:642`), so a missing per-specialization catch is an
inconsistency, not a crash. The risk worth naming is a catch that substitutes a
*plausible* value — `CLASS_WITH_METHODS` clears `parameters` and `modifiers` but
sets `signature_type = "class"`, so failure reads as success. And 3.6 applies to
**upward walks**, not child-index loops, which are bounded by construction.

**On reusing the shared helper.** `function_call_extractor.hpp` generalises
call-argument extraction over a per-language node-name table, and
`native_context_extraction.hpp` carries `ExtractParameterList`,
`ExtractModifiersFromNode`, `SplitAnnotations` and `ExtractDecoratorTexts`. Use
them rather than writing a third. But do not read delegation as satisfying 3.5
or 3.6: measured, `function_call_extractor.hpp` contains **zero** depth bounds
and **one** `ts_node_is_null` guard in 650 lines, and 9 of its per-language
literals are absent from their grammars. Findings there are filed against the
shared helper, once, not against each language that delegates to it.

## 4. Implement, retract, or reclassify?

A declared strategy with nothing behind it has three honest fixes, and choosing
the wrong one is how a module acquires an extractor nobody wanted.

1. **Implement** when the node really carries the thing the strategy extracts.
2. **Retract to `NONE`** when the declaration claims something the grammar does
   not have — the node owns no argument list, no annotation, no parameter list.
3. **Retract and reclassify** when the strategy was mis-declared *because* the
   `semantic_type` was. A node declared `COMPUTATION_CALL` that owns no argument
   list is usually not a call at all, and retracting the strategy alone leaves
   the wrong classification in place.

A review must name which of the three applies, and why, for every pair it finds.
"Add an extractor" is a guess about intent, not a finding. Worked examples are in
§7, past tense, so that no open case arrives with its verdict attached.

### 4b. Declare, or leave alone?

§1–§4 run one direction only: they ask whether a declaration is delivered, never
whether a node **should have declared** something. Both of zig's largest
user-facing gaps are of this kind — `function_declaration` and `builtin_function`
each declare `NONE` beside a real argument container, so
`pub fn add(a: i32, b: i32)` measures `parameters = []`.

Scope this deliberately, or it becomes a survey of the whole grammar: consider
only nodes declared `NONE` **whose children include a container some other
declaration in the same language already names**. For each, say declare or leave
alone, with the same grammar evidence §4 demands.

## 5. What a review must produce

1. **The build state first** — compiled in, or dormant table.
2. **Every `CI` and `script` item run**, with its real output quoted and a
   verdict of `ok`, `n/a` or `void`. Some exit non-zero by design:
   `verify_native_dispatch.py`'s current baseline is recorded in
   `spec/abi/module-abi.yaml`, not in this page, so that it cannot go stale here.
3. **A rating per surface**, on this scale and no other:

   | rating | means |
   |---|---|
   | `sound` | no measured wrong or missing answer |
   | `partial` | a blank where a value is owed |
   | `defective` | a populated value that is **wrong** |
   | `absent` | the surface does not exist (a state, not a failure) |

   `defective` is worse than `partial`: a blank invites a second look, a wrong
   value passes every non-emptiness check we own.

4. **Judgment findings separated from gated ones**, saying what you read and what
   you compared it against.
5. **§4 and §4b answered** for every pair and every candidate, with grammar
   evidence from a real parse.
6. **What you could not check, and why.** Absence of a finding is not absence of
   a defect, and a review reporting nothing unchecked is the one to distrust.

**Expectation first, with one exception.** A proposed change names the test that
fails on today's output, confirmed to fail for the stated reason, before any
implementation. A **retraction has no such test** — output is identical before
and after, so a test would pin the bug. Its expectation lives in the gate
instead: edit `spec/abi/module-abi.yaml` first, confirm `verify_module_abi.py`
goes red with the specific message, then change the declaration. If no gate
reports it, say which gate change would.

## 6. Where the counts live

`spec/abi/module-abi.yaml` carries the declared baselines — `unmapped`,
`unimplemented`, `reserved`, `no_declaration_source` — and
`verify_module_abi.py` fails on any drift from them. A review that finds a new
case does not widen those lists; it fixes the case, or records why the
declaration should never have been there. A finding **finer** than the baselines
can express (a single node type inside a declared pair) belongs in the review
and in a tracker entry, not in the yaml.

## 7. Worked examples, past tense

- **zig `call_expression` / `FUNCTION_CALL` → implement.** The node has an
  `arguments` child holding real arguments.
- **graphql `fragment_spread` / `FUNCTION_CALL` → retract.** A fragment spread is
  `... FragmentName` plus optional directives; it has no argument list, and the
  name already binds.
- **markdown `link_reference_definition` / `CUSTOM` → retract.** The only
  `CUSTOM` declaration in the tree, and nothing defines what it should produce.
- **python `assignment` / `VARIABLE_WITH_TYPE` → defective, not missing.** The
  `annotated_assignment` branch is dead — the node does not exist in the grammar
  — so `d: float = 1` reports `int` and `y: int` reports the string
  `assignment`. A populated wrong answer, found by reading the branch against the
  symbol table.
- **lua `local` modifiers → defective.** A four-deep ancestor walk gave an
  anonymous function nested inside a `local function` its parent's `[local]`.
  Right on the form its own test contained, wrong one nesting level out.
