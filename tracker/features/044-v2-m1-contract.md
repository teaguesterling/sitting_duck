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
**All of (a)–(e) are done; the metric was met.** Deliverable 2 of M1 is complete; 1, 3, 4
and 5 remain.

## Conformance kit baseline — recorded 2026-10-09

`scripts/conformance_kit.sh` **exits 3** on a clean tree, because four checks
genuinely fail. Nothing in the repo said so until now, which made the kit
unusable as a gate: a reader could not tell a new failure from an expected one.
The baseline is the gate.

Run: `bash scripts/conformance_kit.sh --binary build/release/duckdb`
Scan: 3876 `DEF_TYPE` entries from 59 files, 31 languages; 27 languages, 67 fixtures.

```
TOTALS ok=183 FAIL=4 dirty=1 void=12 UNDECL=5 n/a=11 | languages=27
```

The four FAILs, all pre-existing and none caused by the kit:

| language | check | detail |
|---|---|---|
| kotlin | NATIVE-DECL | 4 of 6 function defs take parameters in source, **0 delivered** |
| lua | NATIVE-DECL | 12 of 14 function defs take parameters in source, **0 delivered** |
| sql | NATIVE-DECL | 2 of 2 function defs take parameters in source, **0 delivered** |
| typescript | NATIVE-ABST | 1 of 214 NONE-declared nodes carries native payload |

The NATIVE-DECL trio is one shape: the `.def` declares a function-shaped
native strategy and the extractor delivers nothing, so the declaration is
unbacked. NATIVE-ABST's single typescript row is the opposite direction — a
node declared NONE that carries payload anyway.

Also expected, and neither pass nor fail:

- **`duckdb`: 5 × UNDECL.** The adapter wraps DuckDB's own parser and ships no
  `.def`, so no declaration source can answer. A result, not a harness
  problem (#197).
- **`kotlin` PARSE: dirty.** 1 ERROR/MISSING node of 799; `simple.kt:1`
  quarantined. The language is still judged on the rest.
- **12 × void.** No fixture, no clean fixture, or the corpus does not exercise
  the capability. Never read as a pass.

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

NEXT ACTION (2026-10-08): deliverable 3, the **conformance kit** — pure test writing
against the current layout, no product risk, and it is what makes M2/M3 verifiable. It is
item #11 in `048`'s register. Deliverable 1 (module ABI) should be specified in parallel
but not built out, since the cross-module name resolver (`043` Part C) is the consumer
whose needs should shape it.
