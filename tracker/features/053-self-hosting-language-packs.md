# 053 — Self-hosting the language packs

**Status:** open, for consideration. Raised by Teague on 2026-11-02:
`sitting_duck` (everything) and `sitting_duckling` (minimal) belong in
community-extensions; the per-language packs do not need to clutter that
registry.

Mechanism surveyed from <https://github.com/tobilg/duckdb-extension-self-hosting>
and <https://duckdb.org/docs/2.0/extensions/extension_repositories> on
2026-11-02. Everything below marked *documented* comes from one of those two;
the tensions are mine.

## What the RFC already says, and what this changes

`docs/planning/v2-architecture.md` already settles the build question:

> Packs are build outputs, not repos. We do not ship per-language extensions by
> default. … One or two flagship packs get built to prove the `INSTALL` path;
> demand pulls the rest.

So Teague's instinct matches the RFC. What self-hosting changes is the
**distribution** row of the four-doors table, which currently reads:

| Door | Mechanism | Status |
|---|---|---|
| Pack extension | `INSTALL sitting_duck_<lang>` — a compiled module wrapped as its own DuckDB extension; **rides DuckDB signing/distribution** | flagship proof only |

Self-hosting replaces "rides DuckDB signing/distribution" with "rides ours". That
is not a small edit — the three consequences below are what the decision turns
on.

## The mechanism, documented

A GitHub Action (MIT) signs a build with **your own RSA-2048 key** and publishes
it to S3-compatible storage laid out as a DuckDB custom extension repository:

```
.well-known/duckdb-extension-repo.json
<duckdb_version>/<platform>/<name>.duckdb_extension.gz
<name>/<extension_version>/<duckdb_version>/<platform>/<name>.duckdb_extension.gz
```

Clients install **signed**, with no `-unsigned` flag:

```sql
SET allow_extension_repositories = 'allowed';
CREATE EXTENSION REPOSITORY IF NOT EXISTS sd_packs
    WITH PREFIX 'https://packs.example.com';
INSTALL sitting_duck_rust FROM sd_packs;
LOAD sitting_duck_rust FROM sd_packs;
```

Tested by its author against SeaweedFS and real Cloudflare R2. Platform and
DuckDB version are read from each binary's own footer, not from artifact names.

## Three things it costs us

### 1. It costs the autoload story, permanently

Documented: **bare `LOAD <ext>` and autoloading do not work** for a custom
repository, and `UPDATE EXTENSIONS` skips these extensions (`FORCE INSTALL` is
the upgrade path). A per-machine `CREATE EXTENSION REPOSITORY` is required once.

Today a user types `INSTALL sitting_duck FROM community; LOAD sitting_duck;`.
A self-hosted pack is three statements plus a one-off repository definition, and
`LOAD sitting_duck_rust` alone will never work. For the *packs* that may be an
acceptable trade — they are an opt-in, demand-pulled artifact — but it means
packs and the two registry extensions behave differently, forever, and the docs
have to say so.

### 2. Version coupling is the number that decides viability

Documented ABI rule: **CPP and `C_STRUCT_UNSTABLE` builds load only in the exact
DuckDB version they were built for.** `C_STRUCT` builds against the stable C API
load in later versions, and the action can publish one binary into several
version directories.

sitting_duck is a **CPP** extension and deeply so — the `duckdb` language adapter
wraps DuckDB's *own parser* and its `CreateASTNode` works on `SQLStatement` and
`ParsedExpression` (see `043`). A pack carrying native extraction inherits that.
So unless packs can be built against the stable C API — which I have **not**
established and doubt — the matrix is:

```
packs × platforms × every DuckDB version we support
```

That is the figure to compute before adopting this. If it is 28 × 4 × N, the
answer is not "self-host", it is "do not ship 28 packs".

### 3. We would hold a signing key, with no revocation

Documented: trust on first use, **no revocation, no expiry, no rollback
protection, no sandbox**. A second reserve key is recommended. The key signs
native code that executes in the user's process.

That is the same conversation as `register_language`'s trust boundary, which
`docs/reference/module-abi.md` already states plainly — the gate controls
*whether* and *from where*, never *what*. Self-hosted packs add a *fourth*
thing we would be asserting (provenance) with a mechanism that cannot withdraw
the assertion. Worth deciding deliberately rather than inheriting.

Also: DuckDB's own documentation marks custom repositories **work in progress**.

## The alternative we already own, and when each applies

This is the part worth carrying forward. **We have two pack-shaped distribution
mechanisms, and self-hosting only serves one of them.**

| | runtime `.so` (`register_language`) | pack extension (self-hosted) |
|---|---|---|
| artifact | `libtree-sitter-<lang>.so` + a JSON config | a signed DuckDB extension |
| distribution | any HTTPS URL, a tarball, a package manager | a custom extension repository |
| signing | none; gated by `sitting_duck_enable_runtime_grammars` + `allowed_directories` | our RSA key, trust on first use |
| DuckDB version coupling | **none** — only tree-sitter's language ABI range | exact, for a CPP build |
| what the user gets | `semantic_type`, `name`, `flags` | all of that **plus** native extraction |
| what the user does not get | `parameters`, `signature_type`, `modifiers` — `native_strategy` is refused in a runtime config | — |

So the rule that falls out:

- **A pack that needs native extraction must be a compiled extension**, and
  self-hosting is how it reaches users without cluttering community-extensions.
- **A pack that needs only types, names and flags needs none of this today.** It
  ships as `.so` + JSON, documented in
  [`docs/how-to/custom-languages.md`](../../docs/how-to/custom-languages.md),
  with no registry entry, no key, and no DuckDB version coupling.

The second row is probably most of the demand, and it is already shipping. That
reframes the question from "how do we distribute 28 packs" to "which languages
actually need native extraction badly enough to pay the compiled-extension
cost".

## What would have to be decided

1. **Can a pack be built against the stable C API (`C_STRUCT`)?** If yes, version
   coupling largely disappears and this becomes attractive. If no, compute the
   matrix before anything else. **Unknown; blocks the rest.**
2. Does `sitting_duckling` go to community-extensions as a second descriptor, as
   the RFC's M3 says? That is independent of this and unaffected.
3. If packs are self-hosted: where, and who holds the key. R2 with a custom
   domain is the tested path (not `r2.dev`).
4. Do we accept that packs never autoload, and say so in their docs?

## Not in scope

Adopting it now. There is no pack to distribute until M3 extracts
`sitting_duck_languages` and a module can be built standalone, and the RFC puts
that behind the split-after-stability rule. This entry exists so the
distribution question is already answered when the first pack is buildable,
rather than being discovered then.
