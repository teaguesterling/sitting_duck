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
| `803b35b5` | the current registry ref | **green on all 9 platforms** — run 37157449013 |
| `b49238e` | v1.16.0, what PR #3065 ships | **green on all 9 platforms** — run 38566432103 |
| `3bb6488` | main today | linux_amd64 green |

The 9 are linux_amd64, linux_arm64, osx_amd64, osx_arm64, windows_amd64, windows_amd64_mingw, wasm_mvp, wasm_eh, wasm_threads — the full matrix, on both lines, at both registry-relevant
refs. **Correction to an earlier claim of mine:** I reported the v2.0 leg as
covering only 3-4 platforms with no osx/arm64/wasm_mvp/wasm_threads coverage.
That came from reading one *pull_request* run's partial job list instead of
measuring; push-to-main runs build the full matrix. The coverage gap I asserted
does not exist.

And the leg is testing the right line: **`b15358c3` is an ancestor of
`v2.0-cyanoptera`** (`compare/b15358c3...v2.0-cyanoptera` → `behind_by = 0`,
`ahead_by = 1340`), so the `v2.0-cyanoptera` CI leg builds a *descendant* of
exactly the commit the registry builds. Compile-compat evidence here transfers.

So **compiling is not the risk.** The v2.0 leg has existed since before the
registry ref and is enforced by `duckdb-pin` (it fails if no job ships
`DUCKDB_NEXT`). What is missing is any *test* evidence — see §3.

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

### RESOLVED: tests are NOT running on the v2.0 leg

This looked like an open question and is not. On today's runs the v2.0 leg's
`Test` step shows **✓ (ran, succeeded)** rather than `-` (skipped), in 40m06s
against the v1.5.6 leg's 25m20s — which looked like `skip_tests: true` being
ignored by `ci_tools_version: main`.

It is honoured, just at a different layer. `_extension_distribution.yml@main`
declares `skip_tests` (:138), exports it as `CI_SKIP_TESTS` (:234), and then runs
the step **unconditionally** — `run: python3 extension-ci-tools/scripts/ci_phase.py test`
— where `ci_phase.py:795` does the skipping:

```python
def test(self) -> None:
    if self.enabled("CI_SKIP_TESTS"):
        print("Tests skipped by workflow input.")
        return
```

So the step runs a no-op and reports success. By contrast
`@v1.5-variegata` gates the test steps in YAML
(`if: ${{ ... inputs.skip_tests == false }}`, four sites), which is why the
stable leg's behaviour reads differently.

**Consequences:**

- The 40m06s is build and setup. **No test has ever run against the 2.0 line
  in this repo's CI.**
- `bugs/040`'s status is **unchanged** — nothing here contradicts the 27×
  measurement, and nothing here supports it either.
- "Compiling is not the risk" (§2) stands, but it is *all* we have. There is
  **zero** test evidence on the 2.0 line, which is a weaker position than a
  green v2.0 leg suggests to anyone reading the check names.

My first reading posed this as "either ci-tools ignores `skip_tests` or my job
attribution is wrong". Both were wrong: the third possibility — honoured one
layer down, so the step still reports as run — is what was happening. A ✓ on a
step named `Test` is not evidence that tests ran.

## 3b. The registry's own test phase is running ZERO tests, for everyone

Measured 2026-10-15 while watching the registry build our rewritten #3065.

The registry built `ref` (`b49238e`) against its own 2.0 target — `duckdb_version:
b15358c3`, `duckdb_tag: v2.0.0-alpha46057`, `ci_tools_version: v2.0-cyanoptera`
— and went green on `build / linux_amd64`, `windows_amd64`, `wasm_eh`
(run 38799112177). **That is real compile evidence from the registry's own
infrastructure, and it is the closest thing to a dry run of the 21st.**

Its `Test` step is a no-op, and unlike ours it is not even deliberate:

```
all tests passed in 3s (156 skipped tests)
Skipped tests (156):  require sitting_duck: 156
ran tests: 0 passed, 156 skipped in 3s
```

Every one of the 156 test files skipped on `require sitting_duck` — the
extension is not loadable in that unittest binary — and the harness printed
**"all tests passed"**.

**This is not specific to us, and it is new.** Comparing registry builds either
side of #3079 (merged 15:42Z):

| when | extension | result |
|---|---|---|
| 09:34Z, before | `anofox_decide` | `All tests passed (3523 assertions in 139 test cases)` |
| 09:50Z, before | `altertable` | `All tests passed (34 skipped, 23 assertions in 10 test cases)` |
| 16:45Z, after | `duck_hunt` | `ran tests: 0 passed, 61 skipped` — `require duck_hunt: 61` |
| 20:05Z, after | `sitting_duck` | `ran tests: 0 passed, 156 skipped` — `require sitting_duck: 156` |

So since the 2.0 migration **the community registry's build phase executes no
tests for any extension and reports green.** Six days before v2.0.0 ships, and
invisibly — the step is named `Test`, prints "all tests passed", and shows ✓.

(The outer `Tests are skipped in this run...` line is unrelated and expected:
`duckdb_extension.Makefile:207` sets `SKIP_TESTS=1` when `LINUX_CI_IN_DOCKER=0`,
so the non-Docker invocation defers to the Docker one. The Docker one is the run
quoted above.)

### What this does to this plan

**There is now no test evidence on the 2.0 line from any source**: not our v2.0
leg (skips by design, §3), and not the registry's (skips by accident, here). So
§5's options have to be decided on compile evidence plus `bugs/040`'s local
measurements, and step 3 below — re-measuring 040 against `b15358c3` locally —
is the *only* way to get a number. It is no longer a nice-to-have.

