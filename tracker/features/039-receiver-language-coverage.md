# 039 — `[receiver=X]` / native `receiver` — language coverage follow-ups

**Context:** #86 landed the native `receiver` field (the object a method is invoked on)
and the `[receiver=X]` selector filter, via a **general, operator-driven rule** in
`src/include/function_call_extractor.hpp`:

- separator matched by **source text**, grammar-agnostic:
  member `.` `->` `?.` `?->` ⇒ receiver = object's trailing simple name;
  scope `::` ⇒ **no** receiver (free/static/qualified call, no runtime object).
- bare calls, computed/subscript objects, and chained call-results decline (NULL).
- `TrailingSimpleName` rejects call-result object nodes by type (self-enforcing — do
  NOT rely on config omission; see the guard comment).

Adversarial verification (all 27 languages × 11 construct types) found **zero
wrong-value bugs**. Everything below is an **under-match** (NULL where a name was
reasonable) or a **by-design** consequence — never a wrong answer.

## Fully working (receiver resolves via the general rule)
python, javascript, typescript, java (incl. `this`), c#, cpp, c, rust, go, php, ruby*
— simple/multi-level member, self/this, nested-object trailing name, `::` decline,
subscript/chain/constructor decline, C/C++ `->` and `(*p).m()`, JS/TS optional `?.`.

## By-design (qualifier reached via `.`, not a runtime object) — product call, not a bug
- Java `Foo.bar()` → `Foo`; `java.util.Objects.equals(...)` → `Objects`
- Go `pkg.Func(x)` → `pkg`
The text rule can't distinguish a class/package qualifier from an object when the
separator is `.`. Emitting the qualifier name is defensible; revisit if undesirable.

## Under-match follow-ups (NULL today; all need per-grammar wiring, not logic changes)
1. **Kotlin / Swift** — member `.` is nested inside a `navigation_suffix` one level
   below the children `FindSeparator` scans on the `navigation_expression`. Needs
   `FindSeparator`/shape handling to descend into the suffix (or config to expose it).
2. **Ruby `self.m()` / `self.db.x`** — `self` and the nested paren-less `call` object
   node aren't in ruby `function_name_types`. Wiring them is safe ONLY because
   `TrailingSimpleName` now rejects call-result objects by type (the getter-chain trap
   is guarded) — but verify before adding.
3. **R** — member separator is `$` (`obj$method`); add `$` to `IsMemberSep` and the
   `extract_operator`/access node to R's `function_name_types`.
4. **PHP null-safe `$o?->m()`** — node `nullsafe_member_call_expression` is tagged
   `PARSER_CONSTRUCT` in `php_types.def`, not `COMPUTATION_CALL`, so it never routes to
   the FUNCTION_CALL strategy. Needs a `php_types.def` DEF_TYPE (COMPUTATION_CALL |
   FUNCTION_CALL) + the matching `nullsafe_member_access_expression`. Semantic-typing
   change with broader effects (call-graph/semantic queries) — do deliberately + retest.
5. **Dart / Lua / Bash** — no discrete call node reaches the FUNCTION_CALL strategy
   (Dart `selector` chains, Lua `function_call`/no config entry, Bash commands not
   tagged as calls). Broader parser/config work; no receivers today.

## Also noted
- The disabled `test/sql/schema_compatibility_validation.test.disabled` hard-codes the
  native-column count; bump it for `receiver` if/when it is re-enabled.
- The hierarchical / nested-struct output shapes (`read_ast_hierarchical`,
  `parse_ast_hierarchical`, `ToValue`) deliberately omit `receiver` (independent return
  types, verified no crash/mismatch). Add there only if those shapes need it.

---

# #64 Phase 3 prep — per-language PARAMETER inventory (2026-09-09)

Empirical probe (`function f(aa, bb)` per language) — every language flags parameters
as NAME_REFERENCE today (universal gap). Two fix mechanisms, split by where the param
name sits:

**List-based** (identifier directly in the param *list* → context hook, add the list
type to `IsBareNameDefinitionParent<Adapter>`; safe because typed/defaulted params live
in per-param wrappers, not directly in the list — verified JS `bb=CONST` → parent
`assignment_pattern`, and its default value `CONST` correctly stays a use):
- python `parameters` / `lambda_parameters` — DONE
- javascript `formal_parameters`
- ruby `method_parameters`
- lua `parameters`

**Wrapper-based** (per-param wrapper node → set `NAME_DEFINITION` on that node in the
`.def`, Phase-1 style; VERIFY the node is param-only first, like the splat check):
- php `simple_parameter` — ALREADY DEF (the reference pattern)
- typescript `required_parameter` (+ `optional_parameter` to check)
- java `formal_parameter` (+ `spread_parameter`?)
- cpp / c / go `parameter_declaration`
- rust / csharp / kotlin / swift / r `parameter`
- dart `formal_parameter`

**Also per language** (Phase-1-style distinct binding-wrapper nodes, same as Python's
aliased_import / as_pattern_target / splats): import aliases, catch/except-clause vars,
destructuring/binding patterns, and (JS/TS) `variable_declarator` / `property_definition`,
Go `const/let`/short-var, Java/C# `field_declaration`. Needs a per-language sub-inventory
(one probe each) before editing — the ambiguity check (node type appears ONLY in a
binding context) is mandatory per node type.

**Still deferred for ALL languages** (needs the design work, not per-language grind):
for-loop targets (field-position aware), destructuring bind targets that are bare
identifiers, walrus/assignment-expr targets, and the IS_SCOPE model.

**Execution note:** wrapper-based languages are pure `.def` edits (cheap, low-risk after
the param-only check). List-based languages each add one line to `IsBareNameDefinitionParent`.
Defaulted/typed params in list-based langs still need their wrapper (e.g. JS
`assignment_pattern`) handled — do NOT use a broad "parent contains 'parameter'" rule; it
mis-flags default values as definitions.
