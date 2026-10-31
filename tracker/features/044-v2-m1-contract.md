# 044 — v2.0 M1: Contract extraction (in-tree, no files move)

Opening milestone of the v2.0 arc (docs/planning/v2-architecture.md). M1 is contract-first,
**in the current repo, no files move**; the scary repo split is M3, gated behind the
split-after-stability rule. Started 2026-09-16 after v1.14.0 released (release CI green;
only the known v2.0-cyanoptera canary red = #160).

## M1 deliverables (RFC "The contract")
1. **Module ABI** — versioned C++ interface for adapters (registration, extraction strategies,
   flag/strategy enums), semver'd independently of the extension version.
2. **Taxonomy spec** — the universal `semantic_type` system + flag layout as a single versioned
   DATA artifact; enum tables + name tables GENERATED from it (retires the #80 hand-sync parity
   risk). ← THE FIRST SLICE (below). **DONE — PR #195, 2026-10-07.**
3. **Conformance kit** — generalize the #91 pattern: every module proves call nodes are named
   (#name binds), modifiers/signatures populate per declared capabilities, raw-AST-join
   agreement, and no silent-empty (#89). Out-of-tree modules run the same kit.
4. **Trust-boundary doc** — write the register_language (#80) security model into the contract
   (settled: default-off `sitting_duck_enable_runtime_grammars`, allowed_directories; no
   pre-exec vetting of a .so; no privilege escalation beyond SET/LOAD).
5. **Merge #80** — the runtime `.so` door is part of the contract.

Safety net for later (M2): the #92 invariance test — default build byte-equivalent, canonical
registration order — must stay green throughout.

## First slice: taxonomy-spec → codegen — **LANDED 2026-10-07, PR #195**

`spec/taxonomy/taxonomy.yaml` is now the source of truth. `scripts/generate_taxonomy.py`
writes the tables into marked `<<< BEGIN GENERATED TAXONOMY >>>` blocks in five sources —
`src/include/semantic_types.hpp`, `src/semantic_types.cpp`, `src/include/node_config.hpp`,
`src/language_config_json.cpp`, `src/ast_type_map_function.cpp` — and the CI job
`taxonomy-tables-sync` makes any divergence a hard failure. Nothing in the build
regenerates them, so a spec change must be regenerated and committed together.

**Scope caveat, stated precisely because it is easy to overstate:** the *taxonomy
tables* (semantic-type codes and names, the flag byte, the strategy enums) are
generated. The per-language `src/language_configs/*.def` files still reference those
names **by hand** — mapping a grammar's node types onto semantic types is the work and
is not generated. Step (e) below was the design intent and remains accurate.

Retires the .def ↔ C++ drift that has caused real bugs (e.g. flag-name/enum sync). Before
this slice, these were hand-maintained in parallel:
  - semantic_type codes + names ....... src/include/semantic_types.hpp (+ ast_type.hpp)
  - flag byte layout (bits + names) ... src/include/node_config.hpp (ASTNodeFlags: IS_SYNTAX_ONLY
        0x01, NAME_ROLE 0x06, IS_SCOPE 0x08, IS_EXPORTED 0x10, IS_CONSTITUENT 0x20, …)
  - name/native extraction strategy enums + names ... src/language_config_json.cpp (9 name
        tables), consumed by the .def DEF_TYPE(raw_type, semantic_type, name_extraction,
        native_extraction, flags) macros in src/language_configs/*.def
  - runtime introspection surface ..... ast_type_map(), semantic_type_code('NAME'), etc.

Plan for the slice:
  a. Define ONE spec artifact (YAML) capturing: semantic_type {code,name,super-type}, the flag
     byte {bit,name,role}, and the strategy enums {name}. Versioned (taxonomy spec version).
  b. Write scripts/generate_taxonomy.py (mirror scripts/embed_sql_macros.py: deterministic,
     idempotent) that generates the C++ tables (semantic_types.hpp enum + name arrays, the
     node_config flag defs, language_config_json.cpp name tables) FROM the spec.
  c. Prove BYTE-EQUIVALENCE first: generated output must match the current hand-written tables
     exactly (no behavior change) — same discipline as embed-sync. Only then does the spec
     become source of truth.
  d. CI check "taxonomy tables in sync" (mirror "Embedded SQL macros header in sync").
  e. .def files keep referencing names; the spec is the single definition those names resolve to.
Metric: byte-equivalent generated tables + green full suite + a sync CI check. No behavior change.
**All of (a)–(e) are done; the metric was met.** Deliverable 2 of M1 is complete.
(That sentence used to end "1, 3, 4 and 5 remain" — see the M1 standing below, which
supersedes it: 5 had already merged when it was written.)

## M1 standing — measured 2026-10-30, after v1.17.0

| # | Deliverable | State | Evidence |
|---|---|---|---|
| 1 | Module ABI | **DONE** | `spec/abi/module-abi.yaml` (ABI 1.0.0) + `docs/reference/module-abi.md` + `scripts/verify_module_abi.py`, CI job `module-abi-sync` |
| 2 | Taxonomy spec | DONE | PR #195, `taxonomy-tables-sync` |
| 3 | Conformance kit | **DONE in-tree; out-of-tree partial** | `ok=192 FAIL=0 dirty=1 void=7 UNDECL=5 n/a=19 \| languages=28`, all 9 negative controls `ok`, **exit 0** on `cee95ef`. The "exits 3 on a clean tree" baseline above is now stale: v1.17.0 fixed the three #220 NATIVE-DECL FAILs. The limit is one axis: the native-context checks are DEF-DERIVED and in-tree only, because `native_strategy` is not surfaced by `ast_type_map()` (kit:49–76). It matters less than it reads — a runtime config cannot declare native extraction at all, since `ParseLanguageConfigJSON` refuses `native_strategy` outright |
| 4 | Trust-boundary doc | **DONE** | the "Trust boundary" section of `docs/reference/module-abi.md`, written with the contract as the RFC intended |
| 5 | Merge #80 | DONE | `aee6656`, 2026-07-24 |

M2 (`048` row 13) was gated on deliverable 1 stabilising. The gate now exists, so
"stabilising" is measurable rather than a judgement: `module-abi-sync` fails on any
drift between the spec and the sources.

### The ABI is three contracts, not one

Measured while writing it, and the reason the spec has three layers:

- **Binary.** `register_language()` crosses the library boundary with ONE symbol,
  `const TSLanguage *(*)()`, plus tree-sitter's own `ts_language_version()` range
  check. No C++ type crosses it, because every `LanguageAdapter` method passes
  `string`/`vector`/`unordered_map`/`unique_ptr`.
- **Data.** Out-of-tree semantics arrive as JSON (`ParseLanguageConfigJSON` →
  `DynamicLanguageAdapter`), which implements the whole source interface from data.
  This is the layer a third-party module actually uses.
- **Source.** `LanguageAdapter`'s 10 pure virtuals (compiler-enforced) plus the
  template hooks that are NOT (below). In-tree only.

### What the gate catches that nothing else did

`verify_native_dispatch.py` iterates `*_native_extractors.hpp`, so a language with
NO extractor header is absent from its input set and never judged. The conformance
kit's NATIVE-DECL check judges only function-shaped strategies, so a declared
`FUNCTION_CALL` or `CUSTOM` with no extractor is outside its matrix. #220's shape —
a declared strategy with no `NativeExtractionTraits` mapping — fell between them.

**Found by the new gate, on this tree:** four compiled-in languages are in exactly
that condition, so `parameters` comes back `[]` where the `.def` declares extraction.

| language | node type | declared | site |
|---|---|---|---|
| graphql | `fragment_spread` | FUNCTION_CALL | `graphql_types.def:226` |
| hcl | `function_call` | FUNCTION_CALL | `hcl_types.def:170` |
| markdown | `link_reference_definition` | CUSTOM | `markdown_types.def:169` |
| zig | `call_expression` | FUNCTION_CALL | `zig_types.def:182` |

All four measured on `build/release/duckdb`, and all four return `parameters = []`
while binding a name (name extraction is a separate axis). Oracle: python, which HAS
the mapping, returns `[{'name': '', 'type': 1}, {'name': '', 'type': 2}]` for
`foo(1, 2)`. So this is the silent-empty defect, not a property of FUNCTION_CALL.

Four more languages (`fsharp`, `haskell`, `julia`, `scala`, 47 declarations) are in the
same condition but absent from the build — dormant, the condition yaml was in before
#227. All eight are DECLARED under `native_extraction.unmapped` so the gate is green
today and a NINTH fails it. Fixing them is extractor work, deliberately not bundled
into the contract.

### The gate's own input set, which its first draft got wrong

Worth recording because it is the same defect one level up. The first draft scanned
`src/language_configs/*_types.def` alone — and **sql's table is not there**: its 265
`DEF_TYPE` entries are inlined in `src/language_adapters/sql_adapter.cpp`, so sql, one
of the three languages #220 broke, was absent from the gate's input set and silently
unjudged. The count it printed (27 declaring languages) was wrong for the same reason;
the real figure is 330 non-NONE declarations across 28 languages.

Fixed by reusing `audit_leaf_text_gaps`' parser helpers rather than writing a third
one: `resolve_config_source` already knows sql and duckdb declare inline,
`preprocess_def` follows `.def` #includes, and `iter_macro_arglists` tolerates several
calls per line and C-concatenated literals — the two things clang-format does to
`sql_adapter.cpp`.

The durable fix is check 5, the DENOMINATOR: every per-language adapter must resolve to
a table the gate can read, or be named in `no_declaration_source`. Its own first version
tested that a FILE existed, and passed vacuously — `duckdb_adapter.cpp` exists and holds
**zero** `DEF_TYPE` entries. It counts entries now.

Also latent: **6 of the 17 `NativeExtractionStrategy` members have no case in
`ExtractNativeContextTemplated`** (`GENERIC_FUNCTION`, `METHOD_DEFINITION`,
`INTERFACE_DEFINITION`, `ENUM_DEFINITION`, `IMPORT_STATEMENT`, `EXPORT_STATEMENT`).
No `.def` names one today; the spec lists them as `reserved` and the gate refuses a
`.def` that does. Note that `conformance_kit.sh`'s `FUNCTION_NATIVE_STRATEGIES`
includes two of them, so the kit would judge a language on a strategy the dispatcher
cannot deliver.

## Conformance kit baseline — recorded 2026-10-09

`scripts/conformance_kit.sh` **exits 3** on a clean tree, because four checks
genuinely fail. Nothing in the repo said so until now, which made the kit
unusable as a gate: a reader could not tell a new failure from an expected one.
The baseline is the gate.

Run: `bash scripts/conformance_kit.sh --binary build/release/duckdb`
Scan: 3492 `DEF_TYPE` entries from 59 files, 31 languages; **28** languages, 69 fixtures.

> **Two scanners, two numbers, neither wrong.** The kit's own `.def` scan is
> line-oriented and reports **3492**; `scripts/audit_unreachable_declarations.py`
> handles the two things clang-format does to `src/language_adapters/sql_adapter.cpp`
> — several `DEF_TYPE` calls per line, and a long `raw_type` split across adjacent
> string literals that C concatenates — and reports **3698**. The second number is
> the one that equals what the extension actually loads: `SELECT count(*) FROM
> ast_type_map()` is 3698 exactly. Quote whichever you mean, and say which.

> **Scan count history.** 3876 when first recorded -> 3873 (#215 converted three
> typescript `DEF_TYPE` entries to `DEF_TYPE_ANON`, and the scanner's
> `/DEF_TYPE[ \t]*\(/` does not match `DEF_TYPE_ANON(`, so the kit neither trips
> over the new macro nor sees the 27 anonymous declarations it adds) -> **3817**
> (#222 deleted 56 typescript entries that were byte-identical duplicates of
> javascript ones and therefore dead under the initialiser list's first-wins
> rule). Making `DECL-UNIQUE` namedness-aware, and making the `.def` scan follow
> `#include`s, are both follow-ups; until then the anonymous table is outside
> the gate. **3817 -> 3794 -> 3492**: yaml's wiring (#227) added its `.def` to the
> scan, then #232 deleted 315 declarations for node types no grammar can emit.
> The shadowing itself is now gated by
> `scripts/audit_shadowed_declarations.py`, and **3817 -> 3791** is #222 part 2
> deleting the last 26 dead declarations (17 in typescript, 9 in javascript for
> node types the JavaScript grammar cannot emit).
>
> typescript's `NATIVE-ABST` denominator moved 214 -> 124 in that change (the
> kit reads the `.def` file rather than the include chain, so deleting dead
> duplicates shrinks what it counts), and the row disappeared entirely in part 2
> once the dead `variable_declarator NONE` line was gone.

```
TOTALS ok=189 FAIL=3 dirty=1 void=13 UNDECL=5 n/a=13 | languages=28
```

The three FAILs, all pre-existing and none caused by the kit:

| language | check | detail |
|---|---|---|
| kotlin | NATIVE-DECL | 4 of 6 function defs take parameters in source, **0 delivered** |
| lua | NATIVE-DECL | 12 of 14 function defs take parameters in source, **0 delivered** |
| sql | NATIVE-DECL | 2 of 2 function defs take parameters in source, **0 delivered** |

The NATIVE-DECL trio is one shape: the `.def` declares a function-shaped
native strategy and the extractor delivers nothing, so the declaration is
unbacked. Tracked as #220.

There was a fourth FAIL — `typescript NATIVE-ABST`, a node declared NONE that
carried payload anyway. It is **gone as of #222 part 2**, and it was never an
engine fault: the kit was reading `typescript_types.def`'s dead
`variable_declarator NONE` while javascript's `VARIABLE_WITH_TYPE` was the
declaration in force. Deleting the dead line removed the contradiction at its
source. Filed as #221, closed as a wrong diagnosis, and now structurally
impossible for that node.

Also expected, and neither pass nor fail:

- **`duckdb`: 5 × UNDECL.** The adapter wraps DuckDB's own parser and ships no
  `.def`, so no declaration source can answer. A result, not a harness
  problem (#197).
- **`kotlin` PARSE: dirty.** 1 ERROR/MISSING node of 799; `simple.kt:1`
  quarantined. The language is still judged on the rest.
- **12 × void.** No fixture, no clean fixture, or the corpus does not exercise
  the capability. Never read as a pass.

**yaml joined the matrix in #227** (`languages=27 -> 28`, `ok 184 -> 189`, two more
fixtures, `void 12 -> 13` and `n/a 11 -> 13`). It had existed as an adapter, a
`YAMLAdapter` declaration and a 43-entry `.def` for a long time while being
absent from `cmake/BuiltinLanguages.cmake`. Its rows are `ok` on PARSE,
NATIVE-ABST, NO-EMPTY, DECL-UNIQUE and DECL-RUNTIME; `n/a` on the two
call-naming checks and `void` on RAW-JOIN, because YAML has no calls -- which is
the capability gating working rather than a gap. `def scan` moved 3791 -> 3794
because its `.def` went from 43 entries to 46: six declared node types this
grammar cannot emit were removed, and nine it does emit were added.

A new FAIL, a new UNDECL, or a count that moves without an explanation in this
table is a regression. A control that stops firing voids the run outright —
`CONTROLS OK` is a precondition for reading the matrix at all, and the kit
exits 4 rather than 3 when one does not fire.

## Sequencing (RFC M1→M4)
M1 contract (here) → M2 in-tree core/ + languages/<lang>/ layout, macro split (→ v2.0.0-alpha)
→ M3 repo split via git filter-repo (→ v2.0.0) → M4 capabilities (WASM, splice/rewrite +
ast_rewrite [UNPARSE lands here, #157], grammar-language modules, incremental reparse, DuckPL).
Independent of the DuckDB-v2.0 / #160 planning-cost work (this is an architecture split on the
shipped DuckDB line).

~~NEXT ACTION: design the taxonomy spec schema (read semantic_types.hpp + node_config.hpp +
language_config_json.cpp in full to enumerate exactly what must round-trip), then write
generate_taxonomy.py and prove byte-equivalence before flipping the source of truth.~~
**Done — PR #195.**

~~NEXT ACTION (2026-10-08): deliverable 3, the **conformance kit** — pure test writing
against the current layout, no product risk, and it is what makes M2/M3 verifiable. It is
item #11 in `048`'s register. Deliverable 1 (module ABI) should be specified in parallel
but not built out, since the cross-module name resolver (`043` Part C) is the consumer
whose needs should shape it.~~
**Both done.** On the resolver caveat: `043` Part C already writes down what it needs from
the ABI — "Module-id rule + import-child shape are per-language -> live in
.def/native-extraction config, not SQL", on top of primitives `read_ast` already returns —
so specifying ABI 1.0.0 did not have to wait for an M4 epic. The ABI is semver'd
independently and a module declares the range it builds against, which is what absorbs the
resolver's additions later.

NEXT ACTION (2026-10-30): **M2** — the in-tree `core/` + `languages/<lang>/` layout
(`048` row 13), with the #92 invariance test as the safety net. Two seams to move, both
measured: `src/include/language_adapter.hpp` is 788 lines holding the 10-virtual interface
AND 27 concrete per-language adapter classes plus `DynamicLanguageAdapter` (`DuckDBAdapter`
is the 28th per-language adapter and lives in `duckdb_adapter.hpp`);
`src/include/native_context_extraction.hpp` is 679
lines holding the dispatcher AND the per-language rules (20 traits mappings, 14
`IsBareNameDefinitionParent`, 6 `IsBareNameDefinitionField`, plus python's three free
helpers). Every v1.17.0 name-binding fix landed in the second file, so until it is split
the most active line of work sits on the engine side of the seam.
