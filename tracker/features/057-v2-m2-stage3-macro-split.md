# 057 — M2 stage 3: the SQL macro split (duckling core vs add-ons)

**Status:** specced, awaiting Teague's approval. No code written. The
`superpowers:brainstorming` hard gate applies: nothing is built until this
document is approved.

**Measured on:** `main` at `821af42`, 2026-11-10. DuckDB binary
`.worktrees/perf/decorrelate-pseudo-classes/build/release/duckdb`, reporting
`v2.0.0-dev88614`, post-#325. Every `duckdb` call used `-no-agent`.

**Scope source:** `docs/planning/v2-architecture.md:101` "Macro layer: split
between duckling and proper". `tracker/features/054-v2-m2-in-tree-layout.md:13`
records stage 3 as "not started, by decision".

---

## 1. Situation, task, action, result

**Situation.** `sitting_duck` is a DuckDB extension. It parses source code into
abstract syntax trees with tree-sitter, and serves them as SQL tables. The v2
RFC splits the product into three repos. `sitting_duckling` is the engine and
the slim extension. `sitting_duck_languages` holds the 32 language modules.
`sitting_duck` stays as the batteries-included composition. M2 prepares that
split in-tree. Stages 0 to 2 are merged. Stage 3, the macro split, is the last
*planned* M2 piece. **Whether it is the last outstanding one is unresolved:**
054:30 lists "`core/` + `languages/<lang>/` directories" as one piece, marked
"not started"; 054:185 makes stage 1 a `git mv` to both; only `languages/`
exists today. 054 §10 records three corrections to its own plan, and none
descopes `core/`. So the `core/` half was either dropped silently or is still
owed, and `v2.0.0-alpha` may not be one stage away. Settle this outside this
document.

**Task.** Decide which of the 96 SQL macros in `src/sql_macros/` belong to the
duckling core, and which are add-ons. 054 recorded that this list did not
exist. Produce the list, then the plan.

**Action.** The split rule was found already written in the RFC. Every macro
was then measured against it: its signature, its engine calls, its hardcoded
language literals, and its place in the full macro call graph.

**Result.** 60 of 96 macros are core, 36 are add-ons. The RFC rule settles 51
of the 96 by itself. 45 were judgment calls, and §7 marks each one. Stage 3
also adds 9 `_from` variants, so that every core macro can run on an AST that
`read_ast` did not produce. **Only ONE file is mixed.** Measured from the 96 rows in §7:
`tree_navigation.sql` splits 11 core / 8 add-on, and the other 12 files land
whole on one side. The "9 of 13" figure in the first draft of this document came
from the superseded criterion of §2, and was never re-derived after the rule
changed. §8.1 now carries the consequence, which is that the re-grouping can be
far smaller than planned.

---

## 2. The rule, and the document that denies it exists

`docs/planning/v2-architecture.md:103` states the rule:

> **Split rule (not a judgment call):** a macro belongs in duckling iff it is
> language-agnostic and depends only on the AST schema. Anything that depends
> on a language module or embodies an analysis opinion is an add-on.

Line 106 then names examples on both sides.

**`054:190` reads, in full:**

> "Core accessors vs add-ons" is not written down anywhere as a **list**

The first draft of this document called that simply wrong. **That was an
over-claim, and the emphasis is the reason.** The RFC gives a *rule* and about
twenty named examples. It does not give a list of 96, and this document needed
45 judgment calls to produce one, under a rule the RFC calls "not a judgment
call". So 054 is right about the list and misleading about the rule, which it
does not mention.

The correction owed to 054 is therefore one clause, not a retraction: the rule
exists at `v2-architecture.md:103` even though the list did not. §10.1 records
it at that strength.

### What the rule does NOT say

A call to `read_ast` or `parse_ast` does not make a macro an add-on. `read_ast`
is the engine, and duckling ships the engine. RFC:16 also gives duckling the
native host-language module, which is DuckDB SQL today. So duckling can parse.
The two disqualifiers are narrow:

1. **a dependency on a specific language module**, and
2. **an analysis opinion**.

Measured: only two files fail test 1. `src/sql_macros/css_selectors.sql:94`
calls `parse_ast_list_table(selector, 'css')`, and
`src/sql_macros/ast_select_rules.sql:105` calls `parse_ast(query, 'css')`.
This is exactly what RFC:111 predicts, where it says the selector layer is
"*forced* out of duckling because it parses selectors with sitting_duck's own
CSS grammar".

Two other language literals are false positives. `duck_blocks.sql:217` is
`'yaml' AS encoding`, a format label. `pattern_matching.sql:294` is
`language := 'python'`, a default parameter value.

---

## 3. What was measured

### 3.1 The macro census

96 macros across 13 files, 7503 lines. A live build registers 45 scalar macros
and 52 table macros, which is 97 rows. 44 of the scalars come from the files;
the 45th is `ast_dispatch_predicate`, which C++ creates, not a file. The counts reconcile
exactly, so no macro silently fails to register today.

### 3.2 The call graph

27 macro-to-macro edges. `semantic_predicates.sql` is the hub: five files depend
on it, and it depends on nothing. Edges run from add-on to core, never the
reverse. **Zero core-to-add-on edges, with transitive closure included.**

`tree_navigation.sql` has exactly one internal edge, `ast_call_graph` to
`ast_get_calls`. Its 19 macros otherwise never call each other. Each analysis
macro re-implements its own traversal inline. So the file carries no internal
dependency cost when it splits.

### 3.3 Binding time, and why registration is not a gate

A `CREATE MACRO` statement binds its body eagerly. An unknown function fails
the CREATE, not the later call. This holds for scalar macros, for table macros,
and inside a `CASE` arm. Four shapes were tested and all four failed at CREATE.

**There is one exception, and it covers the family that matters most.** When a
table macro's `FROM` is `query_table(<parameter>)`, the schema is unknown at
CREATE time, so DuckDB defers the whole body. Measured:

```
CREATE MACRO g1(t) AS TABLE (SELECT missing_fn_xyz(x) FROM query_table(t));
-- CREATE SUCCEEDS. The error appears only when g1 is called.
```

Every `_from` macro uses `query_table()`. That is the core's foreign-AST
surface, and the 9 new variants in §6 will join it. **So a mis-split in the
`_from` family does not fail at LOAD. It fails inside a user's query.** The
static call graph is the only check that can catch it, which is why §8 requires
a CI gate rather than trusting a green build.

