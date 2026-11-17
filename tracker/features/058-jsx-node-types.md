# 058 — Declare the JSX node types

**Status:** approved, stance A. Implementation in progress.
**Branch:** `feat/jsx-node-types` (from main `1a1daff`).
**Raised by:** Teague, 2026-11-14 — "let's properly define the node types", item 3 of
four after PR #329 added the `tsx` language.
**Relates to:** #329 (tsx added), PR #332 (the tsx/typescript grammar fix),
`tracker/bugs/063` (the regeneration defect), feature 049 (language support parity).

---

## 1. Situation, measured

`tsx` became a first-class language in PR #329, merged 2026-11-13. Its grammar is
tree-sitter-typescript's own `tsx` dialect. JSX syntax therefore reaches three of
our languages, and `read_ast` gives every JSX node the fallback classification.

The tsx grammar declares exactly **8** named `jsx_*` node types. The javascript
grammar declares the same 8. The typescript grammar declares **none** — that is the
whole point of the ts/tsx split.

```
$ python3 -c "import json; d=json.load(open('grammars/tree-sitter-typescript/tsx/src/node-types.json')); \
print(sorted(t for t in {n['type'] for n in d if n.get('named')} if 'jsx' in t))"
['jsx_attribute', 'jsx_closing_element', 'jsx_element', 'jsx_expression',
 'jsx_namespace_name', 'jsx_opening_element', 'jsx_self_closing_element', 'jsx_text']
```

Exactly **one** of the 8 is declared today:

```
languages/javascript/javascript_types.def:1270:
DEF_TYPE("jsx_text", PARSER_CONSTRUCT, NODE_TEXT, NONE, 0)
```

The other 7 fall back to `PARSER_CONSTRUCT` with no name. Measured against the
repo's own fixture `test/data/tsx/component.tsx`: 9 of 171 nodes are `jsx_*`, and
all 9 report `PARSER_CONSTRUCT`.

### One declaration reaches all three languages

`languages/typescript/typescript_types.def:60` holds
`#include "../javascript/javascript_types.def"`, and `TSXAdapter` takes
TypeScript's node configs unchanged (`languages/tsx/tsx_adapter.cpp`: "Everything
but the grammar is TypeScript's"). So a single entry in `javascript_types.def`
classifies the type for javascript, typescript and tsx at once. The typescript
rows are dead, exactly as `jsx_text`'s row is dead there today.

### Measured tree shapes

```
<div className="x">        jsx_element > jsx_opening_element > identifier(div)
                                                             > jsx_attribute > property_identifier(className)
<br/>                      jsx_self_closing_element > identifier(br)
<Foo.Bar>                  jsx_opening_element > member_expression > identifier(Foo) . property_identifier(Bar)
<svg:circle/>              jsx_self_closing_element > jsx_namespace_name > identifier(svg) : identifier(circle)
<>...</>                   jsx_element > jsx_opening_element('<' '>')        -- a fragment: NO tag child
<input disabled {...rest}/> jsx_self_closing_element > jsx_attribute > property_identifier(disabled)
                                                    > jsx_expression > spread_element
xlink:href="#z"            jsx_attribute > jsx_namespace_name    -- NOT property_identifier
```

Four consequences the generic name strategies cannot handle:

1. A tag is one of three node types: `identifier`, `member_expression`,
   `jsx_namespace_name`.
2. `FIND_IDENTIFIER` descends two wrapper levels
   (`src/language_adapter.cpp`, `FindIdentifierInWrappers(node, content, 2)`).
   For `<Foo.Bar>` it returns `Foo`, and for `<svg:circle>` it returns `svg`.
   Both are plausible wrong answers, the failure class this repo keeps paying for.
3. A fragment's opening element has no tag child. Its name must be empty.
4. `FIND_PROPERTY` finds nothing on a namespaced attribute such as `xlink:href`.

### The two-adapter trap — this is the #329 class of problem again

`JavaScriptAdapter` and `TypeScriptAdapter` are **siblings**, both direct children
of `LanguageAdapter` (`src/include/language_adapter.hpp:150,191`). `TSXAdapter`
inherits from `TypeScriptAdapter` (`:214`). They do not share `ExtractNodeName`.

