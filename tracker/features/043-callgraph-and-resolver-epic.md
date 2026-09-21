# 043 — Call-graph consolidation + cross-module name resolver (epic)

Status: SPEC / deferred. Not in scope for the current selectors-correctness round.
Consolidates #146, #152, #160, #164 (+ enables #147). Design settled 2026-09-16 with Teague.

## Naming decisions (settled)
- `:calls` / `:called-by` / `::callees` / `::callers` = **DIRECT only** (one hop).
- Transitive **call** closure = `:reaches` / `:reached-by` (pseudo-classes),
  `::reachable`-style pseudo-elements. Honest about being call-only; won't imply imports.
- `dependencies` / `dependents` = RESERVED for the general **resolution** closure
  (calls + imports + refs), NOT call-only. Only meaningful once the resolver exists —
  do not spend the name on call-closure (would over-promise: e.g. "which modules import
  this" is an import edge, not a call edge).
- Rejected for the transitive version: `chain` (linear — it branches), `tree` (acyclic —
  recursion/mutual recursion make it a cyclic graph), `stack` (one ordered runtime path —
  static analysis yields a deduplicated reachable SET, not a stack).

## Semantics established (probed on qualified_names.py / cg.py, 2026-09-16)
- `scope.function` = IMMEDIATE enclosing function scope: a call inside a lambda/nested def
  binds to the lambda/nested def, NOT the outer function. (This is #152.)
  So DIRECT = one `scope.function` hop; TRANSITIVE containment = walk the scope.function
  chain upward (recursive), or the subtree range scan.
- Current `:calls`/pe_callers matches on `call.name = def.name` GLOBALLY — a name-match
  approximation, not resolution: same-named defs across files collide, imports ignored.
  This is both the imprecision AND the O(n^2) blowup (#164) AND the bind tax (#160).

## Part A — call-graph rewrite (SQL, the near-term win; fixes #160 + #164)
- Rewrite DIRECT :calls/:called-by/::callees/::callers as a SEEDED join off the small
  matched set, using `scope.function` as the edge (equi-join), NOT the global name self-join.
  Seeded + bounded => removes #164 OOM and (out of hot path) the #160 bind tax.
- Take pe_callers/pe_callees OUT of the hot `ast_select_from` path. Because SQL macros
  always inline, a single wrapper that UNIONs both paths binds both — so the dispatch
  (has `::callers`/`::callees`/call-graph pseudo-class?) must live where only ONE macro is
  referenced per call: tooling/client picks the macro, or a C++ dispatch. Default path
  drops pe_callers/pe_callees entirely.
- Keep today's name-match semantics for DIRECT, but DOCUMENT them as "direct, name-matched
  (not resolved)" so the imprecision is explicit.

## Part B — transitive call closure (deferred; C++ unless bounded in SQL)
- `:reaches` / `:reached-by`: `WITH RECURSIVE` over scope.function edges with CYCLE
  handling (recursion/mutual recursion must terminate; UNION-distinct dedups). Bounded by
  scope-nesting/graph size. Risky on large graphs (#164 territory at one hop already) —
  defer to C++, or ship SQL only behind an explicit depth cap.

## Part C — cross-module resolver (the epic; C++; enables precise everything)
The general relation is the transitive closure of the REFERENCE -> DEFINITION edge; calls,
imports, and type/name refs are typed instances. Building it precisely = a symbol resolver.

Boundary (Teague): resolve only within the parsed corpus; a ref whose source module is not
in the table stays `external`/unresolved. Corollary: the DATA SOURCE that assembled the
corpus also supplies the module-name <-> file-path map (it knows package roots/layout).
sitting_duck provides the resolution machinery; the data source provides the layout. Same
seam as "external libs are the data source's problem."

Primitives already in read_ast (probed 2026-09-16):
- `qualified_name` = struct(semantic_type, name, index)[]  -- a LIST of typed path
  segments (rendered by ast_qualified_name_as_string, e.g. F[use], I[pkg_a]).
  Cross-module qualification = PREPEND a module segment to this list (structured, not string).
- `scope.module`, `is_exported(flags)`, `name_role(flags)`, `file_path`, `language`.
- Import statements carry the SOURCE MODULE in `name` (import_from_statement name='pkg_a');
  imported symbols/aliases are child nodes (aliased_import name='A' binds the alias).

Three derived relations:
1. modules(module_id, file_path)          -- canonical id per file; needs the package-root
                                              convention from the data source. THE crux.
2. exports(module_id, symbol, def_node_id) -- is_exported + name_role=definition.
3. imports(module_id, local_name, source_module_id, source_symbol) -- import stmts + children.

Resolution order for a ref `name` in module M:
  1) local scope chain (have via scope.*)  2) module-level def in M
  3) M's imports -> (source_module, symbol); in corpus -> exports lookup, else external
  4) qualified N.sym -> module N export     5) unresolved -> external