This also explains the `ast_dispatch_predicate` ordering. `css_selectors.sql`
registers seventh, and `src/ast_sql_macros.cpp:181` creates the stub after the
file loop. The call binds anyway, because `ast_select_from` reads
`query_table(source)`.

### 3.4 The dispatch seam

Relevant to a later decision about `ast_select` as its own extension.

| Where | What |
|---|---|
| `src/ast_sql_macros.cpp:181` | creates the no-op stub, `AS (false)`, after the file loop |
| `src/sitting_duck_extension.cpp:46` | `PRAGMA sitting_duck_enable_dynamic_predicates` replaces it with `apply(fn, node, arg)::BOOLEAN` |
| `src/sql_macros/css_selectors.sql:2153` | the only caller, in an `ELSE` arm |

The PRAGMA needs the `func_apply` community extension, and refuses with an
install hint when it is absent.

### 3.5 The engine surface the macros rely on

At least 19 names are called from the `.sql` files but defined in C++. The
five below are the ones the split turns on. The others are semantic-type
scalars from `src/semantic_type_functions.cpp` — `is_definition`,
`is_construct`, `is_identifier`, `is_constituent`, `is_exported`, `is_kind`,
`is_scope`, `is_name_declaration`, `is_name_definition`, `is_name_reference`,
`is_semantic_type`, `semantic_type_code`, `semantic_type_to_string` — plus
`parse_ast_objects`. **So this table is not duckling's engine contract, and
must not be cited as one.** All are registered before
`RegisterASTSQLMacros`, which `src/sitting_duck_extension.cpp` calls at line
110.

| Name | Kind | Registered at |
|---|---|---|
| `is_syntax_only` | scalar | `src/semantic_type_functions.cpp:626` |
| `parse_ast_list` | scalar | `src/parse_ast_list_function.cpp:269` |
| `ast_unparse_rules` | table function, per language | `src/unparse_rules.cpp:489` |
| `ast_semantic_aliases` | table function | `src/ast_semantic_aliases_function.cpp:99` |
| `ast_dispatch_predicate` | stub macro | `src/ast_sql_macros.cpp:181` |

`ast_unparse_rules` takes an optional language argument, and its data comes from
each module's `languages/<lang>/<lang>_unparse.def`.

**Consequence for core, and the first draft of this document got it wrong.** It
claimed a duckling shipping only the DuckDB SQL module "can unparse DuckDB SQL,
and nothing else". That is false, measured three ways:

1. `src/unparse_rules.cpp` has 53 `map[...]` entries covering about 30
   languages. **Neither `duckdb` nor `sql` is among them.**
2. `languages/duckdb/` holds only `duckdb_adapter.cpp` and `module.cmake`.
   There is no `duckdb_unparse.def`, and no `duckdb_types.def` either.
3. `src/sql_macros/ast_unparse.sql:502-515` carries a measured note headed
   **"NOT SERVED: the `duckdb` language adapter"**. Every node reports
   `start_byte = end_byte = 0` with an empty `file_path`, and the tree is an AST
   rather than a CST: 15 nodes for an 84-byte file, with no node for any
   keyword, punctuation or comment. A dedicated guard **errors**.

So a duckling with only the DuckDB SQL module can unparse **nothing**. The 8
unparse macros stay in core, because the mechanism is language-agnostic and the
rules are module data; what duckling ships is the capability, and a module
brings the content. They cannot be exercised by that build's own language, so
§9.3 drives them from a committed fixture instead.

This is settled rather than merely noted. Teague's 2026-11-10 ruling that
macros may live in `sitting_duck` without living in `sitting_duckling` means the
split never has to prove that duckling alone is useful for every macro it
carries. A core macro earns its place by being language-agnostic, not by
working on zero modules.

---

## 4. Goals

1. Produce the core-vs-add-on list, which 054 called missing. §7 is that list.
2. Re-group the macros into files that match the boundary, so a build can
   include core without add-ons.
3. Give every core macro a form that runs on a caller-supplied AST table.
4. Prove the re-grouping changed no behaviour, with an oracle, not an opinion.

## 5. Non-goals and anti-goals

**Non-goals.**

- The repo split. That is M3, and the RFC puts it behind the
  split-after-stability rule.
- A `sitting_duckling` build target, a second registry descriptor, or any
  distribution change. `tracker/features/053` holds the distribution question.
- The column contract. A separate measurement found that `read_ast`'s 22
  columns contain 21 that a macro must declare, but only 10 that a selector
  must have populated. Narrowing that is its own decision, and it is not in
  this document.
- `tracker/features/046` versus `048` row 5, which disagree about whether
  `046` gates v2.0.0. Still open.
- Correcting `054:190`, `MainDistributionPipeline.yml:546`, or the dead
  `src/sql_macros/CMakeLists.txt`. §10 records all three.

**Anti-goals.** These would make stage 3 a failure even if it worked.

- A core that cannot be exercised **with a committed pre-parsed AST fixture**.
  The first draft said "cannot be exercised without a tree-sitter language",
  which §3.5 then measured to be impossible to satisfy: a duckling shipping
  only the DuckDB SQL module can unparse nothing, because that adapter reports
  zero byte offsets and produces an AST rather than a CST. The achievable and
  useful form of this anti-goal is the fixture one, and §9.3 is how it is
  tested. Teague settled the surrounding question on 2026-11-10: "I'm totally
  ok with having macros in sitting_duck that are not in sitting_duckling." So
  a capability that needs a language module is not a defect in the split.
- Any change to a macro's behaviour, name, or parameters during the move. A
  re-grouping that also edits bodies cannot be verified by the §8 oracle, and
  it breaks the Golden Rule in `~/.CLAUDE.md`.
- A silent reduction in what a default `sitting_duck` build registers. The
  composition must still register all 105 macros. Only the file grouping moves.

## 6. The 9 new `_from` variants

Teague asked for parity, so that a core macro works on pre-parsed data. This is
his case 3: "use any of the core utilities or macros on pre-parsed data".

9 core macros take a path or a glob, and have no table-taking sibling:

