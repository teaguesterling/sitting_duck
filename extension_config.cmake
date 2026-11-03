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

# v2.0 stopped linking an extension into the duckdb targets just because it is
# loaded here. extension/extension_build_tools.cmake:784 states it outright:
# "DONT_LINK was removed, extensions are only linked when listed with
# duckdb_extension_statically_link() or STATICALLY_LINK_EXTENSIONS". The four
# core extensions still get linked, via LINK_CORE_EXTENSIONS calling this same
# function (extension_build_tools.cmake:947).
#
# Without this line the from-source duckdb CLI contains no sitting_duck at all:
# 0 symbols, and no row in duckdb_extensions(). On v1.5.6 the CLI had it
# implicitly, which is why two CI gates that never called LOAD passed there and
# failed on the v2.0.0 pin.
duckdb_extension_statically_link(sitting_duck)
