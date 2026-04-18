# Docs Restructure Plan

**Date:** 2026-04-18
**Status:** In progress

## Problem

The docs were written incrementally by multiple authors (human + AI sessions).
Result: inconsistent voice, duplicate content, mixed tutorial/reference/guide
within single pages, no progressive disclosure path, and buried features.

## Approach: Divio documentation framework

Four document types, each with one purpose:

| Type | Purpose | Reader's question |
|---|---|---|
| **Tutorial** | Learning-oriented | "Teach me" |
| **How-to** | Task-oriented | "How do I solve X?" |
| **Reference** | Information-oriented | "What's the exact spec?" |
| **Explanation** | Understanding-oriented | "Why does it work this way?" |

## Page audit and classification

### Tutorials (new section — mostly fresh writing)

| Source | Becomes | Action |
|---|---|---|
| getting-started/installation.md | tutorials/installation.md | Shrink to essentials |
| getting-started/quickstart.md + basic-usage.md | tutorials/first-query.md | Merge into one 5-min tutorial |
| guide/selectors/tutorial.md | tutorials/css-selectors.md | Reshape, tighten |
| guide/tutorial-pattern-matching.md (767 lines!) | tutorials/pattern-matching.md | Cut to 200 lines |
| (new) | tutorials/call-graphs.md | Fresh: ast_callers/ast_callees/scope.function |
| ai-agent-guide.md (718 lines) | tutorials/ai-agents.md | Reshape, move |

### How-to guides (split from cookbook + reshape guides)

| Source | Becomes | Action |
|---|---|---|
| guide/cookbook.md (892 lines) | Split into 5-8 focused how-tos | Extract recipes |
| guide/selectors/examples.md | Merge relevant parts into how-tos | |
| guide/cross-language.md | how-to/cross-language-analysis.md | Reshape |
| guide/multi-file.md | how-to/multi-file-processing.md | Reshape |
| guide/context-extraction.md | how-to/extract-context.md | Reshape |
| guide/structural-search.md (609 lines) | how-to/structural-search.md | Cut significantly |
| (new) | how-to/find-dead-code.md | Extract from cookbook |
| (new) | how-to/security-audit.md | Extract from cookbook |
| (new) | how-to/complexity-analysis.md | Extract from cookbook |
| (new) | how-to/parse-once-query-many.md | Fresh: ast_select_from workflow |

### Reference (consolidate API pages + move selector reference)

