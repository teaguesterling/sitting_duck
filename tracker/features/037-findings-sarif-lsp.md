# Feature: Findings Interchange — canonical diagnostics + SARIF + LSP

**Priority:** P2
**Complexity:** Medium
**Status:** Proposed (nominated 2026-08-18)

## Summary

A standard findings schema and SARIF exporter (plus a thin LSP shim) so
sitting_duck analyses become consumable by CI, editors, and security tooling —
turning "a query result set" into "a scan result."

## Motivation

Every analysis today is a bespoke result set: great for exploration, opaque to
everything else. The long-term vision already wants a VS Code extension and CI
quality gates — **SARIF is the missing lingua franca that makes both real**
(GitHub code scanning, the VS Code Problems panel, and most SAST pipelines all
speak SARIF). This is the feature that lets other tools consume sitting_duck
without knowing SQL.

## Proposed API

- **Findings contract:** any query projecting `(file_path, start_line,
  start_column, end_line, end_column, rule_id, severity, message)` is a
  "findings" relation.
- `ast_to_sarif(findings)` → SARIF 2.1.0 JSON (built with DuckDB's JSON funcs).
- Optional `ast_lsp_serve()` PRAGMA — a minimal stdio LSP that runs a configured
  findings query on `didSave` and publishes diagnostics.

## Relationship to tracker

Consumes `ast_security_audit`, `ast_dead_code`, `ast_function_metrics`, and —
once duckdb#21890 lands — `ast_select_rules` rulepacks. Realizes the
long-term-vision "VS Code extension / CI gates" items via a standard interchange
rather than a bespoke integration. Pairs with the `sitting_duckling`
modularization (#87) toward a shippable scanner.