Output edge: resolved_ref(ref_node_id, def_node_id|NULL, via: local|module|import|qualified|external).
That single edge powers precise :calls, dependencies/dependents, :is-referenced (#147),
and name binding (#140).

Language-graded ("if possible"): clean for Python/JS/TS/Java/Go/Rust (path->module defined);
degrades to `external` for C/C++ (headers, macros). Module-id rule + import-child shape are
per-language -> live in .def/native-extraction config, not SQL.

Feasibility: relations 1-3 + the join are prototypable in SQL to validate on a real corpus;
precision (shadowing, re-exports, relative/wildcard imports) and scale push the real
implementation to C++. Module-id<->path map is the gating dependency (data-source seam).

## Sequencing
1. (this round, elsewhere) finish the near-term selector correctness fixes.
2. Part A — call-graph direct rewrite (SQL) — fixes #160 + #164. Highest ROI.
3. Part B — :reaches/:reached-by transitive (C++, or SQL w/ depth cap).
4. Part C — cross-module resolver (C++ epic) -> dependencies/dependents, precise :calls,
   #147, #140.

## #160 MEASUREMENT RETRACTION (2026-09-16) — probe harness unreliable
The text-surgery probe harness (asf_probe = sed-extracted ast_select_from body + swapped
terminals) became unreliable and I retract the confident localization above:
  - the extraction line range (77..1891) predated the :scope/:in-scope line-shift, so the
    later asf_lean/asf_cg macros failed to CREATE and produced ERROR-PATH times (~0.13s)
    that masqueraded as fast binds. The two-macro "dispatch works" result was measuring
    macro-not-found errors, not real binding. RETRACTED.
  - earlier "swap the terminal" isolation was also confounded: DuckDB binds ALL CTE
    definitions in a WITH regardless of whether the terminal references them (only the
    physical PLAN is pruned) — so terminal-swap measures plan-pruning, not bind cost.
WHAT REMAINS RELIABLE (measured on the INSTALLED macro, interleaved, min-of-N):
  - ast_select('.fn') EXPLAIN ~0.67s quiet / ~1.5-1.8s under 36-core load; read_ast ~0.11s;
    CSS parse ~0.13s => the macro carries ~0.4-0.5s of bind/optimize overhead. REAL.
  - CTE materialization is a NO-OP: byte-identical plan (diff empty), equal timing. REAL.
  - The ~5s the user sees is that ~0.5s amplified by contention. REAL.
NOT established reliably: WHICH part of the macro owns the ~0.5s (pe_callers/callees was the
  hypothesis but the deletion tests were not rigorously interleaved and the later probe was
  broken). And whether a two-macro split sheds it (the wrapper/constant-fold-pruning question
  is UNANSWERED — the test that "confirmed" it was measuring errors).
CORRECT METHOD (do not use the text-surgery probe again for this):
  localize + validate on the REAL macro with REAL builds — edit css_selectors.sql, rebuild,
  measure the INSTALLED ast_select EXPLAIN interleaved before/after, on a quiet box AND under
  induced load. Slower (rebuilds) but the only trustworthy signal. Alternatively use DuckDB's
  own profiling on the installed macro. The probe over-promised repeatedly this session.

## #160 VERIFIED RESULT (2026-09-16) — no single culprit; NO quick bind win
Redid the split with a VERIFIED harness: re-extracted ast_select_from (lines 77-1917) as
asf_full, CONFIRMED it matches installed ast_select_from (.fn=4=4, outer::callees=3=3).
Built asf_lean2 = asf_full minus pe_callers/pe_callees (CTE defs 1768-1797 + terminal
branches 1835-1840); CONFIRMED correct (.fn=4, ::parent-definition=1 both match installed,
::callees=0 as excised, 0 pe_callers refs). Interleaved bind-min x6, quiet box:
    asf_full('.fn')  = 0.63s
    asf_lean2('.fn') = 0.62s   => removing callers/callees saves ~10ms. NOT the cost.
This OVERTURNS the earlier "pe_callers/callees = the 0.5s" claim (that came from the broken
probe's error-path times). Combined with the other verified nulls:
    materialization ~0 | validations ~10ms | pe_callers/callees ~10ms
=> the ~0.6s macro bind is DISTRIBUTED across the ~84 CTEs, ~no single component owns it.
   Halving it needs cutting a large fraction of the macro (major rewrite) or moving to C++
   (no SQL bind of a giant body). There is NO cheap SQL win for #160.
IMPLICATION for the plan:
  - #160 (bind tax, ~0.5s/call, amplified to ~5s under contention): no quick fix. Either a
    substantial macro slimming or the C++ path. Do NOT keep hunting a one-CTE culprit.
  - #164 (exec OOM on big tables) is SEPARATE and still real: the direct-rewrite (seed off
    the matched set, join via scope.function instead of the global name self-join) fixes the
    EXECUTION blowup regardless of #160. Worth doing on its own merits.
  - The two-macro split does NOT help #160 (only ~10ms), so drop it as a #160 remedy; it may
    still be wanted for #164 exec isolation, but that's an exec argument, not a bind one.

## #160 VERIFIED DECOMPOSITION (2026-09-16) — it's 2 logic layers, NOT CTE count
Reliable method (verified asf_full == installed; err=0 on every variant; interleaved min-x4):
  terminal=sel / sel_root_raw : 0.14s  (parse + typed views — typed views are FREE)
  terminal=sel_root           : 0.19s  (+0.05 re-rooting)
  terminal=sel_props          : 0.41s  (+0.22 VALIDATIONS + left/right selector decomposition)
  terminal=matched_base       : 0.39s  (5-arm UNION adds ~0)
  terminal=matched            : 0.62s  (+0.21 matched_raw: the pseudo-class/:has/:not/attr CASE)
  terminal=full               : 0.65s  (+0.03 pe_* branches)
Also: injecting 40 dummy sel-projecting CTEs changed bind by ~0 (0.66->0.62) => CTE COUNT is
NOT the lever; unreferenced/typed-view CTEs are pruned/cheap. Teague's "maybe not 84 CTEs"
tested: consolidating CTE count won't help.
THE COST is two expression-heavy layers, ~0.2s each:
  1. sel_props body: the ~10 validation guards folded into validations_ok + the left/right
     selector decomposition (left_class/left_id/right props).
  2. matched_raw: the giant `CASE pc.pseudo_name WHEN ...(~30 branches, each w/ subqueries)
     END` inside NOT EXISTS, plus :has/:not/attribute filters.
So a #160 rewrite must SIMPLIFY THESE TWO EXPRESSIONS (fewer/cheaper subqueries in the
validation fold and the pseudo-class dispatch), not cut CTE count and not split out pe_*.
Still a real rewrite, not a quick win — but now the target is known and measured.
Supersedes all earlier #160 localizations this session (materialization/validations/
pe_callers claims came from a broken probe; these numbers are verified).

