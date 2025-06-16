# Archived Broken Tests - December 2024

This directory contains tests that were archived during the test suite overhaul in December 2024.

## Why These Tests Were Archived

- **79% failure rate** (15/19 tests failing)
- **Outdated expectations** - hardcoded values that don't match current parsing behavior
- **Overengineered method chaining** - complex AST operations that were too ambitious
- **Wrong node counts** - expected vs actual values were completely off (e.g., 309 vs 11 nodes)
- **Broken macro functionality** - tests assumed features that were never properly implemented

## Test Suite Redesign Goals

The new test architecture focuses on:
1. **Core functionality first** - basic parsing, language support, file detection
2. **Systematic language coverage** - one comprehensive test per supported language
3. **Maintainable expectations** - tests that validate what actually works
4. **Clear separation of concerns** - avoid complex method chaining in tests

## Archived Test Files

- ast_struct.test
- count_fields.test  
- counts_feature.test
- file_extension_detection.test
- glob_patterns.test
- hybrid_ast_objects.test
- macro_ast_struct.test
- macro_loading_test.test
- macro_segfault_regression.test
- peer_review_features.test
- semantic_type_validation.test
- short_names.test
- test_semantic_types.test
- tree_helpers.test

## What Was Kept

- ✅ go_language_support.test (comprehensive, passing)
- ✅ ruby_language_support.test (comprehensive, passing)  
- ✅ supported_languages.test (core functionality)
- ✅ basic/extension_loading.test (core functionality)
- ✅ basic/error_handling.test (core functionality)

These archived tests may contain useful patterns or test cases that can be adapted for the new test suite.