- `.jsx` files route to `javascript` (measured) and therefore to
  `JavaScriptAdapter::ExtractNodeName`.
- `.tsx` files route to `tsx` and therefore to `TypeScriptAdapter::ExtractNodeName`.

Both methods end in `ExtractByStrategy(node, content, config->name_strategy)`, and
the generic `CUSTOM` case returns the empty string silently
(`src/language_adapter.cpp:710`). So a `CUSTOM` strategy declared once and
implemented in one adapter gives `.jsx` and `.tsx` different answers, with no
error. Any JSX naming logic must live in one shared helper that both overrides
call, and the tests must assert a non-empty name in **both** languages.

---

## 2. The ruling: is JSX markup, or is it code?

> **RULED 2026-11-16, Teague: stance A, code semantics.** A JSX element is a call
> site and a JSX attribute is a named argument. Option A's table below is the
> approved mapping. Scope stays as section 3 states it: declarations only, with no
> `css_selectors.sql` change.


Every mapping below follows from one choice, so it is one question.

`languages/html/html_types.def` is the markup precedent:

```
DEF_TYPE("element",          ORGANIZATION_BLOCK,  CUSTOM,    FUNCTION_WITH_PARAMS, 0)
DEF_TYPE("start_tag",        PARSER_DELIMITER,    CUSTOM,    FUNCTION_WITH_PARAMS, 0)
DEF_TYPE("attribute",        DEFINITION_VARIABLE, CUSTOM,    VARIABLE_WITH_TYPE,   NAME_DEFINITION)
```

`javascript_types.def` and `python_types.def` are the code precedent, and they say
a named argument is **not** a definition:

```
python  DEF_TYPE("keyword_argument",     ORGANIZATION_LIST, FIND_IDENTIFIER, NONE,              0)
js      DEF_TYPE("pair",                 LITERAL_STRUCTURED|MAPPING, NONE,   NONE,              0)
js      DEF_TYPE("property_identifier",  NAME_IDENTIFIER,   NODE_TEXT,       VARIABLE_WITH_TYPE, NAME_REFERENCE)
```

**Why this is not cosmetic.** `is_definition()` keys off the *semantic type*, not
the flag (`src/semantic_type_functions.cpp:150-157`), and five shipped macros
filter on it: `file_utilities.sql:145`, `tree_navigation.sql:97,311,344,371`. Under
HTML's mapping, `<div className="x" id="y">` adds two **variable definitions**
named `className` and `id` to every component file, and they flow into the
name-binding walk and the #222 shadowing ledger.

### Option A — code semantics — **APPROVED**

| raw_type | semantic_type | name | native | flags |
|---|---|---|---|---|
| `jsx_element` | `COMPUTATION_CALL` | CUSTOM (tag) | NONE | 0 |
| `jsx_self_closing_element` | `COMPUTATION_CALL` | CUSTOM (tag) | NONE | 0 |
| `jsx_opening_element` | `PARSER_DELIMITER` | CUSTOM (tag) | NONE | 0 |
| `jsx_closing_element` | `PARSER_DELIMITER` | CUSTOM (tag) | NONE | 0 |
| `jsx_attribute` | `NAME_ATTRIBUTE` | FIND_PROPERTY | NONE | 0 |
| `jsx_namespace_name` | `NAME_QUALIFIED` | NODE_TEXT | NONE | 0 |
| `jsx_expression` | `PARSER_CONSTRUCT` | NONE | NONE | 0 |

`<Foo/>` compiles to `React.createElement(Foo, …)`, so `ast_select('.call#Foo')`
finds every use of a component. `NAME_ATTRIBUTE` is the taxonomy's own slot for
"decorative metadata (@decorator, @media, attributes)"
(`src/include/semantic_types.hpp:83`); `typescript_types.def:443` and
`python_types.def:327` already use it for `decorator`.

**Cost, measured:** `test/sql/conformance/call_naming_conformance.test` derives its
gate from the runtime — "a node type declares call naming iff its strategy is other
than 'none'" — and pins the declaration table. Declaring two call-semantic types
adds rows for javascript, typescript and tsx to that pinned table, and obliges the
kit to prove JSX elements are named. That is a coordinated test change, not a
surprise, but it is the most expensive of the three options.