## #164 + #152 SHIPPED (2026-09-16) — PR #167 (bounded call-graph via scope.function)
:called-by / :calls rewritten from range self-joins to bounded scope.function equi-joins.
- #164 (OOM): `.call:called-by(name)` was ~17.8M-row intermediate → 59s/OOM on 78k nodes.
  Now ~1.2s / ~0.55GB under a 2GB cap on 150k nodes. Verified zero + non-zero results.
- #152 (lambda): Teague ruled a call inside a lambda inside F is called-by the LAMBDA.
  scope.function counts the lambda; old range walk filtered DEFINITION_FUNCTION and skipped
  lambdas. :calls now direct too (old subtree scan was transitive, contradicting docs).
- Still name-matched, not resolved (documented). :is-called/:is-referenced (name self-joins)
  left as-is — run bounded under a memory cap, keep their tests. Their name-match imprecision
  is resolver-epic (Part C) territory, not this fix.
- Tests: callgraph_direct.py fixture; ast_select_pseudo_classes 66, multilang 463, green.
Part A status: the DIRECT rewrite is DONE for :called-by/:calls (#164 exec + #152). Still
OPEN in this epic: #160 (the ~0.5s BIND tax — separate from exec, no quick win, see the
verified decomposition above: sel_props ~0.22s + matched_raw ~0.21s), :reaches/:reached-by
transitive (Part B), and the cross-module resolver (Part C).

## #160 FULLY ISOLATED (2026-09-16, verified harness, err-checked, interleaved min)
Re-confirmed the decomposition and drilled into sel_props:
  sel (floor)                    0.14s
  sel_props (full)               0.41s   (+0.27 over sel)
  sel_props, validations pruned  0.18s   => left/right selector decomposition ~0.04s (cheap)
  sel_props, 1 validation only   0.21s   => ~LINEAR: ~0.025s per validation CTE
  matched (adds matched_raw)     0.59s   (+0.18: the ~30-branch pseudo-class/:has/:not/attr CASE)
  full (adds pe_*)               0.66s   (+0.07)
So the ~0.5s fixable bind splits: VALIDATIONS ~0.23s (9 guard CTEs, each a correlated EXISTS
over pseudo_classes/attr_conditions, linear in count ~0.025s each) + matched_raw CASE ~0.18s
+ re-rooting ~0.05s + decomposition ~0.04s. (The earlier "validations=10ms" was the broken
probe; this verified number is ~0.23s. Typed views/CTE count still free; materialization NO-OP.)
FIX PATHS (validations, ~0.23s, the cleaner target — defensive, don't affect matching):
  (a) SQL: consolidate the 9 validation CTEs into fewer passes (several scan the SAME
      pseudo_classes/attr_conditions — compute their flags in ONE CTE). Linear cost => cutting
      9 checks to ~3 could save ~0.15s. Fiddly but low-risk; must keep every guard raising
      (malformed_selectors.test + the scope/in-scope guards).
  (b) C++: a single opaque ast_validate_selector(selector) scalar FUNCTION (not a SQL macro —
      macros inline and don't help, proven) removes the whole 0.23s from the plan. Best, needs C++.
matched_raw CASE (~0.18s): simplify the ~30-branch pseudo-class dispatch, or C++. Harder.
STATUS: diagnosis complete + verified. The rewrite itself is a real multi-build-cycle effort;
do it as a focused piece with the verified-harness discipline (re-extract from current main,
confirm ==installed, EXPLAIN-min before/after quiet AND under load, full suite).
