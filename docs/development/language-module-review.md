# Reviewing a language module

[Adding Languages](adding-languages.md) says how to build one. This page says how
to judge one, and it is a checklist rather than advice for a measured reason.

**The advice already existed and did not hold.** `adding-languages.md` §2.1 is
titled "Native Context Extraction Setup (CRITICAL)" and states, in capitals,
that you MUST update four files with exact class-name consistency. On
2026-10-30, measured across every declaration table in the repository:

- **8 languages** declare a native strategy and have no
  `NativeExtractionTraits` mapping at all — 4 of them compiled in.
- **8 more (language, strategy) pairs** have a mapping but no specialization for
  the strategy they declare, so the language's own base template answers with an
  empty context. All 8 are compiled in, and `ARROW_FUNCTION` accounts for five
  of them.

Prose asked for the right thing and 16 cases drifted anyway. So every item below
carries **how to prove it**, and says plainly whether a machine enforces it
today. An item with no checker is where a reviewer's judgment actually earns its
keep; an item with a checker needs running, not reading.

## How to run the checks

```bash
# static, no build needed
python3 scripts/verify_module_abi.py                  # the module ABI contract
python3 scripts/verify_native_dispatch.py             # dead dispatch literals
python3 scripts/audit_unreachable_declarations.py     # raw_types no grammar emits
python3 scripts/audit_shadowed_declarations.py        # declarations hidden by an include
python3 scripts/audit_anon_token_collisions.py        # named/anonymous raw_type collisions
python3 scripts/check_def_type_single_line.py         # one DEF_TYPE per line

# needs a built extension
bash scripts/conformance_kit.sh --binary build/release/duckdb --languages <lang>
```

## 1. Node definitions

The declaration table: `src/language_configs/<lang>_types.def`, or an inline
table in `src/language_adapters/<lang>_adapter.cpp` for `sql` and `duckdb`.