### Option B — markup semantics

Mirror `html_types.def` exactly: `ORGANIZATION_BLOCK` for the two element types,
`PARSER_DELIMITER` for the tags, `DEFINITION_VARIABLE` + `NAME_DEFINITION` for
`jsx_attribute`. Consistent with HTML, so one mental model covers both markup
dialects. Costs the definition inflation described above, and it contradicts
`keyword_argument` and `property_identifier` in the same files.

### Option C — hybrid

`ORGANIZATION_BLOCK` for the elements (markup framing, no call-naming gate to
extend), `NAME_ATTRIBUTE` and no definition flag for the attribute. Cheapest of
the three. JSX elements stay invisible to call queries.

---

## 3. Scope: this phase declares types, and nothing else

`ast_select('div')` will **not** reach JSX after this work, under any of the three
options. The reason is one gate:

```sql
-- src/sql_macros/css_selectors.sql:193-199
CASE WHEN language = 'html'
          AND type IN ('element', 'script_element', 'style_element')
          AND name IS NOT NULL AND name != ''
     THEN name ELSE type END AS type,
CASE WHEN language = 'html'
          AND type IN ('element', 'script_element', 'style_element')
     THEN list_filter(parameters, lambda p: p.name = 'id')[1].type
     ELSE name END AS name
```

Widening it is a separate change with its own problems, held for a later phase:

- The rewrite replaces `type` with the tag, so widening it makes
  `ast_select('jsx_element')` stop matching named elements. That works today.
- `#id` matching reads the `parameters` column, which only a native extraction
  strategy fills. HTML's `FUNCTION_WITH_PARAMS` dispatches into
  `html_native_extractors.hpp`; javascript's and typescript's own specializations
  parse function parameters, not JSX attributes, and TypeScript's file holds zero
  references to JavaScript's. So `div#id` cannot match JSX until a JSX attribute
  extractor exists. This phase declares `NONE` for native extraction throughout.
- CSS parses `Foo.Bar` as a tag plus a class, so member-expression components
  cannot be selected by tag at all.
- `src/sql_macros/css_selectors.sql` currently has **uncommitted edits** in the
  `perf/decorrelate-pseudo-classes` worktree. Another session is in that file.

---

## 4. Gates a `.def` edit trips

Each is a coordinated test or generated-doc change. Every count below gets
**measured after** the change, never predicted — PR #329 shipped two red gates
because three generators were not run.

| Gate | Why it moves |
|---|---|
| `test/sql/bugs/issue_222_shadowed_declarations.test` | the declared-type ledger for javascript, and for typescript/tsx through the `#include` |
| `docs/scripts/generate_language_reference.py` | generated language reference; this gate was red at #329 |
| `scripts/generate_taxonomy.py` | taxonomy tables; a CI gate in `MainDistributionPipeline.yml` |
| `scripts/check_def_type_tables.py` | the `.def` table checker; a CI gate in `MainDistributionPipeline.yml` |
| `scripts/conformance_kit.sh` + `spec/layout/layout.yaml` | per-language declaration counts |
| `test/sql/conformance/native_strategy_declarations.test` | new native-strategy declarations (all `NONE` here) |
| `test/sql/conformance/capability_declarations.test` | per-language capability rows |
| `test/sql/conformance/call_naming_conformance.test` | **Option A only** — new call-semantic types must prove naming |
| `test/sql/ast_unparse.test` | only if unparse keys on semantic type; to be checked before editing |

`scripts/audit_unreachable_declarations.py` was run before this design and reports
`TOTAL: 0 unreachable of 3912 declarations`. It attributes a `javascript_types.def`
entry to javascript, so the new entries stay reachable and the audit will not
complain. Note that the same run reports `tsx  0  0`: the auditor reads
`languages/<lang>/*_types.def`, and tsx owns no table, so it is invisible to this
check. That is a gap in the auditor, not a result about tsx.

## 5. New tests this feature owns

- `test/sql/languages/jsx_node_types.test` — the 8 types, classified, in **both**
  `javascript` and `tsx`, over one `.jsx` fixture and one `.tsx` fixture. Each tag
  shape asserted by name: `div`, `Foo.Bar`, `svg:circle`, and the fragment's empty
  name. A namespaced attribute (`xlink:href`) asserted explicitly.
