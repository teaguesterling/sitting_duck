# 059 — The parameter-extraction cluster: eight issues, four causes

**Status:** phase 1 implemented. Phases 2 to 4 still plan only.
**Phase 1 branch:** `fix/319-320-nested-parameter-names` (#319, #320, #323).
**Phase 1 corrected four claims in this plan.** Section 7 holds them, and the
corrections are applied in place above, so no section below contradicts it.
**Scope chosen by:** Teague, 2026-11-16, from four candidate clusters among the
47 open issues.
**Issues:** #316, #317, #319, #320, #321, #322, #323, #324, and #255 on a
separate axis.
**Branch:** phase 1 branched from main at `c8d48d7`.
**Every figure below was measured against `build/release/duckdb` at `1f3939f`,
not taken from the issue bodies.** Two issue bodies turned out to be wrong, and
that is the first finding.

---

## 1. Two issue bodies are wrong, and re-measuring found it

The issues were filed at `9d5301d`, `6ea3a09` and `7353c95`. Main has moved a
long way since. All eight symptoms still reproduce, but two stated causes do
not survive a read of the current source.

**#319 (java varargs) says:** "The function's native extractor iterates
`formal_parameter` children only, so the node is never pushed."

**Measured:** it does not. `src/include/java_native_extractors.hpp:128` already
has a `spread_parameter` arm, which calls `ExtractSpreadParameter`. The arm
fires. What drops the parameter is the guard two lines later:

```cpp
} else if (strcmp(child_type, "spread_parameter") == 0) {
    param = ExtractSpreadParameter(child, content);
    is_valid_param = !param.name.empty();      // <- this
}
```

`ExtractSpreadParameter` looks for a **direct `identifier` child** for the
name. The grammar does not put one there:

```
spread_parameter       d
  integral_type        int
  ...
  variable_declarator  d      <- the name is in here
    identifier         d
```

So the name comes back empty and the guard silently drops the whole parameter.

**#322 (zig) asks to check first** whether `function_declaration` declares
`FUNCTION_WITH_PARAMS`. **Measured: it declares `NONE`**
(`languages/zig/zig_types.def:82`), and `src/include/zig_native_extractors.hpp`
**does not exist**. So this is not a conformance violation at all. It is an
honest declared gap, and the fix is to implement an extractor and change the
declaration together, exactly as #263 did for kotlin's `primary_constructor`.

---

## 2. The four causes

### S1 — the name is one wrapper deeper than the lookup, and a guard then drops the parameter

| issue | language | the wrapper | measured result |
|---|---|---|---|
| #319 | java | `variable_declarator` | 4 declared, 3 reported |
| #320 | typescript | `rest_pattern` | 3 declared, 2 reported |

Measured at `1f3939f`:

```
java/319   ['a : int', 'b : String[]', 'c : java.util.Map<String,Integer>']
ts/320     ['a : number', 'b : string']
```

Both lose the last parameter, and nothing marks the list as partial. This is
the worst of the four causes for a caller, because arity is silently short: a
query counting parameters gets a wrong number with no error.

**#319 and #320 are one SHAPE in two languages, not one fix.** No issue says
so, because #319's filed cause pointed elsewhere. The two need different layers:
java's wrapper is handled inside `ExtractSpreadParameter`, because
`variable_declarator` was **already** in `FindIdentifierInWrappers`'s list and
the java path never consults it; typescript's needs both a new
`ExtractRequiredParameter` arm and a new entry in that list. Measured during
phase 1, after this section was first written.

**tsx carries #320 as well, and no issue says so.** `TSXAdapter` inherits
`TypeScriptAdapter`'s node configs, so the same signature reported 1 parameter
in tsx where typescript reported 2.

### S2 — the extractor names node types instead of following grammar fields

| issue | what |
|---|---|
| #317 | C has its own parameter extraction, written independently of C++'s. Measured: `const char* s` → `char*` in C and `const char*` in C++; `int** pp` → `int*` named `param` in C and `int**` named `pp` in C++ |
| #324 | A C++ function returning a function pointer reports the **returned type's** parameters as its own, because the outer `function_declarator`'s `parameters` field belongs to the return type |
| #321 (half) | C# drops `ref` from a parameter's type. Same convention question in a third language |

**The fix already exists and is already shared.** `src/include/declarator_walk.hpp`
holds `FindDeclaratorIdNode`, which #318 moved there precisely so two layers
could reach it. Its own header comment states why the walk cannot make #324's
mistake:

> a `function_declarator`'s own parameter list is a SEPARATE `parameters`
> field. So following the field can never descend into a function pointer's
> parameters and mistake one of those for the name.

**The open precondition in #317 is now settled.** #317 asks to confirm that the
C grammar uses the same declarator field names. Measured from both grammars'
`node-types.json`:

```
            array_declarator  function_declarator  init_declarator  pointer_declarator
c           declarator,size   declarator,parameters  declarator,value  declarator
cpp         declarator,size   declarator,parameters  declarator,value  declarator
```

Identical. C simply lacks four C++-only forms (`reference_declarator`,
`variadic_declarator`, `variadic_parameter_declaration`,
`optional_parameter_declaration`), which the shared walk already handles and
which can never appear in C. So `FindDeclaratorIdNode` works on C unchanged.

`NormalizeDeclaredType` and `DeclaredTypeText` are still static members of
`CppNativeExtractor`, so C cannot reach them. Promoting those two beside
`FindDeclaratorIdNode` is the whole of S2's foundation.

### S3 — a declared capability with no implementation at all

#322 zig. `function_declaration` declares `NONE`; no zig native extractor
exists. Measured: `fn probe(a: i32, b: []const u8) void {}` reports
`len(parameters) = 0`, while the AST rows for both `parameter` nodes are
present and correctly named.

This is the largest single task in the cluster: a new extractor header.

### S4 — the implementation names a node the grammar never emits

| issue | what |
|---|---|
| #323 | `src/include/kotlin_native_extractors.hpp:555` tests for `delegation_specifiers` — **plural**. The grammar emits `delegation_specifier`, singular, as a direct child. Still present at `1f3939f`. Two effects: a kotlin class loses its parent types, and `has_extends` is never set, so `ExtractKotlinClassModifiers` never pushes its `extends` marker either |
| #316 | C++ lambdas report zero parameters. **Cause not established**, by the issue's own admission. Needs diagnosis, not a fix |

`spec/conformance/native-decl-baseline.tsv` carries a warning about #316 worth
heeding: its row reads `seen=1`, so the whole observation rests on **one lambda
in the entire conformance corpus**. The fixture has to grow before the
measurement means anything.

---

## 3. Not extractor defects

**#321, the `params` half.** Measured in the issue and not re-measured here:
the C# grammar emits **no `parameter` node** for a `params` parameter, and zero
ERROR nodes — one `parameter` node for two parameters. There is nothing for an
extractor to read. The issue's own direction is right: report upstream to
tree-sitter-c-sharp or accept it as a declared gap, and do **not** reassemble
the parameter from loose sibling tokens.

**#255, the schema.** `read_ast`'s `parameters` column is
`STRUCT(name VARCHAR, type VARCHAR)[]` — two of `ParameterInfo`'s six fields.
`is_variadic`, `default_value`, `is_optional` and `annotations` are populated in
C++ and unreachable from SQL. One logical type is declared in four places,
three wide and one narrow, and the narrow one at
`src/unified_ast_backend.cpp:621` is what `read_ast` exposes.

This matters to this cluster more than its issue number suggests:
`ExtractSpreadParameter` already sets `param.is_variadic = true`, and no SQL
query can see it. Widening the struct is user-visible, though — it changes the
column's type signature, so every test that casts `parameters::VARCHAR` or
digests a row moves. It belongs in its own pull request, not folded into a fix.

---

## 4. The ledgers every fix must move

**Section 4 was written as "the two ledgers". There are three.** Phase 1 found
the third in CI, not locally, because `scripts/verify_native_dispatch.py` is a
workflow step and not part of `make test`. A full green suite cannot see it.

Both already exist, and both fail loudly when a fix lands. That is by design,
and it is why this cluster is tractable.

**`test/sql/conformance/parameter_binding_multilang.test`** (286 lines). Section
3, "The gaps, pinned so that fixing one is visible", holds #317/#318, #319,
#320, #321 and #322 with controls. Fixing any one makes this file fail. The
expectation moves in the same commit as the fix, and the commit says so.

**`spec/conformance/native-decl-baseline.tsv`** (16 pairs). Its contract is
**bidirectional**: an unlisted declared-and-undelivered pair fails the run, and
**a listed pair that stops reproducing also fails the run**. So a fix that
satisfies a baselined pair must delete its row in the same commit. The rows this
cluster touches:

```
cpp   function_declarator  L3   #324's node: "a gap in the extractor rather than a wrong declaration"
cpp   lambda_expression    L3   #316, seen=1 -- one observation
dart  method_signature     L3   #250, adjacent, not in this cluster
```

**`spec/abi/module-abi.yaml`, the `dead_dispatch` list** (68 rows before phase
1, 67 after). It records every dispatch literal that no grammar emits, keyed on
`(check, language, literal)` and not on a line number. Its contract is
bidirectional in the same way: the gate fails on a new dead literal **and** on a
baselined one the run no longer reproduces. #323 deleted
`kotlin / delegation_specifiers`, so that row had to go in the same change.

This list also decides how a dead arm may be retired. Phase 1 labelled
typescript's `rest_parameter` arm in place rather than deleting it, so its row
at `dead_dispatch` still reproduces and the baseline stays honest. Deleting the
arm would have required deleting the row with it.

The 13 `L5` rows are **not** extractor gaps. They are declarations on
argument/parameter container nodes, which no implementation can satisfy; they
belong to `tracker/bugs/042` step 5 and are out of scope here.

---

## 5. Proposed phasing, one pull request each

| phase | issues | why grouped | size |
|---|---|---|---|
| **1** | #319, #320, #323 | #319/#320 share a shape and fix silently-short arity, the worst symptom in the cluster. #323 joined on Teague's instruction, 2026-11-16: it is also S4-shaped and also one wrong node-type name | small |
| **2** | #317, #324, #321's `ref` half | all S2, all through `declarator_walk.hpp`, after promoting the two type-text helpers into it | medium |
| **3** | #316 | S4. Needs diagnosis and a bigger lambda fixture before any fix. #323 moved to phase 1 | medium |
| **4** | #322, #255, #321's `params` half | a new zig extractor; a user-visible schema widening; one upstream report | large, and separable |

**Recommended start: phase 1.** It is the smallest change, it fixes the symptom
that gives callers a wrong number with no error, and it needs nothing from the
other three phases.

### Phase 1, in detail

**Files:** `src/include/java_native_extractors.hpp`,
`src/include/typescript_native_extractors.hpp` (or the shared name lookup — see
the open question), `test/sql/conformance/parameter_binding_multilang.test`.

**Oracle, before any implementation:** extend
`parameter_binding_multilang.test` section 3 with the two arity assertions as
they should read, confirm they fail, then fix. Both repros are one line of SQL
and both are already in the file as pinned wrong answers, so the red state is
free.

**The open question phase 1 must answer first.** #320 offers two fixes and
prefers the second:

  1. add `rest_pattern` to `FindIdentifierInWrappers`'s wrapper list in
     `src/language_adapter.cpp` — one line, keeps the heuristic, and reaches
     every language that uses `FIND_IDENTIFIER`;
  2. move the parameter nodes onto a field-based declarator walk.

Option 1's blast radius is the thing to measure before choosing: that wrapper
list serves `FIND_IDENTIFIER` for every language that declares it. The source
comment beside the strategy puts its reach at "FIND_IDENTIFIER's 206 node
types" — that is the repo's own figure, not one I measured — so a wrapper added
there can change names well outside java and typescript.
**24 of the language tables declare `FIND_IDENTIFIER`**
(`grep -c FIND_IDENTIFIER languages/*/*.def`, counting files with a non-zero
count).

**"24 languages to fix two" was the wrong denominator, and this plan said it
first.** The list is consulted by node-type NAME, so a type absent from a
grammar can never match it. Measured in phase 1:

  - 4 of 30 grammars emit `rest_pattern` at all: dart, javascript, typescript,
    tsx;
  - of every node type that can own one, exactly one declares
    `find_identifier`: `required_parameter`, in typescript and tsx;
  - every two-hop path is blocked structurally, because each one passes through
    `formal_parameters`, `array_pattern`, `object_pattern` or `list_pattern`,
    and none of those is in the list.

Net reach: one node type, two languages. **Option 1 was chosen on that
measurement**, and the measurement now lives beside the list in
`src/language_adapter.cpp`, where the next person adding a wrapper will read it.

---

## 6. What this plan does not do

  - No `params` reassembly from loose C# sibling tokens.
  - No change to the 13 `L5` baseline rows: those are `tracker/bugs/042`'s.
  - No schema widening in phases 1 to 3, so `is_variadic` stays invisible from
    SQL until #255 is taken.
  - No work on #250 (dart/graphql calls), which the baseline file lists beside
    this cluster but which is a different subsystem.

---

## 7. Phase 1, as built

Branch `fix/319-320-nested-parameter-names`, four commits on main at `c8d48d7`:
red-first tests, the fix, the expectations the fix invalidated, and the
blast-radius note. Implementation and expectations never moved in the same
commit.

### Behaviour, measured before and after

| language | case | before | after |
|---|---|---|---|
| java | `void m(int a, int b, int... d)` | 3 params | 4 params, `d : int[]` |
| typescript | `f(a: string, b: number, ...c: number[])` | 2 params | 3 params, `...c : number[]` |
| tsx | the same signature | 1 param | 2 params, `...c` |
| kotlin | `class D : A(), I` | `[]` | `[A, I]` |
| kotlin | `class D : I by impl` | `[]` | `[I]` |
| kotlin | the `extends` modifier | never set | set when the class has a parent |

Two of those six rows are in no issue: the tsx row, and the `by` delegation
form.

### Four claims in this plan that phase 1 corrected

  1. **#319 and #320 are not one fix.** Same shape, different layers.
     `variable_declarator` was already in the shared wrapper list, so java
     needed its extractor changed and typescript needed both layers. Corrected
     in section 2, S1.
  2. **"24 languages to fix two" used the wrong denominator.** The measured
     reach is one node type in two languages. Corrected in section 5.
  3. **#323 is not one word.** The plural-to-singular change alone passes a
     naive test and still fails `class D : I by impl`, because that form nests
     the type inside an `explicit_delegation` node. `ExtractTypeName` needed an
     arm for it. Section 3 of the test file exists to catch exactly that
     partial fix.
  4. **#320 reaches tsx as well as typescript**, through adapter inheritance,
     and no issue says so.

### The through-line

All three issue bodies named a cause that was not there, and in each case the
code looked like it handled the input it was dropping: java's
`spread_parameter` arm, typescript's `rest_parameter` arm for a node type the
grammar never emits, and kotlin's `delegation_specifiers`. Re-running the repro
found each one. Reading the diagnosis did not.

### Two conventions recorded, not changed

  - **The `extends` marker is kotlin's convention alone.** Kotlin pushes
    `extends` into `modifiers`; java's modifiers read `[]` for
    `class D extends A {}` and java instead stores the inheritance kind in
    `ParameterInfo.type` as `extends` or `implements`. Both are asserted in
    `test/sql/bugs/323_kotlin_parent_types.test`, in both directions, so a
    later change that unifies them has to say it meant to.
  - **The kotlin grammar has 136 named node types**, or 263 entries counting
    anonymous ones. Three mention delegation and none is plural. The larger
    figure is the one easily mistaken for a node-type count.

### Verification

`parameter_binding_multilang.test` 42 assertions, `323_kotlin_parent_types.test`
30, `263_kotlin_constructor_params.test` 38. A 33-file sweep over the bug and
language tests for the affected languages found one failure, `263`, since fixed
and verified. The two `require-env` conformance files pass with
`SITTING_DUCK_TEST_GRAMMAR_DIR` set, 66 and 76 assertions, rather than skipping.
Full suite on longbottom: `All tests passed (10777 assertions in 201 test
cases)`, taken before the rebase onto `c8d48d7`. CI on the pull request is the
gate for the rebased tree.

**CI caught one thing the green suite could not.** `module ABI in sync` failed
on `1 stale baseline entry`: `spec/abi/module-abi.yaml` still listed
`kotlin / delegation_specifiers` as a dead dispatch literal, which #323 had
just deleted. The row is now removed, and `verify_native_dispatch.py` reports
`67 known-dead literal(s) still exactly as spec/abi/module-abi.yaml records
them, and no new ones`. The lesson is in section 4: that gate is a workflow
step, so a local `make test` run can never reach it.