| Source | Becomes | Action |
|---|---|---|
| api/core-functions.md | reference/functions.md | Keep, expand with ast_select |
| api/output-schema.md | reference/output-schema.md | Keep |
| api/parameters.md | reference/parameters.md | Keep |
| api/semantic-types.md | reference/semantic-types.md | Keep (delete guide/ duplicate) |
| api/structural-analysis.md | reference/analysis-macros.md | Keep |
| api/utility-functions.md | reference/utility-functions.md | Keep |
| guide/selectors/index.md | reference/css-selectors.md | Move: it's a syntax reference |
| guide/selectors/pseudo-classes.md | reference/css-pseudo-classes.md | Move |
| guide/selectors/attributes.md | reference/css-attributes.md | Move |
| guide/selectors/kinds-types-and-classes.md | reference/semantic-aliases.md | Move |
| guide/selectors/node-types.md | reference/node-type-selectors.md | Move |
| reference/languages/*.md | reference/languages/*.md | Keep (auto-generated) |

### Explanation (understanding-oriented)

| Source | Becomes | Action |
|---|---|---|
| explanation/native-extraction.md | Keep | Already moved |
| explanation/native-extraction-semantics.md | Keep | Already moved |
| development/architecture.md | explanation/architecture.md | Move (it's conceptual, not contributing) |
| guide/semantic-types.md | DELETE (covered by reference + explanation) | |
| guide/parsing-files.md | explanation/how-parsing-works.md | Reshape as conceptual |
| (new) | explanation/scope-system.md | Fresh: scope struct, scope.function, scope.stack |
| (new) | explanation/selector-engine.md | Fresh: how CSS→SQL dispatch works |

### Development (contributing — keep as-is)

| Source | Action |
|---|---|
| development/adding-languages.md | Keep |
| development/contributing.md | Keep |
| development/updating.md | Keep |

### Languages (category overviews — keep as-is for now)

| Source | Action |
|---|---|
| languages/overview.md + 5 category pages | Keep in current location |

### Delete

| Source | Reason |
|---|---|
| ADDING_NEW_LANGUAGES.md | Duplicate of development/adding-languages.md ✅ done |
| guide/css-selectors.md | Redirect stub ✅ done |
| guide/semantic-types.md | Covered by reference/semantic-types.md |

## Nav restructure

```yaml
nav:
  - Home: index.md
  - Tutorials:
    - Installation: tutorials/installation.md
    - Your First Query: tutorials/first-query.md
    - CSS Selectors: tutorials/css-selectors.md
    - Pattern Matching: tutorials/pattern-matching.md
    - Call Graph Analysis: tutorials/call-graphs.md
    - AI Agent Integration: tutorials/ai-agents.md
  - How-to Guides:
    - Multi-File Processing: how-to/multi-file-processing.md
    - Cross-Language Analysis: how-to/cross-language-analysis.md
    - Extract Context: how-to/extract-context.md
    - Find Dead Code: how-to/find-dead-code.md
    - Security Audit: how-to/security-audit.md
    - Complexity Analysis: how-to/complexity-analysis.md
    - Parse Once, Query Many: how-to/parse-once-query-many.md
    - Structural Search: how-to/structural-search.md
  - Reference:
    - Functions: reference/functions.md
    - Output Schema: reference/output-schema.md
    - Parameters: reference/parameters.md
    - Semantic Types: reference/semantic-types.md
    - CSS Selector Syntax: reference/css-selectors.md
    - Pseudo-Classes: reference/css-pseudo-classes.md
    - Attribute Selectors: reference/css-attributes.md
    - Semantic Aliases: reference/semantic-aliases.md
    - Node Type Selectors: reference/node-type-selectors.md
    - Analysis Macros: reference/analysis-macros.md
    - Utility Functions: reference/utility-functions.md
    - Node Types by Language: reference/languages/index.md
  - Explanation:
    - Architecture: explanation/architecture.md
    - How Parsing Works: explanation/how-parsing-works.md
    - Scope System: explanation/scope-system.md
    - Semantic Type System: explanation/semantic-types.md
    - Native Extraction: explanation/native-extraction.md
    - Selector Engine: explanation/selector-engine.md
  - Languages:
    - Overview: languages/overview.md
    - Web: languages/web.md
    - Systems: languages/systems.md
    - Scripting: languages/scripting.md
    - Enterprise: languages/enterprise.md
    - Infrastructure: languages/infrastructure.md
  - Development:
    - Contributing: development/contributing.md
    - Adding Languages: development/adding-languages.md
    - Updating: development/updating.md

## Style guide

- **Voice:** direct, second-person ("you"), no preamble
- **SQL first:** show the query, then explain what it does — not the reverse
- **One concept per section:** if a section needs a sub-heading, consider splitting
- **Tutorials:** < 200 lines, copy-pasteable, "try this" prompts
- **How-tos:** problem → solution → variations, no theory
- **Reference:** tables, signatures, types, minimal prose
- **Explanation:** can be longer, diagrams welcome, link to reference for specifics

## Execution order

1. ✅ Kill duplicates and orphans
2. Write the restructure spec (this doc)
3. Create new directories and move pages
4. Rewrite mkdocs.yml nav
5. Write fresh tutorials (first-query, call-graphs, scope)
6. Split cookbook into how-tos
7. Consolidate overlapping pages
8. Final editorial pass for voice consistency
```