- The two-language assertion is the guard against the #329 class: a helper
  implemented in one adapter and not the other passes a tsx-only test.

## 6. What this feature deliberately does not do

- No `css_selectors.sql` change, so `ast_select('div')` still misses JSX.
- No JSX native extraction, so `parameters` stays empty for JSX and `div#id`
  cannot match.
- No `languages/tsx/tsx_types.def`. One entry in `javascript_types.def` reaches all
  three languages; a tsx-only table would be the only one in the tree.

---

## 7. What shipped

Branch `feat/jsx-node-types`, from main `1a1daff`.

| commit | what |
|---|---|
| `ede1d75` | this design, with the ruling open |
| `103e50b` | the contract as a RED test, plus the twin fixtures |
| `4fb1e21` | the seven declarations, the shared name helper, the regenerated reference |
| `e174b2b` | the three ledgers that moved, each with its measured value |

### The approved mapping, as implemented

| raw_type | semantic_type | name | native | flags |
|---|---|---|---|---|
| `jsx_element` | `COMPUTATION_CALL` | CUSTOM | NONE | 0 |
| `jsx_self_closing_element` | `COMPUTATION_CALL` | CUSTOM | NONE | 0 |
| `jsx_opening_element` | `PARSER_DELIMITER` | CUSTOM | NONE | 0 |
| `jsx_closing_element` | `PARSER_DELIMITER` | CUSTOM | NONE | 0 |
| `jsx_attribute` | `NAME_ATTRIBUTE` | **CUSTOM** | NONE | 0 |
| `jsx_namespace_name` | `NAME_QUALIFIED` | NODE_TEXT | NONE | 0 |
| `jsx_expression` | `PARSER_CONSTRUCT` | NONE | NONE | 0 |

### One deviation from the approved table

`jsx_attribute` takes **CUSTOM**, not `FIND_PROPERTY`. A namespaced attribute
puts its name in a `jsx_namespace_name`, not a `property_identifier`, so
`FIND_PROPERTY` returns the empty string for `xlink:href`. The semantics the
ruling settled are unchanged: `NAME_ATTRIBUTE`, and no definition flag. The
shared helper already existed for the four element and tag types, so reading
either child costs three lines. An empty name for a real attribute is the kind
of silent gap this feature was written to remove.

### Measured, after the change

```
test/sql/languages/jsx_node_types.test       All tests passed (209 assertions)
test/sql/bugs/issue_222_shadowed_declarations All tests passed (92 assertions)
test/sql/conformance/call_naming_conformance  All tests passed (221 assertions)
test/sql/conformance/native_strategy_declarations  passed, unchanged (26)
test/sql/conformance/capability_declarations       passed, unchanged (76)
test/sql/ast_unparse.test                          passed, unchanged (49)
scripts/check_def_type_tables.py             OK, 3846 entries, no duplicates
scripts/generate_taxonomy.py                 no change
scripts/audit_unreachable_declarations.py    0 unreachable of 3926 (was 3912)
scripts/audit_leaf_text_gaps.py              no change
conformance_kit.sh --languages javascript,typescript,tsx
                                             ok=24 FAIL=0 dirty=0 void=0 UNDECL=0
clang-format and format.py on jsx_name_extraction.hpp   clean
```

`make format-check` reports 398 files, which is **more files than exist** in
`FORMAT_DIRS` (309). It is red on this tree for reasons that predate this work,
and no CI job runs it — `.github/workflows/` holds no reference to
`format-check` or `format.py`. The new header passes `format.py` when checked
on its own, and `clang-format --style=file` changes nothing in it.

### Two findings recorded and left alone

  - `tracker/bugs/064` — the two adapters return different `modifiers` and
    `signature_type` for byte-identical source, on four non-JSX node types.
    Found by this feature's own whole-row comparison. Predates it.
  - `tracker/bugs/065` — `tsx` is the only built-in language with no generated
    reference page, because the generator reads `.def` tables and tsx owns none.
    `audit_unreachable_declarations.py` is blind the same way.
