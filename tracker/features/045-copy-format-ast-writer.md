# 045 — Native `COPY ... TO (FORMAT ast)` Code Generator & Writer

**Status:** Planned (Phase 6).  
**Goal:** Enable direct serialization of AST queries and tables to source code files on disk via DuckDB's native `COPY ... TO` mechanism.

---

## Overview & Motivation

Currently, `ast_unparse()`, `ast_unparse_from()`, and `ast_unparse_code()` return a relational table of `(file_path VARCHAR, source VARCHAR)` records. To save reconstructed code to disk, users must write external application code or shell scripts.

By implementing a native DuckDB `CopyFunction` for `FORMAT ast`, DuckDB can directly serialize AST query streams into reconstructed, formatted source files on disk.

---

## Example Usage

### 1. Single File Export
```sql
COPY (
    SELECT * FROM my_transformed_ast
) TO 'src/generated/models.py' (
    FORMAT ast,
    LANGUAGE 'python',
    PRESET 'black',
    INDENT 4
);
```

### 2. Multi-File Refactoring Pipeline with Automatic Partitioning
```sql
-- Read codebase, apply AST patches, and write refactored source tree
COPY (
    SELECT * FROM ast_patch(
        read_ast('src/**/*.ts'),
        '.call#deprecatedMethod',
        'replacementMethod()'
    )
) TO 'dist/refactored/' (
    FORMAT ast,
    LANGUAGE 'typescript',
    PRESET 'prettier',
    PARTITION_BY (file_path),
    OVERWRITE true
);
```

---

## Supported Options in `COPY (FORMAT ast, ...)`

| Option | Type | Default | Description |
|---|---|---|---|
| `LANGUAGE` | `VARCHAR` | Auto-detect | Explicit language grammar (e.g. `'python'`, `'cpp'`, `'go'`). Inferred from file extension if omitted. |
| `PRESET` | `VARCHAR` | `'default'` | Tool style preset (e.g. `'pep8'`, `'black'`, `'prettier'`, `'gofmt'`, `'llvm'`, `'google'`). |
| `INDENT` | `BIGINT / VARCHAR` | `4` | Indentation width (`2`, `4`) or custom string (`'\t'`). |
| `LINE_ENDINGS` | `VARCHAR` | `'LF'` | Output line endings (`'LF'` or `'CRLF'`). |
| `OVERWRITE` | `BOOLEAN` | `true` | Overwrite existing target files. |
| `STRIP_COMMENTS` | `BOOLEAN` | `false` | Exclude comment AST nodes during code generation. |

---

## Architecture & Implementation Details

1. **`CopyFunction` Registration:**
   - Register `CopyFunction` with name `"ast"` in `sitting_duck_extension.cpp`.
   - Implement `CopyFunction::copy_to_bind`, `copy_to_initialize_global`, `copy_to_sink`, `copy_to_combine`, and `copy_to_finalize`.

2. **Streaming C++ AST Sink:**
   - Consume streaming `DataChunk`s of AST nodes from the query engine.
   - Group rows by `file_path` and document order (`node_id`).
   - Run C++ `UnparseASTStream` using registered `UnparseRule` entries.
   - Stream formatted bytes directly to DuckDB's `FileSystem` / output file handles with zero intermediate table allocation.