| New macro | Existing twin | First argument today |
|---|---|---|
| `ast_has_from` | `ast_has` | `source` |
| `ast_not_has_from` | `ast_not_has` | `source` |
| `ast_inside_from` | `ast_inside` | `source` |
| `ast_precedes_from` | `ast_precedes` | `source` |
| `ast_follows_from` | `ast_follows` | `source` |
| `ast_in_range_from` | `ast_in_range` | `source` |
| `ast_containing_line_from` | `ast_containing_line` | `source` |
| `ast_definitions_from` | `ast_definitions` | `source` |
| `ast_source_of_from` | `ast_source_of` | `file_patterns` |

Each new macro follows the pattern that `ast_select_from` and
`ast_to_blocks_from` already use: take a table name, resolve it with
`query_table()`, and hold the logic once.

**`ast_source_of_from` is the exception, and it cannot be built that way.**
`ast_source_of` joins `read_text(file_patterns)` to get the lines
(`src/sql_macros/file_utilities.sql:113-120`), and a node table carries no
source text. `read_text` also takes only literal arguments, which is the same
restriction `ast_unparse_exact_from` documents for `read_blob`. So it must
either take a second `files` argument, exactly as
`ast_unparse_exact_from(ast_table, files, …)` does, or drop its `source`
column. The first bends the parity pattern; the second breaks §9.2's `EXCEPT`
oracle, which needs both twins to return the same columns. **Take the `files`
argument, and follow the `ast_unparse_exact_from` precedent.** This is my call,
and it sets how case 3 behaves for any macro returning text rather than
nodes. The existing path-taking macro then
becomes a wrapper that calls the `_from` form. That keeps the logic in one
place, as `ast_select` does today.

**Macros were moved OUT of core instead of gaining a variant.** Teague
authorised this on 2026-11-10: "feel free to move them to a different section,
e.g. out of core, if they are out of place".

- `ast_functions_containing` answers "which functions contain a node of type
  X". That is a review question, and the macro sits beside
  `ast_function_metrics` and `ast_nesting_analysis`, which RFC:113 names as
  add-ons.
- `ast_resolve_entity` resolves a name inside one file. Teague ruled it an
  add-on on 2026-11-10.
