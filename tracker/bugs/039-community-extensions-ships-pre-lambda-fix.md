# 039 — community-extensions ships v1.10.2; the lambda fix is in v1.10.3

**Status:** open — **no code change needed here.** The fix is already on `main` and
already tagged. What is missing is a community-extensions descriptor bump.
**Found:** 2026-09-05, from duckeye (downstream consumer of `ast_select`)
**Filed by:** duckeye session, not a sitting_duck maintainer — please sanity-check

## Summary

`bc0c859 fix(macros): modern lambda syntax in ast_qualified_name_as_string (#104)`
(2026-08-31) converted the last single-arrow lambda in the shipped macros. It is
contained in tag **v1.10.3**.

community-extensions pins sitting_duck at **v1.10.2**, ref
`6bff057cbf1e9b475029068e41e9a4ebfcb92797`, which **predates** the fix:

    git merge-base --is-ancestor bc0c859 6bff057   ->  false
    git describe --tags 6bff057                    ->  v1.10.2

So every installed sitting_duck still carries the deprecated arrow. This is a
distribution lag, not a defect in the source.

## Reproduce (against the INSTALLED extension, not a source build)

    duckdb -c "SET lambda_syntax='DISABLE_SINGLE_ARROW'; LOAD sitting_duck;
               SELECT count(*) FROM ast_select('somefile.sh','function_definition');"

    Binder Error: Deprecated lambda arrow (->) detected. Please transition to the
    new lambda syntax ... before DuckDB's next release.

Verified on **two machines** — this workstation and longbottom — both running the
community build `b148903`. Source on longbottom is clean: `semantic_predicates.sql:183`
and `duck_blocks.sql:186` both use `lambda s:` / `lambda p:`, and no arrow lambda
remains in `src/sql_macros/`. The arrow survives only in the `trees/feat-ast-patch`
worktree.

## Why it matters downstream

duckeye must keep `SET lambda_syntax='ENABLE_SINGLE_ARROW'` at both of its load sites
solely for `ast_select`. Its own SQL is now arrow-free (duckeye `ba88bc7`), and with the
pragma forced off the suite is 182/4 — all four failures are `-Q` (AST selector) cases,
none of them duckeye's code.

That pragma is itself the deprecated escape hatch. If DuckDB v2.0 removes the setting,
the `SET` statement *itself* errors and takes down **every** duckeye invocation — a
worse failure than the single lambda it was covering. So the pragma cannot simply be
left in place as the safe option.

## Suggested fix

Bump `extensions/sitting_duck/description.yml` in duckdb/community-extensions to
v1.10.3 (ref = the v1.10.3 tag commit). No sitting_duck source change required.

## Please do NOT "fix" these

A naive grep for `->` in `src/sql_macros/` returns two more hits that are **correct as
written** and would break if converted:

- `duck_blocks.sql:195` — `' -> '` is a **string literal** used when rendering a
  function signature (`THEN ' -> ' ELSE ': '`).
- `json_group_structure` — `json_structure(...) -> 0` is the **JSON operator**.

Only `list_transform(qn, s -> ...)` in `semantic_predicates.sql` was ever a lambda, and
it is already converted.