**Reported upstream: duckdb/community-extensions#3086.**

**MECHANISM WITHDRAWN.** I first localised this to the ci-tools line bump
(`TEST_RUNNER := python3 run_tests.py`, `duckdb_extension.Makefile:197-202`).
That attribution is not supported by my own evidence and I have retracted it
upstream. The before/after varied **two** inputs at once — ci-tools
`v1.5-variegata` → `v2.0-cyanoptera` *and* `duckdb_version` tag → bare SHA,
both at #3079 — so it cannot attribute the regression to either. I presented a
confounded comparison as a localisation.

The **observation** is unaffected and is what matters: zero tests executed,
reported as `all tests passed`.

Leading hypothesis now, from a collaborator's tighter experiment on
`duckdb_webbed` (same ci-tools, one input varied):

```
duckdb_version: v2.0-cyanoptera   -> ran tests: 123 passed, 0 skipped
duckdb_version: b15358c371daf...  -> ran tests: 0 passed, 123 skipped
```

Two caveats I could not clear:

- **Not reproduced.** The one completed webbed run I could read (38832481674)
  had *both* legs on the SHA at 0 tests. The `123 passed` figure currently
  appears as a comment in an in-flight PR, not in a run I could verify.
- **The offered explanation is refuted.** It was attributed to the extension
  being built under an artifact name the harness does not look for. In
  `_extension_distribution.yml@main`, `duckdb_version` reaches only the upload
  artifact name and ccache keys — and **Test runs before Upload** (job order
  Build → Test → Upload), so the artifact name cannot affect whether
  `require <ext>` resolves during the test phase.

So the *input* may well be `duckdb_version`; the *route* is unidentified. A
version-keyed extension install/autoload directory
(`~/.duckdb/extensions/<version>/<platform>/`) is the obvious next place to
look, untraced and not asserted.

What survives regardless of cause, and what #3086 now leads with: **fail when
the executed-test count is zero against a non-empty discovery set.**

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

Add `variegata:` to the descriptor so v1.5.6 builds keep working, and let `ref`
be built against the 2.0 line as the registry now does. No submodule change, no
fleet coordination.

**Name the commit explicitly — this is where the option has a hole otherwise.**
Under the schema in §1, omitting `variegata` makes a v1.5.6 build fall back to
`ref`; so if `ref` moves to `b49238e`, the v1.5.6 artifact silently becomes a
commit the registry never built against v1.5.6. The candidates:

| commit | v1.5.6 evidence |
|---|---|
| `803b35b5` | what the registry currently ships and has built against v1.5.6 — the safe choice |
| `b49238e` | v1.16.0; v1.5.6-green on this repo's own CI (full 9-platform stable leg), but never built by the registry |

Recommend `variegata: b49238e` **if** #3065 lands (so v1.5.6 and the 2.0 line
ship the same source, and the 141-commit backlog actually reaches users), and
`variegata: 803b35b5` if it does not. Either way it must be stated, not defaulted.

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
2. ~~Settle the `skip_tests` question.~~ **Done — §3.** Tests have never run on
   the 2.0 line; `skip_tests: true` is honoured inside `ci_phase.py`. So the
   remaining question is not "does CI tell us" but "what happens when we run
   them", which is step 3.
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

**DONE 2026-10-15** (Teague: "If the pr isn't merged yet, rewrite it. Make sure
we have v1.5.6 and upcoming v2.0.0 covered"). #3065 was still open and now sets:

```yaml
repo:
  github: teaguesterling/sitting_duck
  ref:       b49238e...   # default build: the 2.0 line now, v2.0.0 once tagged
  variegata: b49238e...   # v1.5.6 builds
  ref_next:  b49238e...   # a prerelease that is none of the above
```

`ref_next` was **kept**, not dropped as this section first proposed. It cannot
hijack the default build — `DUCKDB_LATEST_STABLE` is set to the same commit as
the default (`build.yml:80`), so `DUCKDB_VERSION != DUCKDB_LATEST_STABLE` is
false there — and keeping it means prerelease validation builds this release
instead of printing "Skipping prerelease validation" if `build_next.yml` is
re-enabled.

Verified by transcribing `build.py:42-54` and running it against the descriptor.
All six paths resolve to a commit: the default, v1.5.6 → `variegata`, v1.4.5
(no `andium` → falls back to `ref`), a future prerelease → `ref_next`, and both
post-v2.0.0-flip cases, where v1.5.6 keeps its own ref.

Note `variegata` is read by `build.py:47` but **no extension in the registry
uses it yet** — the migration is hours old. `andium`, the identical mechanism
for v1.4.5, is in use (`urlpattern`) and is the precedent followed.

## 8. What I am NOT doing

- Not repointing the submodule. Standing instruction, and §4.
- ~~Not editing PR #3065.~~ **Done — §7.** The registry's own CI is now
  building `ref` against the 2.0 line for the first time, which is the one piece
  of evidence no sitting_duck run can produce.
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
- ~~Whether `b15358c3` is on the cyanoptera line's history.~~ **Resolved, and it
  is good news — see §2.** `compare/BASE...HEAD` measures from the merge base, so
  `behind_by = 0` means `b15358c3` is an *ancestor* of `v2.0-cyanoptera`, and
  1340 is simply that branch's commits since. There was no inconsistency with the
  Oct-9 date; I misread the API.
- bugs/040's severity on `b15358c3` specifically — every existing measurement is
  against a different commit.
