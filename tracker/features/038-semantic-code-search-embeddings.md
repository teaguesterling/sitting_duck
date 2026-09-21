# Feature: Semantic Code Search via Local Embeddings

**Priority:** P3 (exploratory)
**Complexity:** High (external dependencies)
**Status:** Proposed / research (nominated 2026-08-18)

## Summary

Embed extracted functions with a local code model and index them for vector
similarity search — enabling "find functions like this one" and natural-language
code search: the *semantic* similarity that structural fingerprinting (036)
can't capture.

## Motivation

Structural clone detection finds code shaped the same way; embeddings find code
that *means* the same thing — different implementations of one intent, or "where
do we validate JWTs?" It reaches the semantic tier from a pragmatic,
model-driven angle rather than via dataflow.

## Proposed sketch

- Extract per-function text (`signature_type` + `peek`/body) via `read_ast`.
- Embed with a local model — e.g. an Ollama code-embedding model already on the
  host (`qwen2.5-coder`-class) — via an HTTP UDF / `duckdb_mcp`, or offline batch.
- Store vectors + node identity; query with DuckDB VSS (HNSW) for k-NN.
- `ast_similar(fn, k := 10)` → nearest functions; `ast_search('natural language', k)`.

## Why exploratory

Depends on external components (an embedding model + DuckDB VSS) and raises
model/versioning reproducibility questions; belongs behind a flag as an optional
capability, not core. Highest ceiling, lowest certainty — and uniquely enabled by
a local-Ollama homelab stack, which is the point.

## Relationship to tracker

Orthogonal to the structural features; complements 034/036. Cross-extension:
leans on `duckdb_mcp` and DuckDB's vector search.
