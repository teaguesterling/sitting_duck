# This file is included by DuckDB's build system. It specifies which extension to load

# core_functions must load before sitting_duck: the generated static loader
# (LoadAllExtensions) loads extensions in the order listed here, and
# sitting_duck's embedded SQL macros use core_functions operators (e.g. "&").
# With sitting_duck listed first, the built duckdb shell failed at startup with
# 'Scalar Function with name "&" is not in the catalog'.
duckdb_extension_load(core_functions)

# Extension from this repo
duckdb_extension_load(sitting_duck
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}
    LOAD_TESTS
)
