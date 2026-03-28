# Phase 2 Roadmap: Performance and UX Optimization

**Timeline**: After Phase 1 (Language Support) completion
**Duration**: 6-8 weeks
**Status**: Partially addressed via SQL macros; C++ hot-path work not yet started

## Overview

Based on peer review feedback, Phase 2 should focus on performance optimization and AI agent UX improvements.

## Week 1-2: Performance Critical Path

### C++ Hot Path Implementation
- [ ] Implement C++ versions of core macros:
  - `ast_find_type` — *SQL version exists in semantic_predicates.sql*
  - `ast_filter` — *SQL version exists*
  - `ast_extract_names` — *SQL version exists*
- [ ] Add filter pushdown to `read_ast()` — tracked as feature #014
- [ ] Benchmark and validate improvements

### Expected Outcomes
- 10-100x performance improvement on large files
- Sub-second queries on 10K+ node files

## Week 3-4: AI Agent Experience

### Semantic Functions — Partially Done via SQL Macros
- [x] `ast_definitions()` — list all functions/classes/variables
- [x] `ast_function_metrics()` — complexity metrics
- [x] `ast_source_of()` — find and extract function source
- [ ] `code_find_callers()` - Find function callers — tracked as #017
- [ ] `code_find_implementations()` - Find interface implementations
- [x] `ast_nesting_analysis()` — nesting complexity

### Error Context
- [ ] Propagate file path and line numbers in errors
- [ ] Improve error messages

## Week 5-6: Scalability Architecture

### Caching Strategy
- [ ] Design AST caching layer
- [ ] Implement cache invalidation
- [ ] Add cache statistics

### Multi-file Support — DONE
- [x] Glob patterns in `read_ast('src/**/*.py')`
- [x] Parallel file processing
- [x] `ignore_errors` parameter for resilient batch processing

## Week 7-8: Testing and Documentation

### Performance Testing
- [ ] Create performance benchmark suite — exists but slow (#006)
- [ ] Add regression tests
- [ ] Document performance characteristics

### AI Agent Documentation — DONE
- [x] AI_AGENT_GUIDE.md — comprehensive task-oriented guide
- [x] Common patterns documented
- [x] 87 test files as usage examples

## Success Criteria

1. **Performance**: 10K+ node files query in <1 second
2. **Usability**: SQL macros cover most common analysis tasks ✓
3. **Reliability**: No performance regressions
4. **Documentation**: Complete guide for AI agent integration ✓

## Dependencies

- Phase 1 completion (stable API) ✓
- C++ development environment setup ✓
- Performance testing infrastructure — partially done
