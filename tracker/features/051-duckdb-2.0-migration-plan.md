# 051 — DuckDB 2.0 migration plan

**Status:** PLAN ONLY — nothing in here has been executed
**Created:** 2026-10-15
**Trigger:** the community registry moved to the 2.0 line today, not on the 21st
**Blocks on:** a decision about `tracker/bugs/040`, and a fleet-wide ABI decision

> Teague asked for a plan including the submodule repoint. **The repoint
> contradicts a standing instruction** ("never repoint the shared duckdb
> submodule pin, `069cc9f9b5b`"), so no step below is executed without an
> explicit, specific go-ahead. This document is the thing to argue with.

## 1. What changed, verified from upstream

`duckdb/community-extensions#3079` ("2.0 Release Prep") merged
**2026-10-15T15:42:03Z**. Effects, all read from the upstream API rather than a
local fork:

- **`build.yml`'s default `duckdb_version` is no longer a tag.** It is now
  `b15358c371daf10dad799a43b6167f6ccdce84d8` — duckdb/duckdb `b15358c371da`,
  2026-10-09, "Support canonical ANY signatures in aggregate-state SQL
  (#26783)". Same literal at lines 78, 81, 111, 135, 159.
- **`build_next.yml`'s `test_against_latest` job is now `if: false`.** The
  `ref_next` path is dead for the moment.
- `v2.0.0` is **not tagged** in duckdb/duckdb yet (a compare against it 404s).
  Branch `v2.0-cyanoptera` currently heads at `b9bfa2934e72`.

The registry's default build target therefore moved from a v1.5.6 tag to a
2.0-line commit **six days before the announced date**.

### The descriptor schema, from `scripts/build.py:42-54` (authoritative)

```python
extension_ref = desc['repo']['ref']
if   DUCKDB_VERSION == 'v1.4.5':  extension_ref = repo.get('andium', ref)
elif DUCKDB_VERSION == 'v1.5.6':  extension_ref = repo.get('variegata', ref)
elif DUCKDB_VERSION != DUCKDB_LATEST_STABLE:
    if repo.get('ref_next'): extension_ref = repo['ref_next']
    elif REQUIRE_REF_NEXT:   extension_ref = None   # skip
```

So: **`ref` is the primary, used for the default (now 2.0-line) build.
`variegata` is the per-version override for v1.5.6 builds, `andium` for v1.4.5.
`ref_next` only applies to a prerelease version — and the only job that set
that path is now disabled.**

This matches the account relayed from the fleet session, and I verified it
rather than taking it: the mechanism is a per-version override keyed on
`DUCKDB_VERSION`, not a rename.

## 2. The current true state

Our descriptor (upstream, authoritative):

```yaml
version: 1.15.4
repo:
  ref:      803b35b5e4b8cec4b464169d4135648ce8a74875
  ref_next: 803b35b5e4b8cec4b464169d4135648ce8a74875
```

So the registry will build **`803b35b5` against the 2.0 line**, and carries no
`variegata`, so there is no v1.5.6 override.

`duckdb-pin` caught the change within minutes — main's 14:21Z run was green, the
15:33Z run red — and after the parse fix (PR #264) it now says the true thing:

```
the community registry builds default=b15358c371daf... next=v2.0-cyanoptera
but this repo ships default=v1.5.6 next=v2.0-cyanoptera
```

**main stays red until this is settled.** That is deliberate.

### What IS already verified green on the 2.0 line

| commit | what it is | v2.0-cyanoptera build |
|---|---|---|
| `803b35b5` | the current registry ref | **green** — run 37157449013 (linux_amd64, wasm_eh, windows_amd64) |
| `b49238e` | v1.16.0, what PR #3065 would ship | **green** — run 38566432103 (linux_amd64, windows_amd64, windows_amd64_mingw, wasm_eh) |
| `3bb6488` | main today | linux_amd64 green |

So **compiling is not the risk.** The v2.0 leg has existed since before the
registry ref and is enforced by `duckdb-pin` (it fails if no job ships
`DUCKDB_NEXT`).

## 3. The actual blocker: bugs/040

`tracker/bugs/040` — open, **ship-blocking for the "v2.0-compatible" claim**.
Measured, re-measured 2026-10-08 against cyanoptera `d4c9dfd469f`:

| | v1.5.6 | 2.0 line |
|---|---|---|
| `ast_select` per call | 0.47 s | **13.0 s** (~27×) |
| `css_selectors_multilang.test` | 162 s | **2060 s** |

It is a **planning** cost, independent of input size and selector complexity,
upstream (`duckdb/duckdb#26036`), reproducing on both cyanoptera and duckdb
main. It is a latency regression, not a correctness one — so it sails through
builds and says nothing in a compile-only CI leg.

### The open question that must be measured FIRST

The workflow sets `skip_tests: true` on the v2.0 leg precisely because of this.
**But on today's runs that leg's `Test` step ran and passed** — ✓, not skipped —
in 40m06s against the v1.5.6 leg's 25m20s. Either `ci_tools_version: main` no
longer honours `skip_tests`, or the bare-named/tuple-named job attribution is
wrong. `gh run view --log --job=...` returned empty for both a fresh and an old
job, so **this is unresolved and must not be assumed either way.**

If tests really do pass on the 2.0 line in 40 minutes, bugs/040's practical
severity has changed and this whole migration gets much cheaper. If they do not,
nothing below is viable yet. **Everything in §5 is gated on settling this.**

## 4. The constraint the repoint runs into

`duckdb-pin`'s own rationale (fleet rule, 2026-09-13): the submodule is pinned
at exactly the commit the registry builds every extension with, because
**extensions built in different repos cannot co-load locally — the ABI is
exact-version.** Measured then: five sibling repos on a July dev commit, one on
v1.5.6-dev, one on duckdb main.

Nine repos under `~/Projects` carry the identical `duckdb-pin` job and the same
stale parse: `duckdb_markdown`, `duckdb_mcp`, `duckdb_panduck`, `duckdb_webbed`,
`duckdb_yaml`, `duckdb_zim`, `duck_tails`, `duckdb_duck_block_utils` (+ a
worktree duplicate), and `sitting_duck`.

**So repointing sitting_duck alone breaks co-load with the other eight.** This
is a fleet decision with a lockstep requirement, not a per-repo one. It is the
single biggest reason not to rush it.

## 5. Options, with what each costs

### A. Hold at v1.5.6 and add `variegata` (lowest risk)

Add `variegata: <v1.5.6-good commit>` to the descriptor so v1.5.6 builds keep
working, and let `ref` be built against the 2.0 line as the registry now does.
No submodule change, no fleet coordination.

- Keeps a working v1.5.6 artifact.
- Does **not** make the 2.0-line build good — it will compile and be slow
  (bugs/040).
- Honest, and buys time. **Recommended as step 1 regardless of the end state.**

### B. Migrate the pin to the registry's commit (what was asked to be planned)

Repoint `duckdb` from `069cc9f9b5b` (v1.5.6) to `b15358c371daf...`, flip
`DUCKDB_VERSION` to that commit, and restructure the two build legs.

Consequences, in order of how much they hurt:

1. **Fleet ABI break** until the other eight repos move to the same commit. Must
   be simultaneous or co-load is broken for everyone (§4).
2. **bugs/040 ships.** A 27× `ast_select` planning cost is the product's
   headline feature. Migrating without fixing or mitigating it means shipping a
   version where selectors are unusable at scale.
3. `duckdb-pin` asserts the pin equals a *tag's* commit
   (`git ls-remote --tags .../${DUCKDB_VERSION}`). A SHA target needs that
   assertion restructured, not just its constants changed.
4. The `DUCKDB_VERSION` constant feeds a stray-literal scan keyed on
   `v[0-9]+\.[0-9]+\.[0-9]+`; a bare SHA satisfies none of it, so the scan's
   semantics need revisiting too.
5. Every local build directory and every cached artifact becomes ABI-stale.

### C. Wait for v2.0.0 to be tagged, then migrate to the tag

`v2.0.0` is not tagged yet. Migrating to a *tag* avoids §5-B-3 and §5-B-4
entirely, because `duckdb-pin`'s existing tag-resolution assertion keeps
working. If the registry switches its default from the current SHA to the
`v2.0.0` tag once it exists, this is strictly less work than B.

- **Risk:** the registry may keep a SHA default indefinitely, in which case this
  is waiting for something that never comes. Worth asking the registry
  maintainers directly rather than inferring.

## 6. Sequenced plan

Each step gated; nothing proceeds on assumption.

1. **Land PR #264** (parse fix). Done, open. Makes the check tell the truth.
2. **Settle the `skip_tests` question** (§3). Dispatch one v2.0 run and capture
   the log while it is live — `gh workflow run MainDistributionPipeline.yml`,
   then read the job log immediately rather than after expiry. Determines
   whether the suite passes on the 2.0 line at all.
3. **Re-measure bugs/040 against `b15358c3` specifically** — not against
   cyanoptera's head, and not against the Oct-8 commit. The registry builds that
   exact commit, so that is the number that matters. `build/release-v2.0-cyanoptera`
   already exists locally, so this is a rebuild, not new infrastructure.
4. **Decide A / B / C** on the evidence from 2 and 3. My current lean is
   **A now, then C** — add `variegata`, keep the pin, and migrate to a tag when
   one exists — because B's fleet-ABI cost is paid once and cannot be undone
   cheaply, and because bugs/040 is a product problem that a pin change does not
   solve.
5. **Fix PR #3065 for the new schema** (see §7) regardless of which option wins.
6. If B or C: **write the fleet lockstep plan** before touching any submodule —
   which repos, what order, how co-load is verified afterwards.

## 7. PR #3065 is now schema-stale

My open registry PR (`duckdb/community-extensions#3065`, sitting_duck 1.16.0)
sets `ref` **and** `ref_next` to `b49238e`. Under the schema in §1:

- `ref_next` now does nothing — `build_next.yml` is `if: false`.
- `ref` is built against the **2.0 line**, so `b49238e` would be a 2.0-line
  build (it does compile there — verified, §2).
- There is **no `variegata`**, so the v1.5.6 artifact would come from `ref` too,
  which under the new keying is not what happens — v1.5.6 builds take
  `variegata` if present and `ref` otherwise.

So #3065 should be rewritten as `ref: b49238e` + `variegata: b49238e`, dropping
`ref_next`. That is a small edit to an open PR, and it is **the one externally
visible action in this document**, so it waits for a go-ahead too.

## 8. What I am NOT doing

- Not repointing the submodule. Standing instruction, and §4.
- Not editing the descriptor or PR #3065 yet.
- Not landing a parse fix into the other nine repos — the *comparison* question
  should be settled once, not nine times, and a fleet of repos silently
  agreeing on a wrong assertion is worse than nine red checks with a known cause.
- Not renaming the v2.0 CI jobs to say "v2.0", though they should be: they render
  as bare `Build extension binaries / linux_amd64`, which is why a fleet sweep
  concluded sitting_duck had no v2.0 leg at all. The YAML warns that
  required-check configuration may key on job names, so that is its own change.

## 9. Unknowns, stated

- Whether the suite passes on the 2.0 line (§3). **The gating unknown.**
- Whether the registry's default stays a SHA or moves to the `v2.0.0` tag.
- Whether `b15358c3` is on the cyanoptera line's history; a compare reported
  cyanoptera 1340 commits ahead and 0 behind, which I could not reconcile with
  a 2026-10-09 date and so am not relying on.
- bugs/040's severity on `b15358c3` specifically — every existing measurement is
  against a different commit.
