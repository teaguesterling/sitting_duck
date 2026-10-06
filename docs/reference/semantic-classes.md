# Semantic Classes Reference

Every `.class` usable in an `ast_select` selector. **This page is generated from the
engine** — the same table the selector validator consults — so it cannot drift from
what the engine accepts:

```sql
SELECT selector, resolves_to, match_kind FROM ast_semantic_aliases() ORDER BY selector;
```

A class that is not in this list **raises** rather than matching nothing:

```
.frobnicate   -> ast_select: unknown semantic class ".frobnicate". Full list: ...
.annotation   -> ast_select: unknown semantic class ".annotation". There is an
                 ATTRIBUTE filter of that name -- did you mean [annotation*="..."]?
```

A class that *is* in the list but matches nothing in your code returns no rows, as in
CSS. Only unknown classes raise.

## How broadly a class matches

`match_kind` says how much of the taxonomy a class covers:

| `match_kind` | Matches | Example |
|---|---|---|
| `exact` | one semantic type | `.fn` → `DEFINITION_FUNCTION` |
| `kind` | a whole kind — several related types | `.def` → every `DEFINITION_*` |
| `quadrant` | a top-level quadrant, the broadest | `.computation` → everything computational |

So `.def` is `.fn` + `.class` + `.var` + `.mod` together, which is what makes
"where is X defined" answerable without knowing whether X is a function or a class.

## Broad classes (`kind` and `quadrant`)

These cover a family of types. They are the ones most often missing from
hand-written documentation, and `.name` is the largest class in a typical source
file — more nodes than `.call`.

| Selectors | Resolves to | Match |
|---|---|---|
| `.computation` | `COMPUTATION` | quadrant |
| `.access` | `COMPUTATION_NODE` | kind |
| `.def` `.definition` | `DEFINITION` | kind |
| `.err` `.error` | `ERROR_HANDLING` | kind |
| `.statement` `.stmt` | `EXECUTION` | kind |
| `.ext` `.external` | `EXTERNAL` | kind |
| `.control` `.flow` | `FLOW_CONTROL` | kind |
| `.lit` `.literal` `.value` | `LITERAL` | kind |
| `.meta` `.metadata` | `METADATA` | kind |
| `.name` | `NAME` | kind |
| `.op` `.operator` | `OPERATOR` | kind |
| `.block` | `ORGANIZATION` | kind |
| `.syn` `.syntax` | `PARSER_SPECIFIC` | kind |
| `.pat` `.pattern` | `PATTERN` | kind |
| `.transform` `.xform` | `TRANSFORM` | kind |
| `.type` `.typedef` | `TYPE` | kind |

## Specific classes (`exact`)

One semantic type each.

| Selectors | Resolves to | Match |
|---|---|---|
| `.attr` `.field` `.member` `.prop` | `COMPUTATION_ACCESS` | exact |
| `.call` `.invoke` | `COMPUTATION_CALL` | exact |
| `.class` `.cls` `.interface` `.struct` `.trait` | `DEFINITION_CLASS` | exact |
| `.fn` `.func` `.function` `.method` | `DEFINITION_FUNCTION` | exact |
| `.mod` `.module` `.namespace` `.ns` `.package` | `DEFINITION_MODULE` | exact |
| `.const` `.let` `.var` `.variable` | `DEFINITION_VARIABLE` | exact |
| `.catch` `.except` `.rescue` | `ERROR_CATCH` | exact |
| `.defer` `.ensure` `.finally` | `ERROR_FINALLY` | exact |
| `.raise` `.throw` | `ERROR_THROW` | exact |
| `.try` | `ERROR_TRY` | exact |
| `.export` `.pub` | `EXTERNAL_EXPORT` | exact |
| `.import` `.require` `.use` | `EXTERNAL_IMPORT` | exact |
| `.cond` `.conditional` `.if` | `FLOW_CONDITIONAL` | exact |
| `.break` `.continue` `.jump` `.return` `.yield` | `FLOW_JUMP` | exact |
| `.for` `.loop` `.while` | `FLOW_LOOP` | exact |
| `.bool` `.boolean` | `LITERAL_ATOMIC` | exact |
| `.num` `.number` | `LITERAL_NUMBER` | exact |
| `.str` `.string` | `LITERAL_STRING` | exact |
| `.array` `.coll` `.dict` `.list` `.map` `.set` `.tuple` | `LITERAL_STRUCTURED` | exact |
| `.comment` | `METADATA_COMMENT` | exact |
| `.label` | `NAME_ATTRIBUTE` | exact |
| `.id` `.ident` `.identifier` | `NAME_IDENTIFIER` | exact |
| `.dotted` `.qualified` | `NAME_QUALIFIED` | exact |
| `.self` `.this` | `NAME_SCOPED` | exact |
| `.arith` `.math` | `OPERATOR_ARITHMETIC` | exact |
| `.cmp` `.comparison` | `OPERATOR_COMPARISON` | exact |
| `.logic` `.logical` | `OPERATOR_LOGICAL` | exact |
| `.comp` `.comprehension` | `TRANSFORM_QUERY` | exact |

## Full semantic type names also work

Any semantic type name is a valid class, which is useful when you want to be
unambiguous or when no short alias exists:

```sql
SELECT count(*) FROM ast_select('src/*.py', '.DEFINITION_FUNCTION');  -- same as .fn
```

## Related

- [Pseudo-Classes](css-pseudo-classes.md) — `:has`, `:is-scope`, `:calls`, …
- [Attribute Selectors](css-attributes.md) — filter by *value* (`[signature=int]`),
  as opposed to a class, which selects by *kind*
- [Semantic Types](semantic-types.md) — the taxonomy these resolve into
