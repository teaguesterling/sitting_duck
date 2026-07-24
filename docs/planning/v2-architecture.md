# RFC: sitting_duck v2.0 architecture — engine, languages, compositions

Status: DRAFT for review · Tracking: #87 · Prior art: #80 (runtime grammars), #92 (compile-time language selection), #86 (structural receiver), #71 (macro extraction)

## Goal

Refactor the monolithic extension into an engine (**sitting_duckling**), a languages
source tree (**sitting_duck_languages**), and compositions (**sitting_duck** proper,
duckling, on-demand packs) — so languages can be built, shipped, and created
independently, without multiplying repos or CI beyond what we can maintain.

## End state: 3 repos

| Repo | Identity | Ships |
|---|---|---|
| `sitting_duckling` | The engine AND the lightweight DIY foundation product (one identity — the engine's build output *is* the slim extension) | `sitting_duckling.duckdb_extension`: AST backend, registry + module ABI, core accessor macros, splice/rewrite, runtime grammar loader, **plus the native host-language modules** (duckdb SQL today; DuckPL when it exists) — no tree-sitter languages |
| `sitting_duck_languages` | Languages monorepo: source + recipes, **no release matrix** | Nothing by default. Each `languages/<lang>/` builds on demand to: a static lib (composition), a standalone pack extension, or a loadable grammar |
| `sitting_duck` | The existing repo, refitted as the batteries-included composition — keeps issues, docs, stars, and the community-extensions registration | The full 27-language extension (today's product), pinning duckling + languages as submodules |

Git submodules we own: 2 (duckling + languages, pinned by proper). Submodule refs we
don't own: ~29 upstream `tree-sitter-<lang>` repos referenced from languages
(replacing today's vendored copies — this *reduces* owned surface).

**Packs are build outputs, not repos.** We do not ship per-language extensions by
default. The DIY story: clone languages, `make <lang>.so`, `register_language` it —
or add it as a build module to your own duckling. One or two flagship packs get
built to prove the `INSTALL` path; demand pulls the rest.

## Language module anatomy

```
languages/<lang>/
  grammar/          # git submodule → upstream tree-sitter-<lang>
  parser/           # generated parser.c (or generated at build)
  adapter.cpp       # native semantic extraction — travels WITH the grammar
  <lang>.def        # semantic taxonomy mapping
  conformance/      # the module's conformance suite (below)
  module.cmake      # build declaration (evolution of cmake/BuiltinLanguages.cmake)
```

A module is the **complete** language implementation: an independently built module
has full native semantics, because grammar + adapter + config move together. (This
supersedes the earlier idea of keeping adapters in core — no degraded tier.)

**Module placement rule** (mirror of the macro rule): a language module lives in
duckling iff its parser depends only on the host engine — i.e. NATIVE-kind
adapters with no external grammar (duckdb SQL today; DuckPL when it exists).
External-grammar (tree-sitter) modules live in `sitting_duck_languages`, which
keeps that repo's anatomy uniform (every module there has a grammar submodule +
generated parser). A side effect worth having: a bare duckling can parse and
introspect the language it is embedded in.

## The contract (duckling's most important deliverable)

1. **Module ABI**: versioned C++ interface for adapters — registration, extraction
   strategies, flag/strategy enums. Semver'd independently of the extension version.
2. **Taxonomy spec**: the universal `semantic_type` system and flag layout as a
   versioned data artifact; enum tables and `language_config_json` name tables are
   **generated from the spec** (retires the hand-sync parity risk flagged in #80's
   review).
3. **Conformance kit**: generalization of the #91 test pattern — every module must
   prove: call nodes are named (`#name` binds), modifiers/signatures populate per
   its declared capabilities, raw-AST-join agreement, and no silent-empty behavior
   (#89 principle). A module either passes conformance or it isn't a module.
   Out-of-tree/third-party modules run the same kit.

## Four doors into one registry

| Door | Mechanism | Status |
|---|---|---|
| Compiled-in | `module.cmake` composition (today's model, per-language since #92) | shipped |
| Runtime `.so` | `register_language()` — #80 | in review |
| Pack extension | `INSTALL sitting_duck_<lang>` — a compiled module wrapped as its own DuckDB extension; rides DuckDB signing/distribution | flagship proof only |
| WASM grammar engine | opt-in engine module executing tree-sitter WASM grammars (benchmarked in #80 discussion: ~2× slower, +9MB) — enables **`create_parser(grammar, config)` with no toolchain**, and works where dlopen can't (incl. WASM builds) | v2.x capability |

## Macro layer: split between duckling and proper

**Split rule (not a judgment call):** a macro belongs in duckling iff it is
language-agnostic and depends only on the AST schema. Anything that depends on a
language module or embodies an analysis opinion is an add-on.

- **Duckling (core accessors)**: tree navigation (`ast_descendants`, `ast_ancestors`,
  `ast_children`, `ast_function_scope`), `ast_definition_parent`, semantic
  predicates (`is_function_definition`, …), schema helpers (`+schema` suffix
  machinery).
- **Proper (add-ons)**: the CSS selector layer (`ast_select`, `ast_select_from`) —
  which is *forced* out of duckling because it parses selectors with sitting_duck's
  own CSS grammar, i.e. it depends on a language module; pattern matching
  (`ast_match`); analysis macros (`ast_security_audit`, `ast_function_metrics`,
  `ast_dead_code`, call-graph macros, scope resolution).
- #71 (extraction to fledgling) then concerns specific add-ons, never the engine.
- The SQL surface consumed by fledgling/squackit is a compatibility contract:
  composition-level tests pin it.

## New capabilities (v2.x line, after the split)

1. **Grammar-language modules**: parsers for `grammar.js`/`grammar.json`, EBNF,
   ANTLR `.g4`, yacc — ordinary language modules (first stress test of the module
   system) and the front half of the on-demand pipeline.
2. **On-demand parsers**: `create_parser(grammar, metadata)` via the WASM engine —
   no `.so`, no toolchain. Direct grammar interpretation (Earley/GLR) stays
   parked as research; the WASM tier delivers the capability sooner.
3. **Unparse / rewrite**: **splice-based lossless rewriting** in duckling — nodes
   carry exact byte ranges, so reconstruction = original bytes + node-anchored
   edits; untouched code is byte-identical, only synthesized nodes need
   formatting. Language-agnostic by construction (no per-language
   pretty-printers). Composed with pattern matching this yields
   `ast_rewrite(source, pattern, replacement)` — codemods in SQL.
4. **Incremental reparse**: tree-sitter's edit-aware reparsing, exposed for
   edit-and-requery loops (agent workflows).
5. **New languages (DuckPL, …)**: the architecture's acceptance test — *one module,
   one day, conformance-green*, from the module template + onboarding doc.
6. **Interop: duck_blocks** (`ast_to_blocks`): render code structure as readable
   documents — definitions → headings (level from scope depth), bodies → code
   blocks, with style/tuning parameters. duck_blocks is a *spec, not a
   dependency* (conforming STRUCTs, no LOAD required), so this is a pure
   producer; duck_block_utils then supplies TOC/filter/validate and terminal
   rendering (`db_render_blocks`). Placement: add-on in proper, per the macro
   rule. The inverse already works for free: markdown code blocks →
   `parse_ast(content, attributes['language'])` — documents' code is queryable,
   code's structure is documentable. Exact-source extraction for code blocks
   rides in with the splice/rewrite work (item 3).

## Migration sequencing (each milestone shippable)

- **M1 — Contract extraction (in-tree, current repo).** Define module ABI +
  taxonomy spec + conformance kit against the existing layout. Generate the enum/
  name tables from the spec. Merge #80 here (the runtime door is part of the
  contract). **No files move.**
- **M2 — In-tree layout.** `core/` + `languages/<lang>/` directories, grammar
  submodule refs replace vendored copies, `module.cmake` per language. The #92
  invariance test (default build byte-equivalent, canonical registration order)
  is the safety net. Macro split lands here (core accessors vs add-ons).
  → tag **v2.0.0-alpha**.
- **M3 — Repo split + compositions.** `git filter-repo` extracts duckling and
  languages with per-directory history intact; proper pins both and builds the
  full extension + duckling targets. Duckling registers with community-extensions
  as a second descriptor. First flagship pack proves `INSTALL`.
  → tag **v2.0.0**.
- **M4 — Capabilities.** WASM engine, splice/rewrite + `ast_rewrite`,
  grammar-language modules, incremental reparse, DuckPL — as v2.x minors.

**Split-after-stability rule:** the repo split (M3) happens only once the M1
contract has stopped moving (measured: no ABI-breaking change for a full
milestone). Splitting earlier makes every interface iteration a three-repo dance
during exactly the phase when the interface churns most.

## Versioning & release trains

- Module ABI: semver'd in duckling; languages declares the ABI range it builds
  against; conformance runs against the pinned duckling in languages CI.
- The composition repo is the single lockstep point: "duckling X + languages Y =
  sitting_duck Z" is asserted and matrix-tested there. Packs inherit their
  compatibility claim from that assertion (answers #87's coupling question).
- v1.x remains the maintenance line until M3 tags v2.0.0.

## Risks & costs (accepted, with mitigations)

- **Cross-repo friction** (a core+adapter change = 2–3 PRs + submodule bumps):
  accepted as the price of independent builds; mitigated by contract-first
  sequencing, the conformance kit, and the split-after-stability rule.
- **WASM builds can't dlopen**: proper (batteries) remains the WASM answer; the
  WASM grammar engine is the eventual WASM-native extensibility path.
- **History/identity churn**: mitigated by keeping `sitting_duck` as the living
  repo (issues, docs, community-extensions ref) and extracting rather than
  replacing.
- **Silent scope creep into non-code formats** (logs, diffs): out of scope; the
  suite has duck_hunt/duck_tails for that. Grammar-language modules are in scope
  because they serve the on-demand pipeline.

## Open questions

1. Pack payload mechanics: statically embed the grammar in the pack extension
   (works everywhere, no dlopen) vs carry a `.so` asset through
   `register_language` (simpler, native-only). Decide at the M3 flagship pack.
2. Does `ast_match` (pattern matching) sit in duckling or proper? It is
   language-agnostic at engine level but analysis-flavored; default: proper,
   revisit if duckling users ask for it.
3. Conformance capability declarations: how a module states "I don't populate
   signatures" (e.g. bash) so the kit tests what's claimed rather than a fixed
   bar — needs a small capability manifest in `module.cmake` or the `.def`.
4. Whether `sitting_duck_languages` needs any CI beyond conformance-on-pinned-
   duckling (proposal: no — recipes are exercised by proper's matrix and the
   flagship pack).