- `ast_node_edit` and `ast_patch` leave core for a mutator add-on suite. Teague
  ruled this on 2026-11-10: "probably a whole suite of ast mutator addons
  (which is a special case we haven't even fully defined)". `ast_replace` was
  already an add-on, so the three now ship together.

**`ast_source_of` was moved out of core and then back in.** It reads as a glob
and a name match, so it looked like a search. It is not: it returns the source
text of a named definition, with its kind and line range. That is the
"show me the definition of X" accessor. Teague's test settled it on 2026-11-10:
"If it's a useful accessor -> core".

## 7. The classification

60 core, 36 add-on. The `basis` column says whether the RFC settled the macro,
or whether it was my call. A "my call" row is a decision a reviewer should
check first.

| file | macro | lands in | kind | first arg | engine calls | basis |
|---|---|---|---|---|---|---|
| `ast_patch.sql` | `ast_node_edit` | **add-on** | scalar | `node` | `-` | my call |
| `ast_patch.sql` | `ast_patch` | **add-on** | TABLE | `edits` | `read_text` | my call |
| `ast_patch.sql` | `ast_replace` | **add-on** | TABLE | `source` | `read_ast` | my call |
| `ast_select_rules.sql` | `ast_select_list` | **add-on** | TABLE | `source` | `-` | RFC:111 forced out (css grammar) |
| `ast_select_rules.sql` | `ast_select_rules` | **add-on** | TABLE | `source` | `parse_ast` | RFC:111 forced out (css grammar) |
| `ast_unparse.sql` | `ast_unparse` | **core** | TABLE | `path` | `read_ast` | my call |
| `ast_unparse.sql` | `ast_unparse_code` | **core** | TABLE | `source_code` | `parse_ast` | my call |
| `ast_unparse.sql` | `ast_unparse_custom` | **core** | TABLE | `ast_table` | `-` | my call |
| `ast_unparse.sql` | `ast_unparse_exact` | **core** | TABLE | `path` | `read_ast,read_blob` | my call |
| `ast_unparse.sql` | `ast_unparse_exact_code` | **core** | TABLE | `source_code` | `parse_ast` | my call |
| `ast_unparse.sql` | `ast_unparse_exact_from` | **core** | TABLE | `ast_table` | `read_blob` | my call |
| `ast_unparse.sql` | `ast_unparse_exact_splice` | **core** | TABLE | `ast_table` | `-` | my call |
| `ast_unparse.sql` | `ast_unparse_from` | **core** | TABLE | `ast_table` | `-` | my call |
| `css_selectors.sql` | `ast_select` | **add-on** | TABLE | `source` | `read_ast` | RFC:111 forced out (css grammar) |
| `css_selectors.sql` | `ast_select_from` | **add-on** | TABLE | `source` | `parse_ast_list` | RFC:111 forced out (css grammar) |
| `duck_blocks.sql` | `ast_to_blocks` | **add-on** | TABLE | `source` | `read_ast` | my call |
| `duck_blocks.sql` | `ast_to_blocks_from` | **add-on** | TABLE | `source` | `-` | my call |
| `duck_blocks.sql` | `ast_to_blocks_list` | **add-on** | TABLE | `source` | `-` | my call |
| `file_utilities.sql` | `ast_get_source` | **core** | scalar | `file_path` | `read_text` | my call |
| `file_utilities.sql` | `ast_get_source_line` | **core** | scalar | `file_path` | `read_text` | my call |
| `file_utilities.sql` | `ast_get_source_numbered` | **core** | scalar | `file_path` | `read_text` | my call |
| `file_utilities.sql` | `ast_source_of` | **core** | TABLE | `file_patterns` | `read_ast,read_text` | my call |
| `parse_ast_list_table.sql` | `parse_ast_list_table` | **core** | TABLE | `code` | `parse_ast_list` | my call |
| `pattern_matching.sql` | `ast_capture` | **add-on** | scalar | `captures_map` | `-` | my call |
| `pattern_matching.sql` | `ast_match` | **add-on** | TABLE | `source` | `read_ast` | RFC:112-114 names it |
| `pattern_matching.sql` | `ast_pattern` | **add-on** | TABLE | `pattern_str` | `parse_ast` | my call |
| `pattern_matching.sql` | `ast_pattern_list` | **add-on** | scalar | `pattern_str` | `-` | my call |
| `pattern_matching.sql` | `clean_pattern` | **add-on** | scalar | `pattern_str` | `-` | my call |
| `pattern_matching.sql` | `is_pattern_wildcard` | **add-on** | scalar | `name` | `-` | my call |
| `pattern_matching.sql` | `parse_html_wildcard` | **add-on** | scalar | `html_str` | `-` | my call |
| `pattern_matching.sql` | `pattern_has_recursive` | **add-on** | scalar | `pattern_str` | `-` | my call |
| `pattern_matching.sql` | `pattern_has_variadic` | **add-on** | scalar | `pattern_str` | `-` | my call |
| `pattern_matching.sql` | `semantic_type_base` | **add-on** | scalar | `sem_type` | `-` | my call |
| `pattern_matching.sql` | `wildcard_capture_name` | **add-on** | scalar | `name` | `-` | my call |
| `relational_operators.sql` | `ast_follows` | **core** | TABLE | `source` | `read_ast` | my call |
| `relational_operators.sql` | `ast_has` | **core** | TABLE | `source` | `read_ast` | my call |
| `relational_operators.sql` | `ast_inside` | **core** | TABLE | `source` | `read_ast` | my call |
| `relational_operators.sql` | `ast_not_has` | **core** | TABLE | `source` | `read_ast` | my call |
| `relational_operators.sql` | `ast_precedes` | **core** | TABLE | `source` | `read_ast` | my call |
| `scope_resolution.sql` | `ast_callees` | **add-on** | TABLE | `source` | `read_ast` | RFC:113 scope resolution |
| `scope_resolution.sql` | `ast_callers` | **add-on** | TABLE | `source` | `read_ast` | RFC:113 scope resolution |
| `scope_resolution.sql` | `ast_exports` | **add-on** | TABLE | `source` | `read_ast` | RFC:113 scope resolution |
| `scope_resolution.sql` | `ast_find_references` | **add-on** | TABLE | `source` | `read_ast` | RFC:113 scope resolution |
| `scope_resolution.sql` | `ast_imports` | **add-on** | TABLE | `source` | `read_ast` | RFC:113 scope resolution |
| `scope_resolution.sql` | `ast_resolve` | **add-on** | TABLE | `source` | `read_ast` | RFC:113 scope resolution |
| `selector_for.sql` | `ast_selector_for` | **add-on** | TABLE | `source` | `-` | my call |
| `semantic_predicates.sql` | `ast_qualified_name_as_string` | **core** | scalar | `qn` | `-` | my call |
| `semantic_predicates.sql` | `is_annotation` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_arithmetic` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_assignment` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_block` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_boolean_literal` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_class_definition` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_comment` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_comparison` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_conditional` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_directive` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_export` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_foreign` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_function_call` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_function_definition` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_import` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_jump` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_list` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_literal` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_logical` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_loop` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_member_access` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_module_definition` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_number_literal` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_string_literal` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_type_composite` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_type_definition` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_type_generic` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_type_primitive` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_type_reference` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `semantic_predicates.sql` | `is_variable_definition` | **core** | scalar | `st` | `-` | RFC:106 names it |
| `tree_navigation.sql` | `ast_ancestors` | **core** | TABLE | `ast_table` | `-` | RFC:106 names it |
| `tree_navigation.sql` | `ast_call_arguments` | **core** | TABLE | `ast_table` | `-` | my call |
| `tree_navigation.sql` | `ast_call_graph` | **add-on** | TABLE | `source` | `-` | RFC:112-114 names it |
| `tree_navigation.sql` | `ast_children` | **core** | TABLE | `ast_table` | `-` | RFC:106 names it |
| `tree_navigation.sql` | `ast_class_members` | **core** | TABLE | `ast_table` | `-` | my call |
| `tree_navigation.sql` | `ast_containing_line` | **core** | TABLE | `source` | `read_ast` | my call |
| `tree_navigation.sql` | `ast_dead_code` | **add-on** | TABLE | `source` | `read_ast` | RFC:112-114 names it |
| `tree_navigation.sql` | `ast_definition_parent` | **core** | TABLE | `ast_table` | `-` | RFC:106 names it |
| `tree_navigation.sql` | `ast_definitions` | **core** | TABLE | `source` | `read_ast` | my call |
| `tree_navigation.sql` | `ast_descendants` | **core** | TABLE | `ast_table` | `-` | RFC:106 names it |
| `tree_navigation.sql` | `ast_function_metrics` | **add-on** | TABLE | `source` | `read_ast` | RFC:112-114 names it |
| `tree_navigation.sql` | `ast_function_scope` | **core** | TABLE | `ast_table` | `-` | RFC:106 names it |
| `tree_navigation.sql` | `ast_functions_containing` | **add-on** | TABLE | `source` | `read_ast` | my call |
| `tree_navigation.sql` | `ast_get_calls` | **add-on** | TABLE | `source` | `read_ast` | RFC:112-114 names it |
| `tree_navigation.sql` | `ast_in_range` | **core** | TABLE | `source` | `read_ast` | my call |
| `tree_navigation.sql` | `ast_nesting_analysis` | **add-on** | TABLE | `source` | `read_ast` | my call |
| `tree_navigation.sql` | `ast_resolve_entity` | **add-on** | TABLE | `path` | `read_ast` | my call |
| `tree_navigation.sql` | `ast_security_audit` | **add-on** | TABLE | `source` | `read_ast` | RFC:112-114 names it |
| `tree_navigation.sql` | `ast_siblings` | **core** | TABLE | `ast_table` | `-` | my call |

---

## 8. Target file layout and registration order

### 8.1 Why the files must change

`src/ast_sql_macros.cpp:80` iterates `EMBEDDED_SQL_MACROS`, which is a vector
of (filename, content) pairs. The unit of inclusion is therefore **a file**.

**The boundary already matches the existing files, with one exception.**
Measured over §7's 96 rows:

| | files | macros |
|---|---|---|
| pure core | `semantic_predicates`, `ast_unparse`, `relational_operators`, `file_utilities`, `parse_ast_list_table` | 49 |
| pure add-on | `pattern_matching`, `scope_resolution`, `ast_patch`, `duck_blocks`, `css_selectors`, `ast_select_rules`, `selector_for` | 28 |
| **mixed** | **`tree_navigation`** | **11 core + 8 add-on** |

That is not luck. The files were grouped by purpose, and the RFC rule is about
purpose, so the two mostly coincide. **The minimum change that expresses the
boundary is therefore to split one file.** Two shapes follow, and §13 records
which one Teague chose:

- **Full re-group.** 14 new filenames, 12 renames, every macro's text
  relocated. A `core_`/`addon_` prefix then carries the boundary, so §9.4's
  gate can decide ownership from a filename alone.
- **Minimal.** Split `tree_navigation.sql` in two and leave the other 12 names
  alone. One file's text moves. The boundary then needs a manifest, because no
  filename states it; `spec/layout/layout.yaml` is the natural home, and §11
  already floats it.

**054:170-176 argues for the minimal shape.** It states that M3 extracts
duckling and languages with `git filter-repo`, keeping per-directory history,
and that "every move must be a pure `git mv` commit with no content edit mixed
in". A file that splits in two cannot be a pure `git mv`: at most one output is
scored as the rename of the input. The full re-group turns 1 unavoidable
non-rename into 13.

### 8.2 The authoritative file list, and a trap

**The real list is `scripts/embed_sql_macros.py:176-190`.** It is a hardcoded,
ordered Python list of the 13 filenames. `CMakeLists.txt:252` runs that script
on every configure.

**`src/sql_macros/CMakeLists.txt` is dead code, and it is a trap.** It contains
its own generator, and its file list reads:

```cmake
set(SQL_MACRO_FILES semantic_predicates.sql file_utilities.sql)
```

Two files, not 13. Nothing includes it: the only `add_subdirectory` in the root
`CMakeLists.txt` is `third_party/tree-sitter/lib`. A builder who searches for
"the macro file list" will find this file first, and it is wrong. Do not edit
it. §10 proposes deleting it separately.

### 8.3 The grouping: one split, one manifest

**Teague ruled for the minimal shape on 2026-11-10.** Stage 3 splits one file
and renames nothing.

| Action | File |
|---|---|
| keeps its path and its 11 core macros | `tree_navigation.sql` |
| new, takes the 8 add-on macros | `tree_analysis.sql` |
| unchanged, 12 files | everything else |

`tree_navigation.sql` keeps its path because it keeps the accessors, which is
what the name means. **Measured: the add-on half is the larger one** -- 628 of
the file's 950 lines against 308 for the core half, because the two kinds of
macro are interleaved rather than stacked.

That does not matter, and the first draft's reason for caring was wrong. It
claimed git would "score the split as a rename of the input" and that the larger
half therefore had to keep the name. **Rename detection never applies here.**
Git reports a rename only for a path that disappears, and `tree_navigation.sql`
persists, so the commit is one modified file plus one added file. 054's
`git filter-repo` constraint is satisfied for a different and simpler reason:
both files sit in `src/sql_macros/`, so a per-directory extraction keeps both
whatever their sizes. Size and naming are free choices, and the name should
follow the meaning.

`tree_analysis.sql` takes 8 macros, and they are **not contiguous**.

**CORRECTED 2026-11-10.** This table first gave each macro's span as its
`CREATE` line to the line before the next `CREATE`. That is the wrong unit. Each
macro's `-- <name>: <summary>` banner and its leading comment block sit **above**
the `CREATE` line, and `scripts/embed_sql_macros.py` lifts the
`duckdb_functions()` description from that banner. A move cut at the `CREATE`
line therefore strips all 8 descriptions and turns
`test/sql/bugs/issue_170_macro_descriptions.test:99-104` red, which asserts that
no non-system table macro has an empty description. `ast_resolve_entity` is the
clearest case: banner at line 105, `CREATE` at 107.

The unit to move is banner-to-banner. Measured spans in today's
`tree_navigation.sql`, each from the first line of the macro's comment block:

| lines | macro | moves |
|---|---|---|
| 89-133 | `ast_resolve_entity` | yes |
| 298-405 | `ast_function_metrics` | yes |
| 406-484 | `ast_functions_containing` | yes |
| 485-569 | `ast_nesting_analysis` | yes |
| 570-688 | `ast_security_audit` | yes |
| 736-842 | `ast_dead_code` | yes |
| 843-927 | `ast_get_calls` | yes |
| 928-950 | `ast_call_graph` | yes |

So the extraction is 3 discontiguous blocks, not a tail: **89-133, 298-688 and
736-950**. The 11 core macros keep lines **1-88, 134-297 and 689-735**, the
first block including the file header. The file is 949 lines by `wc -l`. The one internal edge,
`ast_call_graph` to `ast_get_calls`, falls inside the moved set, so no
cross-file edge is created.

### 8.4 Where the boundary is recorded

No filename states the boundary under the minimal shape, so a manifest must.
**`spec/layout/layout.yaml` is the home**, because it is already the authority
the layout gate reads, and it already governs registration order for languages.

The manifest lists all 14 files in canonical registration order, each tagged
`core` or `addon`. `scripts/embed_sql_macros.py:176-190` then reads it, instead
of carrying its own hardcoded copy, which gives the generator and §9.4's gate
one source.

### 8.5 Ordering is already solved, and needs no change

`spec/layout/layout.yaml:45-49` records the precedent, for languages:

> Declaration order in `cmake/BuiltinLanguages.cmake` IS registration order;
> the file says so, and re-sorts a user-supplied subset into it so that a
> subset build registers in the same relative order as a full build.

**The macro manifest follows that pattern exactly: one canonical order, and a
subset filters it while preserving relative order.** So there is no "core files
first" requirement, and no reordering in stage 3.

Measured: the core subset in today's order already satisfies every core-to-core
dependency.

| # | file | side |
|---|---|---|
| 1 | `semantic_predicates.sql` | core |
| 2 | `file_utilities.sql` | core |
| 3 | `tree_navigation.sql` | core, after the split |
| 4 | `tree_analysis.sql` | add-on, new, inserted here |
| 5 | `pattern_matching.sql` | add-on |
| 6 | `relational_operators.sql` | core |
| 7 | `parse_ast_list_table.sql` | core |
| 8 | `css_selectors.sql` | add-on |
| 9 | `selector_for.sql` | add-on |
| 10 | `ast_select_rules.sql` | add-on |
| 11 | `scope_resolution.sql` | add-on |
| 12 | `duck_blocks.sql` | add-on |
| 13 | `ast_patch.sql` | add-on |
| 14 | `ast_unparse.sql` | core |

`semantic_predicates.sql` is first, and the five files that depend on it all
come later. `tree_analysis.sql` is inserted at position 4, directly after the
file it leaves, so the full-build order changes by an insertion and nothing
else. Filtering to core yields 1, 2, 3, 6, 7, 14, which preserves relative
order and keeps the hub first.

The cross-file constraints from §3.2 are all still satisfied, and none of them
binds within core: `ast_select_from` needs `parse_ast_list_table` (7 before 8),
`ast_select_rules` needs `ast_select` (8 before 10), and `ast_replace` needs
both `ast_patch` and `ast_select_from` (8 before 13).


---

## 9. The oracle

The §5 anti-goal forbids any behaviour change during the move. An oracle must
prove that, not assert it. The oracle has two halves, because the work has two
halves.

### 9.1 The re-grouping half: byte-identical, for the 96 existing macros

`duckdb_functions()` exposes `macro_definition`. Take a dump before the change
and after it, restricted to the 96 pre-existing names, and require that the two
are byte-identical:

```sql
SELECT function_name, function_type, parameters, macro_definition
FROM duckdb_functions()
WHERE database_name = 'memory'
ORDER BY function_name, function_type;
```

A re-grouping that only moves text between files cannot change any row. If a
row changes, a body was edited, and that is out of scope. This is a strong
oracle, and it is the reason §5 forbids edits during the move.

Reference figures, measured 2026-11-10 on `main`: **45 scalar macros, 52 table
macros, 97 rows**. The 45 is 44 defined in `src/sql_macros/` plus the
`ast_dispatch_predicate` stub, which is itself a scalar macro.

### 9.2 The new-macro half: equivalence with the existing twin

Each of the 9 new `_from` macros must return the same rows as its path-taking
twin, given the same AST. The test parses once, then compares:

```sql
CREATE TABLE t AS SELECT * FROM read_ast('test/data/python/sample_app.py');
-- must be empty, in both directions
SELECT * FROM ast_has('test/data/python/sample_app.py', 'class_definition', 'function_definition')
EXCEPT SELECT * FROM ast_has_from('t', 'class_definition', 'function_definition');
```

**A control is required.** `~/.CLAUDE.md` says a test asserting only an empty
result has no power. So each test must also show the comparison is capable of
failing: assert a non-zero baseline row count first, and plant one wrong
argument to confirm the `EXCEPT` reports it.

### 9.3 The core-alone test, which is the point of the split

The RFC gives duckling the DuckDB SQL module and no tree-sitter languages.
`cmake/BuiltinLanguages.cmake` already accepts `-DSITTING_DUCK_LANGUAGES`.
**Whether it accepts `duckdb` alone is not yet measured. Measure it before
planning this step.**

The test must **call** every core macro, not merely register it. §3.3 is the
reason: a `_from` macro registers whether or not its body is valid. A
registration count would pass on a broken core.

**But the only language a duckling ships cannot exercise all of core.** §3.5
measures that the 8 unparse macros refuse DuckDB SQL by design. `bugs/011`
records that SQL `CREATE MACRO` is not classified as a function definition, so
`ast_definitions`, `ast_source_of` and the `ast_has` family may return zero rows
on DuckDB SQL for reasons unrelated to the split. A test that accepts 0 rows is
the vacuous gate `~/.CLAUDE.md` forbids.

**So §9.3 must be driven by a checked-in pre-parsed AST fixture, not by a live
parse.** That is exactly Teague's case 3, and it is what the `_from` variants
exist for: a table of nodes from a tree-sitter language, committed as test
data, loaded into a build that has no tree-sitter language at all. Every core
macro with a table-taking form is then exercisable with zero grammars. The
macros that remain path-only after §6 need an explicit written exemption list,
with a reason each, and no exemption may be "it returns 0 rows".

`scripts/test_language_subset.sh` cannot be reused: it is hardcoded to
`python;cpp` and asserts that a compiled-out language errors. §9.3 needs its own
script, and probably a second CI build leg.

### 9.4 The static gate CI needs

Because §3.3 shows LOAD cannot catch a mis-split, add a gate that rebuilds the
macro call graph from `src/sql_macros/*.sql` and fails when a file the manifest
tags `core` references a macro owned by a file it tags `addon`. Under the
minimal shape the gate reads ownership from §8.4's manifest, not from a
filename prefix. The script written for this
document does that in about 60 lines. It must strip SQL comments **and**
single-quoted string literals before matching. An earlier version stripped only
comments, and it reported two edges that were text inside an error message.

---

## 10. Recorded, not fixed

1. **`054:190`** says the list "is not written down anywhere as a list". That
   is accurate; the RFC gives a rule and about twenty examples, not 96 rows.
   What 054 omits is that the *rule* exists at
   `docs/planning/v2-architecture.md:103`. The correction is one clause added,
   not a retraction. §2 explains why the stronger claim was wrong.
2. **`src/sql_macros/CMakeLists.txt`** is unreferenced and lists 2 of 13 files.
   Delete it, in its own change.
3. **`MainDistributionPipeline.yml:546`** still describes `duckdb/duckdb#26036`
   as "a super-linear-with-statement-size planning cost". PR #325 measured the
   cause as correlated subqueries in one pseudo-class `CASE`. This is the third
   place carrying the wrong mechanism. Editing a workflow file deserves a real
   CI run, so it was not bundled here.
4. **`pattern_matching.sql:294`** defaults `language := 'python'` in
   `ast_match`. A language-agnostic macro with a Python default is a wart. Not
   in scope.
5. **Three lines** in `css_selectors.sql` (863, 966, 1242) contain `--` inside
   a string literal. The §9.4 stripper handles quotes first, so they are safe.
   A comment-only stripper truncates them, which can only hide an edge, never
   invent one.

---

## 10b. The `core/` directory: a documentation decision, not work

Raised by the builder check on 2026-11-10 and measured the same day.

**The RFC asks for it in M2.** `docs/planning/v2-architecture.md:241-245` makes
M2 "`core/` + `languages/<lang>/` directories, grammar submodule refs replace
vendored copies, `module.cmake` per language ... Macro split lands here", then
tags `v2.0.0-alpha`.

**It was never created.** Measured: no `core/` directory exists; no commit ever
added a file under `core/`; and stage 1's commit A is titled "`git mv`
**per-language** sources into `languages/<lang>/`" (`8532350`), not into both.
054:30 still lists the piece as "not started".

**But the tree and its own gate already assume it will not happen in M2.**
`spec/layout/layout.yaml:118-121` lists `engine_adapters` as files that
"deliberately did NOT move into `languages/`. They are engine code, not language
modules, and **M3 extracts them with the engine**." So layout.yaml, written
during stage 0, already treats the engine as staying in `src/` until M3.

**Recommendation: descope `core/` from M2 formally.** Amend `RFC:241` and
`054:30`, and record that the engine stays in `src/` until M3 extracts it. The
engine-versus-language boundary is already expressed by `languages/` existing,
and M3's `git filter-repo` can extract from `src/` exactly as well as from
`core/`. A `src/` to `core/` rename before M3 is a very wide move with no
functional gain, and 054:170-176 makes every such move a cost.

If that is accepted, stage 3 is genuinely the last outstanding M2 piece and §1's
claim becomes true. **Until it is accepted, §1's hedge stands.** This is a
decision about two documents, not a piece of work.

## 11. Left to the builder

- Nothing about the grouping. §8.3 settles it: one split, no renames. §8.5
  shows the existing order already works, so ordering is not open either.
- Whether the path-taking twin becomes a thin wrapper over the new `_from`
  macro, or keeps its own body. The wrapper is preferred, because
  `ast_select` already works that way, and it holds the logic once. It does
  change `macro_definition` for those 9 rows, so §9.1's oracle must exclude
  them and §9.2 must cover them.
- Test file placement and naming, following the existing `test/sql/` layout.
- The name of the new file. §8.3 proposes `tree_analysis.sql`.
- The manifest's exact shape inside `spec/layout/layout.yaml`. §8.4 settles
  that it goes there and that the generator reads it; the YAML keys are the
  builder's.

## 12. Decisions Teague made, as he made them

| Date | Decision |
|---|---|
| 2026-11-10 | `ast_unparse` and `file_utilities` go in core. "I think those last two go in core." |
| 2026-11-10 | Re-group the macros into new files, rather than label the existing 13. This authorises the refactor that `~/.CLAUDE.md` otherwise forbids. |
| 2026-11-10 | Keep parity: the `_from` variants land in this phase, not a later one. |
| 2026-11-10 | Macros may be moved out of core when they are out of place. |
| 2026-11-10 | `ast_resolve_entity` is an add-on. |
| 2026-11-10 | `ast_patch` is an add-on, and the mutators become their own suite: "probably a whole suite of ast mutator addons (which is a special case we haven't even fully defined)". |
| 2026-11-10 | `ast_source_of` is core, by the accessor test: "If it's a useful accessor -> core". |
| 2026-11-10 | Macros may live in `sitting_duck` without living in `sitting_duckling`: "I'm totally ok with having macros in sitting_duck that are not in sitting_duckling." So "add-on" is an ordinary outcome, not a demotion, and no macro needs forcing into core to justify itself. |
| 2026-11-10 | **Minimal re-group: split `tree_navigation.sql` only, rename nothing.** This supersedes the earlier "re-group into new files" ruling, which was given when I had told him 9 of 13 files were mixed. One is. |
| 2026-11-10 | Deprecation is in scope for 2.0.0: "We can deprecate macros in 2.0.0 heavily as astcss has changed a lot." |
| 2026-11-02 | `sitting_duck` and `sitting_duckling` both go to community-extensions; per-language packs do not. Recorded in `053`. |
| 2026-10-07 | The two `write_ast` laws, split by source retention. Recorded in the RFC. |

His three cases for a duckling-only install, stated 2026-11-10:

1. parse the DuckDB SQL itself,
2. load a single language dynamically,
3. use any of the core utilities or macros on pre-parsed data.

Case 1 is served by the native DuckDB SQL module, per RFC:46. Case 2 is served
by the runtime grammar loader. **Case 3 is why §6 exists.** Before this
document, 13 core macros took a table, and the rest required a path.

## 13. Placement calls, resolved

All four calls from the first draft now have a ruling.

| Macro | Ruling | Date |
|---|---|---|
| `ast_source_of` | **core.** "If it's a useful accessor -> core", and it is one | 2026-11-10 |
| `ast_resolve_entity` | **add-on** | 2026-11-10 |
| `ast_patch`, and the mutators with it | **add-on**, as the start of a mutator suite | 2026-11-10 |
| the 10 pattern helpers | **add-on**, my call, unchallenged | 2026-11-10 |

One tension is recorded rather than resolved. **RFC:16 gives duckling
"splice/rewrite".** The splice half stays core, as
`ast_unparse_exact_splice`. The rewrite half is now an add-on, by Teague's
ruling. So RFC:16 over-promises, and that line needs an edit when the mutator
suite is defined. The ruling wins; the RFC line is stale.

---

## 14. Deprecation assessment for 2.0.0

Teague opened this on 2026-11-10: "feel free to assess if a macro is actually
worth keeping. We can deprecate macros in 2.0.0 heavily as astcss has changed
a lot."

**Method.** Every macro name was counted as a call site in `test/`, in `docs/`
and in `README.md`. A count is evidence of investment, not of correctness, and
a macro that another macro calls is exercised without being counted. So the
census ranks candidates; it does not decide them.

### 14.1 `ast_pattern_list`: nothing to do

The first draft called it the one macro of 96 with no support anywhere, and
recommended dropping it from the public surface. **Both halves were wrong.**

- `test/sql/bugs/issue_170_macro_descriptions.test:60` names it, inside a
  58-macro list that asserts every one carries a description in
  `duckdb_functions()`. My census counted `name(` call sites, and the gate
  lists it as a bare string, so the census could not see it.
- It has a description: "Parse a pattern string into a list of pattern tokens".
- **DuckDB has no private-macro mechanism.** A registered macro is public.
  "Drop from the public surface" is not implementable without editing the #170
  gate to expect one fewer macro.

**Verdict: keep, document nothing new, change nothing.**

### 14.2 `ast_resolve`: keep it, and write a behavioural test

The first draft called this the clearest deprecation case, on 0 test call sites
against `ast_find_references`'s 27, and inferred it was superseded.
**Measured, it is not superseded.** Both were run on
`test/data/python/sample_app.py`:

```
ast_resolve(file)                      -> 166 rows
ast_find_references(file, 'main')      ->   2 rows
```

They are different operations. `ast_resolve` returns the whole file's
reference-to-definition table, and it carries two columns its sibling does not:
`scope_hops BIGINT[]` and
`def_qualified_name STRUCT(semantic_type, name, index)[]`.
`ast_find_references` resolves references to **one named symbol**, and is closer
to derivable from `ast_resolve` than the reverse. `ast_resolve` is the more
primitive of the pair.

It is also already in the #170 description gate, so "0 tests" was never true in
the sense that mattered; what it lacks is a behavioural test.

**Verdict: keep. The defect is a missing test, not a redundant macro.**

### 14.3 The two dialects are not substitutes, so neither can replace the other

The first draft put the pattern dialect's survival to Teague as the one
deprecation decision worth making, and said that if patterns went, 11 of the
add-on macros and a whole file would disappear. **That framing assumed the two
dialects overlap. Measured, they do not.**

`ast_match` returns a column `ast_select` has no analogue for:

```
captures  MAP(VARCHAR, STRUCT(capture VARCHAR, node_id BIGINT, "type" VARCHAR,
                              "name" VARCHAR, peek VARCHAR, start... ))
```

Measured on the pattern `__F__(__X__)`: `map_keys(captures)` returns
`[F, X]` per match, so a caller gets each wildcard bound to a named sub-node.
`ast_select` returns a flat node set and has no capture column at all.

So the pattern dialect is the only surface that **binds named sub-nodes**.
Deprecating it would remove a capability, not a duplicate. That is a different
decision from the one the first draft described, and the answer falls out of the
measurement.

**Verdict: keep the pattern dialect. `addon_patterns.sql` is not in question,
and stage 3's size does not depend on this.**

### 14.4 Thin on both axes, worth a look

| Macro | test | docs | Note |
|---|---|---|---|
| `ast_select_rules` | 3 | 5 | batch multi-rule form over `ast_select` |
| `ast_select_list` | 3 | 3 | the nested variant of the same |
| `ast_unparse_custom` | 2 | 4 | caller-supplied rules; the only escape hatch for unparse |
| `ast_to_blocks_list` | 2 | 0 | pre-aggregated form |
| `ast_capture` | 1 | 1 | goes with the pattern dialect |
| `ast_pattern` | 1 | 3 | goes with the pattern dialect |
| `ast_get_source_numbered` | 1 | 2 | line-numbered variant of `ast_get_source` |

`ast_select_rules` and `ast_select_list` are the pair most likely to have
drifted, because they wrap `ast_select`, and `ast_select` is what changed.
**Recommendation: check both against the current selector semantics before
2.0.0.** Keep `ast_unparse_custom`: two tests is thin, but it is the only way
to supply unparse rules that no module provides.

### 14.5 Keep all 31 predicates, and test them once

14 of the 31 macros in `semantic_predicates.sql` have 0 test call sites:
`is_annotation`, `is_block`, `is_comment`, `is_directive`, `is_export`,
`is_foreign`, `is_import`, `is_list`, `is_literal`, `is_type_composite`,
`is_type_generic`, `is_type_primitive`, `is_type_reference`. Every one is
documented.

**Recommendation: deprecate none of them.** Each is a one-line scalar over
`semantic_type`, and together they are a complete taxonomy surface. Removing a
subset makes the set arbitrary, and a caller cannot predict which predicate
exists. The real defect is the test gap, and one table-driven test covers all
31 at once. That is a better fix than deprecation, and it is cheap.

### 14.6 The inverse gap: tested but undocumented

`duck_blocks.sql` has **0 docs call sites** for all three of its macros, while
`ast_to_blocks` carries 38 test call sites. `ast_source_of` has 15 tests and 0
docs. `ast_resolve_entity` has 10 tests and 0 docs. `ast_node_edit` has 10
tests and 0 docs.

These are not deprecation candidates. They are a documentation gap, and it runs
the opposite way to §14.2. Recorded here so the two gaps are not confused.

### 14.7 Thin test coverage on macros heading into core

Four macros are going into core, are gaining a `_from` twin, and have 1 or 2
test call sites today: `ast_precedes` (1), `ast_follows` (1), `ast_in_range`
(2), `ast_containing_line` (2). The §9.2 equivalence tests will raise this,
because each new twin needs a non-zero baseline and a planted failure. **No
separate work is needed, but the plan should not assume these macros arrive
well covered.**

### 14.8 Summary, after measuring

Teague asked for this assessment on 2026-11-10 and chose to settle it before
stage 3 rather than after. That was the right order, and the result is that
**nothing in §14 changes stage 3.**

| Macro or surface | First draft said | Measured verdict |
|---|---|---|
| `ast_pattern_list` | drop from the public surface | **keep.** In the #170 gate; no private-macro mechanism exists |
| `ast_resolve` | clearest deprecation case | **keep.** 166 rows, two columns its sibling lacks, not superseded |
| the pattern dialect | Teague's strategic call; 11 macros could go | **keep.** It is the only surface with named captures |
| `ast_select_rules`, `ast_select_list` | check against current selector semantics | unchanged: still worth checking, not deprecating |
| the 31 predicates | keep, add one table-driven test | unchanged |
| `duck_blocks` x3, `ast_source_of`, `ast_node_edit` | documentation gap only | unchanged |

**Why the first draft was wrong, and it is one cause three times.** Every
reversed verdict came from treating a call-site count as the thing itself. A
count could not see the #170 gate's bare-string list, could not tell a
whole-file resolver from a single-symbol one, and said nothing about whether two
dialects express the same queries. Each took one query to settle.

This is the same failure as the "9 of 13 mixed files" in §8.1: a number
standing in for the artifact it describes. `~/.CLAUDE.md` names it directly, and
it has now cost this document four wrong conclusions.

**No deprecation is recommended for 2.0.0 on the evidence gathered here.** The
real findings are three test gaps and one documentation gap, and none of them
blocks stage 3.


---

*Written by Claude Opus 5 on 2026-11-10. Session:
<https://claude.ai/code/session_01FVcSpAnvP7udQhjpSvSPTE>*
