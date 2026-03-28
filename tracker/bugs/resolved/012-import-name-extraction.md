# Import Name Extraction Across Languages

**Status:** Resolved
**Priority:** Medium
**GitHub Issues:** #23 (closed), #24 (closed)
**Commits:** `7d2e488`, `0ed5258`

## Problem

Import/include/use statement nodes returned empty or incorrect `name` fields across multiple languages. Root cause: the `DEF_TYPE` entries used `NONE` (returning empty) or `NODE_TEXT` (returning full statement text including keywords) because the generic extraction strategies couldn't reach language-specific child node types that hold the actual module name.

## Languages Fixed

| Language | Node Types | Before | After |
|----------|-----------|--------|-------|
| Python | import_statement, import_from_statement | empty | `os`, `pathlib` |
| JavaScript | import_statement | empty | `"react"` |
| TypeScript | import_statement, import_require_clause | empty | `"react"` |
| C | preproc_include | `#include <stdio.h>` | `<stdio.h>` |
| C++ | preproc_include, using_declaration | empty | `<iostream>`, `std::vector` |
| Dart | import_specification, part_directive, part_of_directive, library_import, import_or_export | empty | `'dart:core'` |
| Rust | use_declaration | `use std::collections::HashMap;` | `std::collections::HashMap` |
| PHP | require_expression, include_expression, namespace_use_declaration | `require 'file.php'` | `'file.php'`, `App\Core\BaseClass` |

## Known Remaining Issue

**Ruby** (`require`, `require_relative`, `load`): These are parsed as regular `call` nodes by tree-sitter-ruby, not special import nodes. The `DEF_TYPE` entries for these keywords are dead code. Fixing this requires a fundamentally different approach — intercepting `call` nodes by checking method name text, plus semantic type override from `COMPUTATION_CALL` to `EXTERNAL_IMPORT`. Documented in #24.

## Fix Pattern

Added custom handling in each adapter's `ExtractNodeName()` method, using `FindChildByType()` to locate the language-specific child node containing the module name. See `memory/import-extraction-patterns.md` for the full reference.

## Test Coverage

- 57 new assertions in `test/sql/languages/import_name_extraction.test`
- 7 test data files: `test/data/{c,cpp,dart,javascript,php,python,rust}/imports.*`
- Updated existing tests in `rust_language_support.test`, `php_language_support.test`, `glob_array_support.test`, `multi_file_edge_cases.test`
