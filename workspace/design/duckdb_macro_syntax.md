# DuckDB SQL Macro Syntax Reference

## Parameter Default Values

DuckDB SQL macros use `:=` for parameter defaults, NOT `DEFAULT`:

### ✅ Correct Syntax
```sql
CREATE OR REPLACE MACRO my_macro(
    param1,                    -- Required parameter
    param2 := 'default',       -- Optional with default value
    param3 := NULL,            -- Optional, defaults to NULL
    param4 := 42               -- Optional with numeric default
) AS (...);
```

### ❌ Incorrect Syntax
```sql
-- This will NOT work:
CREATE OR REPLACE MACRO my_macro(
    param1,
    param2 DEFAULT 'default',  -- WRONG: DEFAULT not supported
    param3 = NULL              -- WRONG: = not supported for defaults
) AS (...);
```

## Examples from Our Implementation

### Simple Defaults
```sql
CREATE OR REPLACE MACRO read_file_lines(
    file_path, 
    start_line := 1, 
    end_line := 2147483647
) AS (...);
```

### Boolean Defaults
```sql
CREATE OR REPLACE MACRO ast_extract_source(
    ast_obj,
    node_ids := NULL,
    pad_lines := 0,
    include_line_numbers := true,
    highlight_range := false
) AS (...);
```

### String Defaults
```sql
CREATE OR REPLACE MACRO get_source_text(
    file_path, 
    start_line, 
    end_line, 
    include_line_numbers := true,
    line_format := '{:4d}: {}'
) AS (...);
```

### List Defaults
```sql
CREATE OR REPLACE MACRO ast_test_functions(
    ast_obj, 
    patterns := ['test_*', '*_test', 'Test*']
) AS (...);
```

## Calling Macros with Named Parameters

When calling macros, you can use named parameters with `:=`:

```sql
-- Positional parameters
SELECT ast_extract_source(ast_obj, NULL, 5, 0, true, false);

-- Named parameters (clearer)
SELECT ast_extract_source(
    ast_obj,
    pad_lines := 5,
    include_line_numbers := true
);

-- Mix positional and named
SELECT ast_extract_source(
    ast_obj,
    pad_lines := 5
);
```

## Key Points

1. **Always use `:=`** for parameter defaults in macro definitions
2. **Optional parameters** must come after required parameters
3. **NULL is a valid default** and commonly used
4. **Lists/arrays** can be default values
5. **Named parameters** make code more readable and maintainable

This syntax is consistent across all DuckDB SQL macros and is different from standard SQL function syntax.