| # | Expectation | Proof | Gated? |
|---|---|---|---|
| 1.1 | Every declared `raw_type` is one the grammar can emit | `audit_unreachable_declarations.py` | **yes** |
| 1.2 | No `raw_type` declared twice with conflicting strategies or flags | kit `DECL-UNIQUE`, `DECL-UNIQ-FLAGS` | **yes** |
| 1.3 | A grammar with a named rule *and* an anonymous token of the same `raw_type` declares both (#215) | `audit_anon_token_collisions.py` | **yes** |
| 1.4 | No declaration is shadowed by an `#include`d sibling table | `audit_shadowed_declarations.py` | **yes** |
| 1.5 | One `DEF_TYPE` per line | `check_def_type_single_line.py` | **yes** |
| 1.6 | Every `semantic_type`, strategy and flag name exists in the taxonomy | generated tables + CI `taxonomy-tables-sync` | **yes** |
| 1.7 | Every declared capability is actually delivered on a real corpus | `conformance_kit.sh` | **yes** |
| 1.8 | The `semantic_type` **chosen** is the right one for that node | — | **no: judgment** |
| 1.9 | `NAME_ROLE`, `IS_SCOPE`, `IS_EXPORTED` are set where the language's semantics call for it | — | **no: judgment** (#64 is the open audit) |
| 1.10 | `name_strategy` finds the name a user would expect, including on wrapper nodes | — | **no: judgment** |

Items 1.8–1.10 are the review's real work. The gates prove a declaration is
*well-formed and delivered*; nothing proves it is *right*.

## 2. The adapter

`src/include/language_adapter.hpp` (declaration) and
`src/language_adapters/<lang>_adapter.cpp` (definition).

| # | Expectation | Proof | Gated? |
|---|---|---|---|
| 2.1 | The 10 pure virtuals are implemented | the compiler | **yes** |
| 2.2 | `GetAnonNodeConfigs` is overridden where 1.3 applies | `audit_anon_token_collisions.py` | **yes** |
| 2.3 | Registered in `src/language_adapter_registry_init.cpp` | `ast_supported_languages()` lists it | partly |
| 2.4 | Present in `cmake/BuiltinLanguages.cmake`, or the table is dormant | the language is absent from `ast_supported_languages()` | partly |
| 2.5 | Extensions and aliases cover what users will actually pass | — | **no: judgment** |
| 2.6 | `IsPublicNode` matches the language's own visibility rule | — | **no: judgment** |

On 2.4: `yaml` had an adapter, a declaration and a 43-entry table for a long
time while being absent from the build (#227), and `fsharp`, `haskell`, `julia`
and `scala` are in that condition today. A dormant table is not a bug, but a
reviewer must say which state a language is in, because every other finding
changes meaning.

## 3. The extractor

`src/include/<lang>_native_extractors.hpp`, plus its wiring in
`src/include/native_context_extraction.hpp`.

| # | Expectation | Proof | Gated? |
|---|---|---|---|
| 3.1 | A `NativeExtractionTraits<<Lang>Adapter>` mapping exists | `verify_module_abi.py` check 4 | **yes** |
| 3.2 | **Every declared strategy has its own specialization** | `verify_module_abi.py` check 6 | **yes** |
| 3.3 | Every declared strategy is one the dispatcher has a case for | `verify_module_abi.py` checks 1–3 | **yes** |
| 3.4 | Every node-type literal an extractor dispatches on exists in the grammar's symbol table | `verify_native_dispatch.py` | **yes** |
| 3.5 | Each specialization wraps its body in `try/catch` and clears the context on failure | — | **no: read it** |
| 3.6 | Child loops are bounded (the house bound is 1000) and guard `ts_node_is_null` | — | **no: read it** |
| 3.7 | Grammar node names were **measured**, not assumed, and the measurement is in a comment | — | **no: read it** |
| 3.8 | The extracted values are *correct*, not merely non-empty | — | **no: judgment** |

3.1 and 3.2 are the pair that drifted. The trap is that neither failure is a
compile error: a missing mapping selects the engine-wide empty default, and a
missing specialization selects the language's own empty base template. Both
produce a blank column and a passing build.

Where a shared helper already covers the shape, use it rather than writing a
third one: `function_call_extractor.hpp` generalises call-argument extraction
over a per-language node-name table, and `native_context_extraction.hpp` carries
`ExtractParameterList`, `ExtractModifiersFromNode`, `SplitAnnotations` and
`ExtractDecoratorTexts`.

## 4. Implement, or retract?

A declared strategy with nothing behind it has two honest fixes, and choosing
the wrong one is how a module acquires an extractor nobody wanted.

**Implement** when the node really carries the thing the strategy extracts.
`zig`'s `call_expression` has an `arguments` child holding real arguments;
`hcl`'s `function_call` has `function_arguments`. Both should deliver.

**Retract to `NONE`** when the declaration claims something the grammar does not
have. `graphql`'s `fragment_spread` is `... FragmentName` with optional
directives — it has no argument list, so `FUNCTION_CALL` was a mis-declaration
and the useful part, the name, already binds. `markdown`'s
`link_reference_definition` is the only `CUSTOM` declaration in the tree, and
nothing defines what `CUSTOM` should produce for it.

A review must name which of the two applies, and why, for every pair it finds.
"Add an extractor" is not a finding; it is a guess about intent.

## 5. What a review must produce

1. **State the language's build state first** (compiled in, or dormant table).
2. **Run every gated check** and quote its output. A gate you did not run is not
   evidence, and several of these exit non-zero by design — `verify_native_dispatch.py`
   reports 26 dead branches of 765 literals today and that is its baseline, not
   a regression.
3. **Rate the three surfaces separately.** A module can have an immaculate
   declaration table and no extractor at all.
4. **Separate the ungated items.** Findings on 1.8–1.10, 2.5–2.6 and 3.5–3.8 are
   your judgment, so say what you read and what you compared it against.
5. **For every declared-but-unimplemented pair, answer §4** — implement or
   retract, with the grammar evidence for the choice.
6. **Say what you could not check**, and why. Absence of a finding is not
   absence of a defect, and a review that reports nothing unchecked is the one
   to distrust.

A plan that proposes changes must put the **expectation first**: the test that
fails on today's output, confirmed to fail for the stated reason, before any
implementation. That is the house rule, and it is what makes a silent-empty fix
provable.

## 6. Where the counts live

`spec/abi/module-abi.yaml` carries the declared baselines — `unmapped`,
`unimplemented`, `reserved`, `no_declaration_source` — and
`verify_module_abi.py` fails on any drift from them. A review that finds a new
case does not widen those lists; it fixes the case, or it records why the
declaration should never have been there.
