# 042 — Getting `ast_select` selectors right (correctness plan)

**As of 2026-09-15.** Tracks the whole selector-correctness effort, not one bug.

## Where we are
- **Merged:** alias fixes — `.access` (#135), `.comment` narrow (#134/#142), flag-doc
  (#102), `is_boolean_literal` stub (#132); **IS_CONSTITUENT** over-match + bash `.call`
  (#139 / #144). Exhaustive alias-coverage oracle test in place.
- **In-flight:** **#129** (combinators: #127/#130/#133) — rebased onto current main,
  builds clean, its own test green (28 assertions); incidentally fixes a *silent
  under-match* in the standalone `.class` arm (`.str` py 78→80 = the true string count).
  Ready to finalize + merge.

## Open selector bugs, by subsystem

| Subsystem | Issues | Status |
|---|---|---|
| **Combinators / structure** | #127 (aliases in steps, 3+ chains), #130 (`>` ignores file_path), #133 (`:has`/`:not` match syntax-only), #141 (`+` counts punctuation) | #127/#130/#133 in #129; #141 verify post-merge |
| **Validation / matching** | #128 (malformed selectors silently over-match / return 0), #151 (bare *type* selector is a prefix match: `lambda` also selects `lambda_parameters`) | open |
| **Pseudo-classes** | #145 (`:scope(...)` silently ignored), #146 (`:calls(name)` not scope-aware), #147 (`:is-referenced` matches every fn), #148 (`:exported` wrong level) | open (new) |
| **Attributes** | #149 (`[receiver=X]` collapses chained receivers), #150 (tutorial attr-filter example errors) | open (new) |
| **Semantic typing / names** | #131 (Python operators typed by token text, double-match), #140 (name binding: bash/Go/Java), #139 case-1 (C++ `.fn` declarator, Go `.mod`) | open / deferred |

## Root-cause themes (why these keep recurring)
1. **Silent failure is the meta-bug.** #128, #145, #147, #133, and the under-matches all
   *quietly* return the wrong set instead of erroring. #89 already set the contract
   ("can't answer → raise, never silent empty"); it isn't enforced across the surface.
2. **Scope-awareness is half-wired.** #145/#146/#148 need the tree/scope model
   (`scope` column, `IS_SCOPE`, ancestor ranges) that exists but isn't threaded into the
   pseudo-classes.
3. **Name binding is per-language and incomplete** (#140, and #146/#149 depend on it).
4. **Semantic typing is per-language and incomplete** (#131, #139 case-1; IS_CONSTITUENT
   was the first pass).
5. **Divergent match paths.** #129 showed the standalone vs combinator arms disagree
   (the 78→80 under-match). One selector, several code paths, inconsistent results.
6. **Count-based tests are fragile.** Magic-number expectations encode whatever the engine
   did, bug or not (our own `.str`=78 encoded a bug). We need oracle/invariant tests.

## Plan (phased; each phase ≈ one PR unless noted)

### Phase 0 — combinator cluster
- #129 MERGED (main e3d89c2). **Verified on merged main:** it cleared **#127** (closed;
  3+ chains now raise, aliases honored) and **#130** (closed). It did **NOT** clear #133
  or #141 (I assumed it would — wrong; verified empirically):
  - **#133 still open** — `.fn:has(.fn)` = 20 = every function though sample_app.py has 0
    nested functions; the syntax-only `def` token is still counted. #129 filtered the
    anchor/ancestor but not the `:has` *has-target*. Fix: apply
    `NOT is_syntax_only AND NOT is_constituent` to the node matched inside `:has`/`:not(:has)`.
  - **#141 still open** — `.id + .id` on `[a, b, c]` = 0; commas are counted as the adjacent
    sibling. Fix: adjacency must skip punctuation / syntax-only (and constituent) siblings.
  Both are small css_selectors.sql fixes, now unblocked on clean main. Do them to actually
  close the combinator cluster (they also fold in the #139 `:has` `NOT is_constituent`
  follow-up).

### Phase 1 — Validation layer (kills the meta-bug; highest leverage)
- #128: malformed / unknown selectors **raise** with a clear message instead of silently
  over-matching or returning 0. Extend the #89 "can't answer → raise" guard to: unknown
  pseudo-class, unknown attribute op, unparseable selector, and a pseudo-class whose
  precondition isn't met (feeds #145/#147).
- This is a chokepoint fix: it converts several "silent wrong" bugs into loud errors and
  prevents the class from recurring.

**#128 implementation design (from orientation 2026-09-15):**
The engine parses the selector text with `parse_ast_list_table(selector,'css')` (scalar,
column-ref capable) and picks the first non-`stylesheet`/non-`ERROR` node (css_selectors.sql
~L174-184). A **bare selector always sits under ONE expected top-level `ERROR` wrapper** —
that must not be flagged. Two malformed signatures, confirmed on fixtures:
  - *Extra/interior `ERROR`*: `.fn[WHERE …]` and `.fn#main extra junk!!` show **2** ERROR
    nodes (count > 1, or an ERROR not at the top-level wrapper position).
  - *Stray tokens, NO extra ERROR*: `.fn[name="main"` (loose `[ identifier = …`) and
    `.fn:nonsense(` (dangling `(`) keep ERROR count = 1 but leave **extra sibling tokens**
    under the wrapper beside the one recognized selector node (a bare `[`/`(`/`]`/`)` not
    enclosed in an `attribute_selector`/`pseudo_class_selector`/arguments node).
Detection (add `parse_completeness_validation` to `validations_ok`), pick one:
  - **(pragmatic, no columns needed)** malformed IFF `count(ERROR|MISSING) > 1` OR the
    top-level wrapper has >1 recognized-selector child / any stray bracket-paren token not
    inside a recognized structure. `error()` names the offending fragment (its `peek`).
  - **(robust)** extend `parse_ast_list_table` to accept `source:='full'` (it currently
    rejects it — 2-arg only), then require the recognized node's `[start,end]` to cover the
    trimmed input; error naming the unconsumed tail. Cleaner but a wider change.
Note `>>`/`.fn >> .call` may already be covered by #129's combinator validation — verify
after merge. Tests: each #128 case asserts a specific error (statement error), #89 style.

**#151 — DESIGN DECISION, not a clear bug (needs Teague).** The type filter is
`a.type = sp.type_filter OR a.type LIKE (type_filter || '_%')` (css_selectors.sql
L1087/L1126). The `LIKE` prefix match is **documented and intentional**:
`docs/reference/node-type-selectors.md` → "Three Tiers of Type Selectors" defines a bare
type (`if`) as a *prefix* match (`if`, `if_statement`, `if_clause`) — the middle tier —
with exact types as the narrow tier. So `lambda` → `lambda_parameters` is working as
designed. The #151 reporter (duckent differential) wants **CSS-standard exact** matching.
This is a genuine conflict, so DO NOT just delete the `LIKE` arm. Options for Teague:
  (a) keep the prefix tier; close #151 as WAI + document louder; add an explicit
      **exact-match syntax** (currently there is NO way to say "exactly `if`, not
      `if_*`" — the bare selector is always prefix, which is the real gap);
  (b) flip bare type to exact (CSS-standard) and move prefix behind an explicit
      wildcard (`if*` / `if_`) — cleaner CSS semantics but a documented-behavior break;
  (c) keep prefix default, add exact syntax.
Legit sub-point regardless of choice: a non-type prefix like `lambd` silently matching
`lambda*` is surprising — overlaps #128 (unknown bare type → refuse/warn). Decide (a/b/c)
before any code; likely its own PR, not batched blindly with #128.

### Phase 2 — Pseudo-class semantics (shared scope/reference model)
- #145 `:scope(selector)` — actually constrain to the scope subtree (currently ignored).
- #146 `:calls(name)` — scope-aware containment, not whole-subtree.
- #147 `:is-referenced` — real reference check, not "every definition".
- #148 `:exported` — module-level only (use `IS_EXPORTED` + scope depth, exclude
  methods/nested).
- Do together: they share the ancestor-range / `scope` / `IS_EXPORTED` machinery.

### Phase 3 — Attribute selectors
- #149 `[receiver=X]` — keep the full chained receiver (`self.db.execute` ≠ `execute`);
  ties into #86's receiver field.
- #150 — fix the tutorial's "Putting It All Together" attribute-filter example (verify it
  runs; likely a doc + attribute-parse fix, related to #128 validation).

### Phase 4 — Semantic typing & name binding (per-language `.def`/native audits)
- #131 Python operators typed by token text (`@`→`.arith`, double-match) — retype in the
  Python grammar mapping.
- #140 name binding — bash command names (the `NONE` left by #144), Go `type_spec`,
  Java import names. New/extended name strategies.
- #139 case-1 — C++ `.fn` declarator + Go `.mod` doubling: the context-dependent case
  IS_CONSTITUENT couldn't express; needs a selector-level "primary node" rule.

## Cross-cutting: the durable fix is the test strategy
The bugs recur because tests pin magic counts. Adopt, alongside the fixes:
- **Oracle/invariant tests** (like `semantic_type_alias_coverage.test`): assert *relationships*
  (`.str` ⊆ string literals; every `.class` match is non-constituent) not raw counts.
- **A "no silent failure" contract test** per pseudo-class / combinator / attribute: a
  malformed or unanswerable selector must raise; an answerable one must be non-empty on a
  fixture that clearly contains matches.
- **Pseudo-identity (unparse, tracker/041)** as a structural sanity net for typing changes.
- Keep counts where useful, but always paired with an invariant that explains the count.

## Suggested order
0 (land #129) → 1 (validation) → 2 (pseudo-classes) → 3 (attributes) → 4 (typing/names).
Phase 1 first because it's the chokepoint that stops silent regressions while the rest lands.

## Unparse (#157) — DEFERRED until all selector issues resolved (2026-09-15)
Per Teague: handle unparse after everything else. #157 stays as the discussion
artifact (table-driven prototype + README; unparse rules are a separate
per-language tag-keyed lookup, NOT semantic flags). When picked up, the agreed
direction is a **pluggable JSON policy** (`ast_unparse(path, policy := NULL)`,
C++ default rendered to JSON) modeled on Topiary's concepts (@append_space /
@indent / @hardline). tree-sitter core provides no formatting default. Do NOT
build/promote until #151 + Phases 2–4 are done.

## Sequencing (current)
#156 (#128) merge on green → #151 (exact bare types + enable [type^=]) → Phase 2
(#145–#148 pseudo-classes) → Phase 3 (#149/#150 attributes) → Phase 4 (#131/#140/
#139-case-1) → THEN unparse. All selector fixes touch css_selectors.sql, so they
serialize behind each merge.

## New issues (2026-09-15 pm)
- #152 — `:called-by` ignores lambdas as the nearest enclosing function (lambdas are
  DEFINITION_FUNCTION but skipped). → Phase 2 (pseudo-classes, with #145–#148).
- #158 — C++ `template_function`/`template_method` USE-sites are classified as function
  DEFINITIONS (should be calls/uses). → Phase 4 (semantic typing, with #131/#139/#140).
- #159 — FEATURE: `:templated` pseudo-class to select/exclude templated C++ definitions.
  Depends on #158 (get the template typing right first); schedule after Phase 4 typing.

## Performance track (separate from correctness)
- #160 — ast_select_from spends ~6 s PLANNING per call regardless of table size or
  selector. Sibling of tracker/bugs/040 (v2.0 planning-cost regression). This is what
  slows the whole test suite (~15-20 s/ast_select locally). Prime suspects: the combinator
  UNION ALL arms decorrelating into always-on hash joins (see the sel_props/validations_ok
  comments), and the many correlated scalar subqueries in sel_props. Own effort; profile
  with EXPLAIN ANALYZE. NOT part of the correctness phases; schedule deliberately.

## #160 investigation result (2026-09-15) — it's the v2.0 planning regression
Measured on the CURRENT build (DuckDB v1.5.5, the shipped/pinned version):
  - ast_select('.fn') tiny=0.42s, sample_app=0.47s (input-independent → ~0.3s planning)
  - ast_select_from('.fn')=0.44s; ast_select_from('.class .fn')=0.44s (combinator, same)
  - LOAD-only=0.11s, read_ast=0.14s baseline
  So on v1.5.5 a call plans in ~0.3-0.4s, NOT ~6s. Earlier this session the SAME calls
  took 15-20s each — that was my v2.0-dev submodule build (e3946f2327).
CONCLUSION: #160's ~6s is the DuckDB v2.0-dev optimizer/planning regression (= tracker/
  bugs/040), reproducing only on the v2.0 line; the shipped v1.5.5 is fine. The macro
  plan itself is clean on v1.5.5 (26 operators, 3 hash joins, no cross/delim joins).
  Not a current-release blocker. Fix paths (only matter for the v2.0 migration):
  (a) report the optimizer regression upstream to DuckDB; (b) restructure the ast_select
  macro to a shape the v2.0 optimizer plans faster (needs profiling ON a v2.0 build —
  EXPLAIN ANALYZE the decorrelation of the UNION ALL combinator arms; currently OOM-
  blocked here). Recommend: note #160 as v2.0-only on the issue; defer the macro
  restructure to the v2.0-migration effort.

## #145 Tier 1 done (2026-09-16) — :scope / :in-scope split — PR #163
Design changed mid-implementation on Teague's call: the pre-existing :scope
conflated "IS a scope" (bare :scope = is_scope) with "is IN a scope"
(:scope(type) = ancestor-walk containment). Split along the CSS grain:
  - :scope[(kind|.class|type)][#name]  — the node IS a scope boundary (is_scope
    + optional semantic-kind / exact-type / #name filter). CSS-consistent.
  - :in-scope(kind|.class|type)[#name] — the node is CONTAINED WITHIN a scope.
    function/class/module via the precomputed scope.* struct; bare tree-sitter
    types via the ancestor walk (= the OLD :scope(type) behavior, renamed).
BREAKING: :scope(type) containment → :in-scope(type). Migrated docs +
return_statement scope test.
Loud errors (anti-silent-empty): .class not in {function,class,module} → #145
Tier 2; #name on keyword/type form (grammar lumps `function#foo` into one
plain_value — only .fn#foo works); bare :in-scope with no arg. Added
sel_pcs_first_plain_value + pseudo_arg_plain to detect the footgun.
Verified: ast_select_pseudo_classes.test 60 assertions; multilang 463 green.
Counts grounded on read_ast+scope.* (qualified_names.py): :scope(function/
class/module)=6/3/1, bare :scope=10; :in-scope(function/class/module)=107/99/132.
Tier 2 (arbitrary semantic scopes, full nested selectors) + :is()/:where()
(need a nested selector-list evaluator; identical w/o CSS specificity) still
open under #145.

Merges this session (final): #162 (the #155 selector_for.sql exact-type patch)
merged. Only open PRs now: #163 (this) + #157 (unparse, parked).

## #160 CORRECTION (2026-09-16) — it IS a current-release cost, ~2.5s PLANNING on v1.5.5
Re-measured cleanly with /usr/bin/time on the whole process (not .timer), on the
SHIPPED build (v1.5.5-dev262 d8cdaa33fd), box only lightly loaded (read_ast=0.56s):
  - LOAD only ............................ ~0.5s
  - read_ast(simple.py) trivial query ... ~0.6s   (LOAD + ~0.05s)
  - parse_ast_list_table(sel,'css') ..... ~0.64s  (CSS parse is NOT the cost)
  - ast_select('.fn')  EXPLAIN-only ..... ~2.6s   (PLANNING)
  - ast_select('.fn')  full run ......... ~2.6s   (EXPLAIN ≈ full run → ~0 exec)
  - planning is FIXED vs selector: 'if'=3.3s, '.fn'=2.6s, '.class .fn'=2.8s,
    '.class .fn:has():not()'=3.4s. Even '.fn' plans ~116 operators.
WHY the earlier "0.4s on v1.5.5, fine" note was wrong: it measured EXECUTION
(~0.4s) and mistook it for total. Planning (~2.1s) was invisible to that method.
The clean EXPLAIN-only number is authoritative: ~2.5s PLANNING per ast_select.
So #160 is NOT v2.0-only — it reproduces on shipped v1.5.5. (v2.0 makes it worse,
~6-20s, = bugs/040, but v1.5.5 already pays ~2.5s.) Reclassify #160 as a
CURRENT-RELEASE planning-cost issue, not deferred-to-migration.

Root cause: the ast_select macro is ONE static SQL body with 84 CTEs + a 5-arm
UNION ALL (matched_base) + dozens of correlated scalar subqueries (validations_ok,
the pseudo-class CASE). SQL macros are static, so the ENTIRE structure is bound and
optimized on every call regardless of what the selector uses — a bare `.fn` pays
for the combinator arms, :has machinery, and all validations being in the plan.
"Abuse of UNION ALL" is one contributor, NOT the whole story: 84 CTEs + correlated
subqueries are always-planned too.
NEXT (no rebuild needed): bisect via runtime `CREATE OR REPLACE MACRO ast_select_probe
AS TABLE <modified body>` overrides — strip the combinator arms, then validations_ok,
then the pseudo-class subqueries — and EXPLAIN-time each to find the dominant term
before restructuring. Candidate fixes: collapse the 5 UNION arms; fold the ~10
validation CTEs into fewer / make them non-correlated; reduce the typed sub-CTE fan-out.
Endgame option if SQL can't get under ~0.5s: move selector→plan into a C++ table
function that emits only the needed arm.

## #160 BISECTION (2026-09-16) — it is NOT UNION-ALL abuse
Rebuild-free bisection: extracted ast_select_from body as `asf_probe`, swapped only
the terminal SELECT so DuckDB prunes unreferenced CTEs; EXPLAIN-timed each (v1.5.5,
/usr/bin/time whole-process, source=pre-materialized read_ast table, selector='.fn').
  terminal target        planning   incremental attribution
  sel / ast / sel_root_raw  ~0.65s   floor (LOAD+read_ast+CSS parse+typed views)
  sel_props                 ~1.70s   +1.05s  validations (10 guard CTEs, correlated
                                             subqueries) + left/right sel_props decomp
  matched_base              ~1.95s   +0.25s  the 5-arm UNION ALL  <-- NOT the culprit
  matched (matched_raw)     ~2.7-3.5 +~1.0s  :has/:not/attr/pseudo-class filter machinery
  full 8-way UNION          ~3.33s   +~0.5s  7 pseudo-element CTEs (pe_*) + 8-way UNION
Typed views are ~free (sel_root_raw=0.68s ~= floor). The 5-arm matched_base UNION is
only ~0.25s, so "abuse of UNION ALL" is NOT the cause. The three real terms:
  (1) validations/sel_props ~1.05s  (2) matched_raw filters ~1.0s  (3) pe_* CTEs ~0.5s
All are STATICALLY planned every call (SQL macro), regardless of selector.
Fix priority (value x risk):
  A. validations (~1.05s, top term, purely defensive guards — don't affect matching):
     consolidate the ~10 correlated-subquery validation CTEs into fewer / non-correlated
     passes, or push selector-shape validation into the C++ parse step. HIGH value, LOW
     semantic risk (errors only).
  B. pseudo-element pe_* CTEs (~0.5s): only 1 of 8 UNION branches ever returns (guarded
     by element_name='x'); all 8 planned always. Gate so pe_* aren't planned when the
     selector has no '::pseudo-element'. MECHANICAL, LOW risk.
  C. matched_raw filters (~1.0s): harder — the :has/:not/attr/pseudo machinery. Reduce
     correlated NOT EXISTS count; defer.
On v2.0 (bugs/040) every term inflates (6-20s), so the restructure helps both lines.
Measurement caveat: EXPLAIN planning-time is noisy (+/-0.3s, contended box); ordering
is robust and the 0.25s UNION result is well-separated.

## #160 CORRECTION #2 (2026-09-16) — materialization is a NO-OP; earlier timings were contention
I mis-diagnosed. Two hard, contention-controlled findings supersede the bisection above:
  1. `AS MATERIALIZED` on the 5 hot CTEs (pseudo_classes, attr_conditions,
     sel_pseudo_classes, sel_pcs_to_args, sel_arg_blocks) produces a BYTE-IDENTICAL plan
     (diff empty) and IDENTICAL timing — interleaved on a quiet box (MAT 0.7s / OLD 0.7s)
     AND under 36-core induced load (both ~1.3-1.8s). DuckDB v1.5.5 already reuses these
     multiply-referenced CTEs, so manual MATERIALIZED does nothing. REVERTED, not shipped.
  2. The earlier "0.67 vs 3.33s" bisection numbers were CONTENTION drift between rounds,
     not structure. Interleaved same-condition runs of the old (inlined) vs materialized
     macro are equal. On a QUIET box the real ast_select plans in ~0.7s total (LOAD ~0.5s
     + read_ast + parse; macro planning itself ~0.15s). The 2.7s I measured at session
     start was because the box was busy (I'd been building/testing).
CONTENTION-INDEPENDENT metric (plan row count, EXPLAIN of asf_probe '.fn', table source):
    sel=45  sel_props=405 (+360)  matched_base=428 (+23)  matched=428  full=428
  => ~94% of plan complexity is sel_props (the ~10 validation CTEs of correlated EXISTS +
     left/right selector decomposition). matched_base UNION arms add ~23 ops. The pe_*
     pseudo-element CTEs add ~0 for a selector with no '::' (constant-folded out). So the
     pe_* gating idea (B) and UNION-collapse (matched_base) are NOT worth much — the plan
     is dominated by sel_props.
HONEST STATUS: there is NO quick 1-line win. The 5s the user sees is CONTENTION on a busy
machine, AMPLIFIED by a large ~428-op plan whose cost is ~94% validations. The only real
lever is REDUCING the validation operator count (fold the 10 validation CTEs into fewer /
fewer correlated subqueries, or skip validation for simple selectors) — a genuine
structural refactor that must keep all guards raising (malformed_selectors.test), and whose
payoff must be proven with UNDER-LOAD timing, not quiet-box or single-round EXPLAIN.
Metric to optimize: plan row count (contention-independent), target sel_props << 405.

## #160 STABLE NUMBERS (2026-09-16) — corrected metric
My "428/405 operators" was TEXT-ROW count of EXPLAIN output, inflated by huge inline
CASE/error() expression text (lines up to 240 chars; 2829 total rows). WRONG metric.
Accurate, warm-cache, quiet box:
  - physical plan for '.fn' = 80 operator boxes (15 CTE, 14 CTE_SCAN, 13 FILTER,
    27 PROJECTION, 2 HASH_JOIN, 2 READ_AST). Modest, not "hundreds".
  - EXPLAIN ast_select('.fn') = 0.67s ; EXPLAIN read_ast-only = 0.11s (warm LOAD)
    => ~0.56s is the macro's own bind+optimize (CSS parse + 84 CTEs + validations),
    producing the 80-op plan, for a selector that uses almost none of it.
So the real per-call planning overhead is ~0.56s on a quiet/warm box (NOT 0.15s, NOT
2.7s). Under CPU contention this 0.56s is what balloons to the user's ~5s.
Genuinely wasteful part (user: "trivial .fn being that big is insane" — fair): the
static macro binds+optimizes ALL validations + 5 match arms + selector decomposition
for every call, and for '.fn' ~all of it is dead. 80 ops survive; far more is chewed
and discarded.
BEST fix direction (revised): move selector VALIDATION into C++ — parse_ast_list already
parses the selector in C++, so a scalar ast_validate_selector(selector) (raise-or-true)
could replace the ~10 SQL validation CTEs that dominate the bind cost, collapsing them to
one function call. Cuts both operators and the ~0.56s. Bigger than a 1-liner; needs C++.
Materialization: confirmed no-op, do not pursue.

## #160 ROOT CAUSE FOUND (2026-09-16) — pe_callers/pe_callees BIND tax; ties to #164
Bisected the ~0.5s planning by DELETING CTE groups from the probe prefix and timing
EXPLAIN (warm, interleaved, min-of-N — contention-robust). Results, each dropping ~to floor:
  full macro (all CTEs) ............................ ~0.60s
  drop matched_raw + matched + all 7 pe_* .......... 0.11s
  keep matched_raw, drop only the 7 pe_* ........... 0.12s
  keep pe_parent..pe_prev, drop only pe_callers+pe_callees .. 0.12s
=> The ENTIRE ~0.5s planning tax is binding pe_callers + pe_callees (the ::callers /
   ::callees call-graph pseudo-element CTEs), on EVERY ast_select call, even for '.fn'
   which has no '::'. Everything else (parse=0.13s, read_ast=0.11s, matched_raw filters,
   validations, matched_base UNION, pe_parent..pe_prev) is ~floor.
RULED OUT (all measured, all wrong hypotheses this session):
  - CTE materialization: byte-identical plan, no effect (DuckDB v1.5.5 already reuses CTEs).
  - validations: removing ALL of them saves ~10ms.
  - the CSS parse: 0.13s.
  - operator count / plan size: flat (27-80 ops) regardless — planning cost is BIND work
    over the macro body, not final plan size. Only warm interleaved TIME is a faithful metric.
pe_callers body: `matched m JOIN ast call_node ON call_node.name=m.name AND semantic_type=CALL
  JOIN ast caller_fn ON caller_fn.node_id=call_node.scope.function`. An unfiltered name=name
  self-join over the full wide `ast` schema (nested structs) — expensive to BIND (wide-schema
  + struct type resolution, 2 ast joins) AND the same shape is #164's EXECUTION blowup.
TIES TO #164: #164 = call-graph PSEUDO-CLASSES (:called-by/:calls) OOM/59s on 78k nodes.
  Same subsystem: the name=name self-join is O(n^2) at execution. So the call graph is the
  common root of #160 (bind tax on all queries) and #164 (exec OOM on big tables).
FIX OPTIONS (need Teague's call — architectural):
  1. Route pe_* out of the hot path. Clean elimination needs the dispatch OUTSIDE one SQL
     macro (SQL macros always inline, so a wrapper that UNIONs both paths binds both):
     tooling/client picks ast_select_from (no pe_*) vs an ast_select_pe_from by '::' presence,
     OR a C++ dispatch. Kills the 0.5s tax for ~all queries (which have no '::callers/callees').
  2. Rewrite the call graph (pseudo-class :called-by/:calls AND pseudo-element ::callers/
     ::callees) as ONE bounded, non-correlated join (index name->call, join on scope.function;
     avoid unfiltered name=name). Fixes #160 bind AND #164 exec together. Bigger, best ROI.
  3. C++ for the call graph specifically (not the whole engine) — opaque to the binder,
     bounded at exec. Endgame if SQL rewrite can't bound it.
Recommend treating #160 + #164 as one call-graph workstream; option 2 first (SQL, measurable
by warm EXPLAIN min + a 78k-node exec/memory check), option 3 if it can't be bounded in SQL.
