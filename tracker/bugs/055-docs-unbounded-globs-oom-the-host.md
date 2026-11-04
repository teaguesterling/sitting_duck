# 055 — 116 documented examples glob an unbounded tree, and one OOM-killed the machine

**Status:** open. Found 2026-11-03 by running the documentation, not by reading it.

**Severity note.** This is not a wrong answer in a doc. Following the
documentation from inside a repository killed a 125 GB machine. The kernel
record is below.

## What happened

A survey ran every fenced ```sql block in `docs/` against
`build/release/duckdb`, with the working directory at the repo root, to measure
how many documented examples still work. 11 minutes in, the kernel killed it:

```
Nov 03 22:25:34 longbottom kernel: Out of memory: Killed process 3159992 (duckdb)
  total-vm:152045732kB, anon-rss:96400596kB, file-rss:2328kB, ... UID:1000
```

**96 GB resident, 152 GB virtual, one process.** The OOM report's own task list
shows the only other candidate in the same scope was a 462 MB `unittest`, so
there is no ambiguity about which process grew.

## The cause

`docs/` contains **116 call sites** whose source argument is an unbounded
recursive glob, across **14 files**. Counted with:

```sh
grep -rnoE "(read_ast|ast_select|ast_callers|ast_callees)\('\*\*/[^']*'" docs/ --include=*.md
```

| glob | sites | what it reads |
|---|---:|---|
| `**/*.*` | **26** | **every file of every language, recursively, from wherever the reader stands** |
| `**/*.py` | 20 | every Python file |
| `**/*.tf` | 5 | |
| `**/*.java` | 5 | |
| `**/*.cpp` | 5 | |
| `**/*.swift`, `**/*.kt`, `**/*.dart`, `**/*.cs` | 4 each | |
| `**/*.zig`, `**/*.rb`, `**/*.{py,js,cpp}` | 3 each | |
| others | | |

A reader who pastes one of these into a shell sitting in any non-trivial
repository gets whatever that tree holds. In THIS repository, measured:

| glob | files matched | bytes |
|---|---:|---:|
| `**/*.json` (`docs/languages/infrastructure.md:122`) | 9,809 | **382.6 MB** |
| `src/**/*.cpp` (`docs/reference/css-pseudo-classes.md:237`) | 29 | 0.4 MB |

`**/*.json` sweeps every grammar's `node-types.json`, two 14.7 MB
`build/bench_*.json`, and **the same files again inside all 19 worktrees under
`trees/`**. JSON is the most node-dense language the extension parses — every
value is a node — so 382 MB of input to one `read_ast` is consistent with 96 GB
resident.

**Which example fired is not known.** The survey accumulated results and
printed at the end, so its output died with it. 26 `**/*.*` sites alone are each
sufficient. The blocks run in file order, and `docs/explanation/` sorts first,
so `docs/explanation/how-parsing-works.md:184` (`read_ast('**/*.*')`) is the
first unbounded site reached — inferred from ordering, not measured.

## The two defects, which are separate

1. **Documentation.** An example's source argument should be bounded and
   reproducible. `test/data/python/sample_app.py` is used correctly elsewhere in
   the same docs (`docs/tutorials/css-selectors.md:205`), so the convention
   already exists and these 116 sites do not follow it. `ignore_errors := true`
   appears on many of them, which reads as caution and does nothing about size.

2. **The engine has no cap.** `read_ast` accepts an unbounded glob and grows
   until the kernel intervenes. It does not fail, and it does not degrade — it
   takes the host down. This is the same family as **#164** (call-graph
   pseudo-classes exhaust memory on a 78k-node table and kill the process rather
   than erroring). There is an unpushed local branch `fix/parse-resource-caps`
   in #164 territory.

   A DuckDB `memory_limit` does not help by itself here: the growth is in the
   extension's own allocations during parse, not in a DuckDB operator that
   respects the buffer manager. **That is inferred and needs measuring** before
   anyone writes it down as fact.

## Not in scope for this entry

Fixing 116 sites is mechanical but wide, and it is a docs-wide convention
change, not a bug fix. It needs its own decision about what the canonical
example corpus is. This entry records the defect and the measurement.

## For whoever reruns a docs survey

Do not run it as it was. The harness needs, per example:

  - a hard `ulimit -v` on the CLI process, so a miss cannot reach the host;
  - a wall-clock timeout;
  - and **"this example globs an unbounded tree" treated as a FINDING**, not as
    something to survive. That is the defect, so detecting it is the point.

The first version of that survey had none of these, which is why this entry
exists.
