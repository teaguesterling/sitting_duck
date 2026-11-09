# Brief — the dead-dispatch worklist: 68 node names the grammars never emit

**Status: idea, not specced, not scheduled.** Nothing here is approved. Start
with `superpowers:brainstorming` to decide *whether* and *in what order* this is
worth doing, because the honest answer may be "fix a handful, close the rest as
intended". Then get a plan approved before writing code, per this repo's
CLAUDE.md.

---

## Situation

`sitting_duck` is a DuckDB extension that parses source code with tree-sitter
and exposes the syntax tree as SQL tables. For each of 28 languages it keeps a
`.def` file declaring, per syntax-node type, how to find that node's name and
which "native extraction" strategy fills in its parameters, return type and
modifiers. The extraction itself lives in per-language C++ headers that dispatch
on node-type **string literals** — `strcmp(child_type, "parameter_declaration")`
and so on.

A node name that the grammar never emits makes such a branch unreachable. The
code compiles, the dispatch runs, the branch never fires, and no test fails —
because the output is merely empty rather than wrong. The project calls this
shape "the #220 shape", after the issue where it was first found in three
languages at once.

There is a checker for it: `scripts/verify_native_dispatch.py`, run by the
`native dispatch in grammar` gate in `workspace/preflight/ci_cheap_gates.sh`. It
reads every dispatch literal out of the C++ and checks it against the grammar's
`node-types.json`. It currently checks **1397 literals across 27 languages**.

## Task

The checker works, and its findings were written down and then left. There is a
baseline list in `spec/abi/module-abi.yaml` under `dead_dispatch:` with **68
entries**, each a node-type literal that the corresponding grammar does not
emit. The file's own comment says:

> "In most of these the correct name is live in the same symbol table, which is
> the oracle that proves the finding: typescript dispatches on `parameter` while
> the grammar emits `required_parameter`; cpp on `class_definition` against
> `class_specifier`; c on `function_specifier` against
> `storage_class_specifier`; css on `media_feature_name` against
> `feature_name`. Fixing them is #042's own work, one language per change,
> expectation first. This records the ground truth; it does not excuse it."

So the baseline is a worklist that has never been worked. Every entry is a
feature that was declared, dispatched, and silently delivers nothing.

## Action

Release v1.18.3 removed exactly one entry from this list, as a side effect.
Issue #315 deleted a C++ branch for `default_parameter_declaration` — a name
occurring **zero** times in that grammar's `node-types.json`, `grammar.json` and
`grammar.js` — and the gate then failed because its baseline entry no longer
reproduced, which is the gate working as designed. The entry was removed and the
baseline regenerated with the file's own documented command:

```
python3 scripts/verify_native_dispatch.py --emit-baseline
```

That took the list from 69 to 68.

## Result

68 entries remain. They are measured, recorded, and inert: the gate is green
*because* they are baselined, not because they are fixed. Nobody has worked
through them, and they span most of the fleet's languages.

---

## Your prompt

Decide whether this list is worth working, and if so, produce an ordered plan.

Start by sizing it honestly, because "68 dead dispatch literals" is not
automatically 68 bugs. Some will be genuine features silently missing. Some will
be leftovers from a grammar upgrade, where the right fix is to delete the branch
rather than rename it. Some may be deliberate — a branch kept for a grammar
variant that is not compiled in. Expect all three, and expect the ratio to
decide whether this is one change or fifteen.

For each entry worth fixing, the measurement that turns it from "a literal that
does not match" into "a user-visible defect" is the same one v1.18.3 used
repeatedly: find the node type the grammar *does* emit, write a test asserting
the correct output, confirm it fails, then fix the dispatch. A dead branch with
no observable consequence is a cleanup; a dead branch whose feature a user can
ask for is a bug.

## Already measured — do not re-derive

The 68 entries, by language and by check kind:

