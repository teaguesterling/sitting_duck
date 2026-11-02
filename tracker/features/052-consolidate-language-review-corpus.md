# 052 — Consolidate the language-review corpus into development docs

**Status:** open, deferred by Teague on 2026-11-01 ("another day"). Not urgent;
it is a distillation task, not a defect.

## What exists

The 28-language review pass (`042`) produced **1.1 MB across 29 files** in
`workspace/reviews/` — 25 fan-out reviews, 3 pilots, and
`00-clustered-work-list.md`. Roughly **4.4M tokens** of agent work went into
them.

**That directory is gitignored**, by the repo's own convention that intermediate
documents live in `workspace/`. So the corpus exists only on longbottom's disk.
Its *conclusions* are durable — `042` carries the clustering, the counts and the
four verified bugs; `043` carries the duckdb record — but the per-language detail
is not.

## What this task is

Distil the 29 files into development documentation that is worth keeping and
small enough to read. Not an archive of the reviews: a short set of pages a
person writing or fixing a language module would actually open.

Candidate shape, to be decided when the work is done rather than now:

- **Per-language pages already exist** at `docs/reference/languages/*.md`, and
  they are GENERATED from the `.def` files. The review findings are a different
  axis — what is wrong rather than what is declared — so they probably do not
  belong there.
- A **"known gaps by language"** page, generated from `spec/abi/module-abi.yaml`
  plus the clustered work list, would stay true as the baselines shrink. Prefer
  this over hand-written prose that will rot, which is the lesson of
  `adding-languages.md` §2.1.
- The **six symptom columns** (`signature_type`, `modifiers`, `parameters`,
  `is_exported`, `semantic_type`, `name_strategy`) are a better organising
  principle than the language list, for the same reason the fix fleet is shaped
  that way.

## Do this before the corpus is needed, not after

The four-agent fix fleet described in `042` reads from these files. If the
corpus is consolidated first, the fleet gets a smaller and better brief; if the
fleet runs first, the corpus has served its purpose and consolidation becomes
pure archaeology. Either order works — but deleting `workspace/reviews/` before
one of them happens throws the detail away.

## Not in scope

Committing the raw corpus. 1.1 MB of agent output in the repository would be
the wrong artifact, and `workspace/` is gitignored deliberately.
