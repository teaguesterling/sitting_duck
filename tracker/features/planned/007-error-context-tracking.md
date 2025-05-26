# Feature: Enhanced Error Messages with Context

**Status:** Planned
**Priority:** High
**Target:** v0.3.0

## Description
Add context tracking to error messages, especially for chained operations, to improve debugging experience.

## Current Problem
```
ERROR: invalid node type 'functions'
```

## Proposed Solution
```
ERROR: in ast_find_type(): invalid node type 'functions'
  Did you mean 'function_definition'?
  Called from: ast(nodes).find_type('functions').count_elements()
```

## Implementation Ideas
1. Add optional _context parameter to track call chain
2. Include suggestions for common typos
3. Show full chain path on errors
4. Maintain typo->correct mapping:
   - functions -> function_definition
   - class -> class_definition
   - func -> function_definition
   - string -> string_content

## Benefits
- Much easier debugging
- Better user experience
- Helps AI agents self-correct