| | |
|---|---|
| total | **68** |
| by check kind | `dispatch` 58, `table` 10 |
| sql | 13 |
| swift | 11 |
| typescript | 7 |
| kotlin | 5 |
| cpp, csharp, php, r | 4 each |
| lua, python | 3 each |
| css, markdown, ruby | 2 each |
| c, go, java, rust | 1 each |

The checker's own census, from the most recent gate run:

```
checked 1397 ts_node_type-bound dispatch literals in 27 languages,
191 literals in 15 shared-table entries, 15 instantiated tags
not judged: 103 comparison sites whose left operand is not a node type
no native extractor header: graphql, hcl, json, markdown, toml, yaml, zig
excluded, no tree-sitter grammar behind the adapter: duckdb, dynamic_language
```

Two entries in that list are already known to be real, user-visible defects,
both found in v1.18.3 and both filed rather than fixed:

- **kotlin `delegation_specifiers`** — issue **#323**. The plural form does not
  exist among that grammar's 263 node types; the real names are
  `delegation_specifier`, `explicit_delegation` and `constructor_delegation_call`.
  Consequence: every Kotlin class loses its parent types and its `extends`
  marker. Java, from the same strategy, is correct, which is the control that
  makes it a defect rather than a missing feature.
- **typescript `parameter`** vs the emitted `required_parameter` — related to
  issue **#320**, where a rest parameter is dropped from the parameter list and
  its row carries an empty name.

So at least two of the 68 are confirmed defects with issues already open. That
is the evidence that the list is worth reading rather than closing.

Also worth knowing: a related gap was noted in v1.18.3 and never filed. The
native-strategy dispatch handles **11 of the 17** strategy enumerators. Only 7
are declared anywhere, and all 7 are handled, so nothing is broken today — but
Kotlin *has* a `GENERIC_FUNCTION` extractor that the dispatch cannot reach, so
declaring that strategy would silently return an empty result. Same shape,
waiting to happen. Filing that is a reasonable first deliverable.

## Limits agreed up front

- **One language per change**, which the baseline's own comment already asks
  for. A sweep across seventeen languages in one commit cannot be reviewed and
  cannot be bisected.
- **Expectation first.** Write the failing test before the fix, and confirm it
  fails where you predicted. Every fix in v1.18.3 did this and it caught two
  cases where the obvious fix was wrong.
- **Never regenerate the baseline to make the gate quiet.** Removing an entry is
  only correct after the literal is genuinely fixed or deleted. The regeneration
  command exists for after the work, not instead of it.
- Route builds and tests through the `blq` MCP tools, and git/GitHub through
  `jetsam`.
- **One trap worth inheriting:** a blq call that goes to the background
  sometimes reports *the previous run's output body* while carrying its own
  metadata — seven instances during v1.18.3. The test runner's `Filters:` line,
  printed inside the output, is what catches it; when it disagrees with what you
  asked for, read the result from blq's store (join `invocations` to `outputs`
  on the invocation UUID) rather than re-running.

## The first question to settle

**Is each entry a missing feature, a stale leftover, or deliberate?** The answer
decides the shape of the whole piece of work: missing features need a test and a
fix each, leftovers need deleting, and deliberate ones need a comment in the
baseline saying so. Classify all 68 before fixing any, because the ratio tells
you whether this is one afternoon or fifteen separate changes — and a
classification pass is cheap, since it needs only the grammars and the C++, no
builds.

## Links

- The baseline: `spec/abi/module-abi.yaml`, under `dead_dispatch:`
- The checker: `scripts/verify_native_dispatch.py`; the gate that runs it,
  `workspace/preflight/ci_cheap_gates.sh`
- Issues already open against entries in the list: `gh issue view 323`,
  `gh issue view 320`
- The originating issue for the shape: `gh issue view 220`
- Release record with the eight instances found in v1.18.3:
  `tracker/roadmap/v1.18.3-name-binding.md`
- Originating session: https://claude.ai/code/session_01FVcSpAnvP7udQhjpSvSPTE
