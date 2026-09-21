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
   risk). ← THE FIRST SLICE (below).
3. **Conformance kit** — generalize the #91 pattern: every module proves call nodes are named
   (#name binds), modifiers/signatures populate per declared capabilities, raw-AST-join
   agreement, and no silent-empty (#89). Out-of-tree modules run the same kit.
4. **Trust-boundary doc** — write the register_language (#80) security model into the contract
   (settled: default-off `sitting_duck_enable_runtime_grammars`, allowed_directories; no
   pre-exec vetting of a .so; no privilege escalation beyond SET/LOAD).
5. **Merge #80** — the runtime `.so` door is part of the contract.

Safety net for later (M2): the #92 invariance test — default build byte-equivalent, canonical
registration order — must stay green throughout.

## First slice: taxonomy-spec → codegen (concrete, self-justifying)
Retires the .def ↔ C++ drift that has caused real bugs (e.g. flag-name/enum sync). Today these
are hand-maintained in parallel:
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

## Sequencing (RFC M1→M4)
M1 contract (here) → M2 in-tree core/ + languages/<lang>/ layout, macro split (→ v2.0.0-alpha)
→ M3 repo split via git filter-repo (→ v2.0.0) → M4 capabilities (WASM, splice/rewrite +
ast_rewrite [UNPARSE lands here, #157], grammar-language modules, incremental reparse, DuckPL).
Independent of the DuckDB-v2.0 / #160 planning-cost work (this is an architecture split on the
shipped DuckDB line).

NEXT ACTION: design the taxonomy spec schema (read semantic_types.hpp + node_config.hpp +
language_config_json.cpp in full to enumerate exactly what must round-trip), then write
generate_taxonomy.py and prove byte-equivalence before flipping the source of truth.
