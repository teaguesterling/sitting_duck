# 059 — The parameter-extraction cluster: eight issues, four causes

**Status:** plan, awaiting approval. No code written.
**Scope chosen by:** Teague, 2026-11-16, from four candidate clusters among the
47 open issues.
**Issues:** #316, #317, #319, #320, #321, #322, #323, #324, and #255 on a
separate axis.
**Branch:** none yet. Phase 1 would branch from main.
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

**#319 and #320 are one defect in two languages.** No issue says so, because
#319's filed cause pointed elsewhere.

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

## 4. The two ledgers every fix must move

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

The 13 `L5` rows are **not** extractor gaps. They are declarations on
argument/parameter container nodes, which no implementation can satisfy; they
belong to `tracker/bugs/042` step 5 and are out of scope here.

---

## 5. Proposed phasing, one pull request each

| phase | issues | why grouped | size |
|---|---|---|---|
| **1** | #319, #320 | one defect, two languages, measured here for the first time. Fixes silently-short arity, the worst symptom in the cluster | small |
| **2** | #317, #324, #321's `ref` half | all S2, all through `declarator_walk.hpp`, after promoting the two type-text helpers into it | medium |
| **3** | #323, #316 | both S4. #323 is one word plus its `has_extends` effect; #316 needs diagnosis and a bigger lambda fixture first | medium |
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
there can change names well outside java and typescript. Measured here:
**24 of the language tables declare `FIND_IDENTIFIER`**
(`grep -c FIND_IDENTIFIER languages/*/*.def`, counting files with a non-zero
count), so option 1 reaches 24 languages to fix two. Option 2 is narrower but duplicates walk logic a third time.

**That measurement is phase 1's first task, and its result decides the fix.**
I will not choose between them from the issue text.

---

## 6. What this plan does not do

  - No `params` reassembly from loose C# sibling tokens.
  - No change to the 13 `L5` baseline rows: those are `tracker/bugs/042`'s.
  - No schema widening in phases 1 to 3, so `is_variadic` stays invisible from
    SQL until #255 is taken.
  - No work on #250 (dart/graphql calls), which the baseline file lists beside
    this cluster but which is a different subsystem.
