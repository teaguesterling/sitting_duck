# Built-in language selection for sitting_duck.
#
# Every built-in language is declared once, below, with everything the build
# needs to include or exclude it as a unit: its adapter translation unit, its
# vendored tree-sitter parser sources, and the C++ adapter class name used by
# the generated registration table.
#
# Selection is controlled by two cache variables:
#
#   -DSITTING_DUCK_LANGUAGES="python;cpp"     build only these languages
#                                             (default "all"; commas also
#                                             accepted: "python,cpp")
#   -DSITTING_DUCK_EXCLUDE_LANGUAGES="dart"   subtract languages from the set
#
# Unknown names fail the configure step. The enabled set is emitted as an
# X-macro table (sitting_duck_builtin_languages.def) consumed by
# src/language_adapter_registry_init.cpp, so registration and parse dispatch
# compile only the enabled adapters. Runtime extension/alias maps are filtered
# through the adapter registry (see src/ast_file_utils.cpp), so an excluded
# language behaves exactly like an unknown one.
#
# This is groundwork for grammar packs / the slim "sitting_duckling" build:
# https://github.com/teaguesterling/sitting_duck/issues/87

# sitting_duck_language(<name> <AdapterClass> <TS|NATIVE>
#                       ADAPTER <adapter.cpp>
#                       [PARSERS <parser sources relative to PARSER_SOURCE_DIR>...])
macro(sitting_duck_language NAME CLASS KIND)
    cmake_parse_arguments(_SDL "" "ADAPTER" "PARSERS" ${ARGN})
    list(APPEND SITTING_DUCK_ALL_LANGUAGES ${NAME})
    set(SD_LANG_${NAME}_CLASS ${CLASS})
    set(SD_LANG_${NAME}_KIND ${KIND})
    set(SD_LANG_${NAME}_ADAPTER ${_SDL_ADAPTER})
    set(SD_LANG_${NAME}_PARSERS ${_SDL_PARSERS})
endmacro()

# Declaration order is registration order (must match the historical order of
# LanguageAdapterRegistry::InitializeDefaultAdapters); user-supplied language
# lists are re-sorted into this order so a subset build registers languages in
# the same relative order as the full build.
set(SITTING_DUCK_ALL_LANGUAGES "")

sitting_duck_language(python PythonAdapter TS
    ADAPTER src/language_adapters/python_adapter.cpp
    PARSERS tree-sitter-python/src/parser.c tree-sitter-python/src/scanner.c)
sitting_duck_language(javascript JavaScriptAdapter TS
    ADAPTER src/language_adapters/javascript_adapter.cpp
    PARSERS tree-sitter-javascript/src/parser.c tree-sitter-javascript/src/scanner.c)
sitting_duck_language(cpp CPPAdapter TS
    ADAPTER src/language_adapters/cpp_adapter.cpp
    PARSERS tree-sitter-cpp/src/parser.c tree-sitter-cpp/src/scanner.c)
sitting_duck_language(typescript TypeScriptAdapter TS
    ADAPTER src/language_adapters/typescript_adapter.cpp
    PARSERS tree-sitter-typescript/typescript/src/parser.c tree-sitter-typescript/typescript/src/scanner.c)
sitting_duck_language(sql SQLAdapter TS
    ADAPTER src/language_adapters/sql_adapter.cpp
    PARSERS tree-sitter-sql/src/parser.c tree-sitter-sql/src/scanner.c)
# DuckDB native parser - SQL parsing with database-native accuracy (no
# tree-sitter grammar; the adapter wraps DuckDB's own parser).
sitting_duck_language(duckdb DuckDBAdapter NATIVE
    ADAPTER src/language_adapters/duckdb_adapter.cpp)
sitting_duck_language(go GoAdapter TS
    ADAPTER src/language_adapters/go_adapter.cpp
    PARSERS tree-sitter-go/src/parser.c)
sitting_duck_language(ruby RubyAdapter TS
    ADAPTER src/language_adapters/ruby_adapter.cpp
    PARSERS tree-sitter-ruby/src/parser.c tree-sitter-ruby/src/scanner.c)
sitting_duck_language(markdown MarkdownAdapter TS
    ADAPTER src/language_adapters/markdown_adapter.cpp
    PARSERS tree-sitter-markdown/tree-sitter-markdown/src/parser.c
            tree-sitter-markdown/tree-sitter-markdown/src/scanner.c)
sitting_duck_language(java JavaAdapter TS
    ADAPTER src/language_adapters/java_adapter.cpp
    PARSERS tree-sitter-java/src/parser.c)
sitting_duck_language(php PHPAdapter TS
    ADAPTER src/language_adapters/php_adapter.cpp
    PARSERS tree-sitter-php/php/src/parser.c tree-sitter-php/php/src/scanner.c)
sitting_duck_language(html HTMLAdapter TS
    ADAPTER src/language_adapters/html_adapter.cpp
    PARSERS tree-sitter-html/src/parser.c tree-sitter-html/src/scanner.c)
sitting_duck_language(css CSSAdapter TS
    ADAPTER src/language_adapters/css_adapter.cpp
    PARSERS tree-sitter-css/src/parser.c tree-sitter-css/src/scanner.c)
sitting_duck_language(c CAdapter TS
    ADAPTER src/language_adapters/c_adapter.cpp
    PARSERS tree-sitter-c/src/parser.c)
sitting_duck_language(rust RustAdapter TS
    ADAPTER src/language_adapters/rust_adapter.cpp
    PARSERS tree-sitter-rust/src/parser.c tree-sitter-rust/src/scanner.c)
sitting_duck_language(json JSONAdapter TS
    ADAPTER src/language_adapters/json_adapter.cpp
    PARSERS tree-sitter-json/src/parser.c)
sitting_duck_language(bash BashAdapter TS
    ADAPTER src/language_adapters/bash_adapter.cpp
    PARSERS tree-sitter-bash/src/parser.c tree-sitter-bash/src/scanner.c)
sitting_duck_language(swift SwiftAdapter TS
    ADAPTER src/language_adapters/swift_adapter.cpp
    PARSERS tree-sitter-swift/src/parser.c tree-sitter-swift/src/scanner.c)
sitting_duck_language(r RAdapter TS
    ADAPTER src/language_adapters/r_adapter.cpp
    PARSERS tree-sitter-r/src/parser.c tree-sitter-r/src/scanner.c)
sitting_duck_language(kotlin KotlinAdapter TS
    ADAPTER src/language_adapters/kotlin_adapter.cpp
    PARSERS tree-sitter-kotlin/src/parser.c tree-sitter-kotlin/src/scanner.c)
sitting_duck_language(csharp CSharpAdapter TS
    ADAPTER src/language_adapters/csharp_adapter.cpp
    PARSERS tree-sitter-c-sharp/src/parser.c tree-sitter-c-sharp/src/scanner.c)
sitting_duck_language(lua LuaAdapter TS
    ADAPTER src/language_adapters/lua_adapter.cpp
    PARSERS tree-sitter-lua/src/parser.c tree-sitter-lua/src/scanner.c)
sitting_duck_language(hcl HCLAdapter TS
    ADAPTER src/language_adapters/hcl_adapter.cpp
    PARSERS tree-sitter-hcl/src/parser.c tree-sitter-hcl/src/scanner.c)
sitting_duck_language(graphql GraphQLAdapter TS
    ADAPTER src/language_adapters/graphql_adapter.cpp
    PARSERS tree-sitter-graphql/src/parser.c)
sitting_duck_language(toml TOMLAdapter TS
    ADAPTER src/language_adapters/toml_adapter.cpp
    PARSERS tree-sitter-toml/src/parser.c tree-sitter-toml/src/scanner.c)
sitting_duck_language(zig ZigAdapter TS
    ADAPTER src/language_adapters/zig_adapter.cpp
    PARSERS tree-sitter-zig/src/parser.c)
sitting_duck_language(dart DartAdapter TS
    ADAPTER src/language_adapters/dart_adapter.cpp
    PARSERS tree-sitter-dart/src/parser.c tree-sitter-dart/src/scanner.c)

#############################
# Selection & validation
#############################

set(SITTING_DUCK_LANGUAGES "all" CACHE STRING
    "Built-in languages to compile in ('all', or a ;- or ,-separated subset, e.g. python,cpp)")
set(SITTING_DUCK_EXCLUDE_LANGUAGES "" CACHE STRING
    "Built-in languages to compile out (;- or ,-separated; applied after SITTING_DUCK_LANGUAGES)")

# sitting_duck_select_languages()
#
# Computes SD_ENABLED_LANGUAGES (canonical order) from the cache variables
# above, failing the configure on unknown names or an empty result.
function(sitting_duck_select_languages)
    string(REPLACE "," ";" _requested "${SITTING_DUCK_LANGUAGES}")
    string(REPLACE "," ";" _excluded "${SITTING_DUCK_EXCLUDE_LANGUAGES}")

    if(_requested STREQUAL "all" OR _requested STREQUAL "")
        set(_requested ${SITTING_DUCK_ALL_LANGUAGES})
    endif()

    set(_unknown "")
    foreach(_lang IN LISTS _requested _excluded)
        if(NOT _lang IN_LIST SITTING_DUCK_ALL_LANGUAGES)
            list(APPEND _unknown ${_lang})
        endif()
    endforeach()
    if(_unknown)
        string(REPLACE ";" ", " _valid "${SITTING_DUCK_ALL_LANGUAGES}")
        message(FATAL_ERROR
            "sitting_duck: unknown language name(s) in SITTING_DUCK_LANGUAGES/"
            "SITTING_DUCK_EXCLUDE_LANGUAGES: ${_unknown}\n"
            "Valid names: all, ${_valid}")
    endif()

    # Intersect in canonical order so registration order is invariant.
    set(_enabled "")
    foreach(_lang IN LISTS SITTING_DUCK_ALL_LANGUAGES)
        if(_lang IN_LIST _requested AND NOT _lang IN_LIST _excluded)
            list(APPEND _enabled ${_lang})
        endif()
    endforeach()

    if(NOT _enabled)
        message(FATAL_ERROR
            "sitting_duck: language selection is empty — SITTING_DUCK_LANGUAGES="
            "'${SITTING_DUCK_LANGUAGES}' minus SITTING_DUCK_EXCLUDE_LANGUAGES="
            "'${SITTING_DUCK_EXCLUDE_LANGUAGES}' leaves no languages to build")
    endif()

    set(SD_ENABLED_LANGUAGES ${_enabled} PARENT_SCOPE)
endfunction()

# sitting_duck_configure_languages(<parser_source_dir>)
#
# Runs selection, generates the registration X-macro table into the build tree,
# and exposes:
#   SD_ENABLED_LANGUAGES        enabled language names, canonical order
#   SD_LANGUAGE_ADAPTER_SOURCES adapter .cpp files for enabled languages
#   SD_LANGUAGE_PARSER_SOURCES  vendored parser .c files for enabled languages
#   SD_LANGUAGE_DEF_INCLUDE_DIR include dir holding sitting_duck_builtin_languages.def
function(sitting_duck_configure_languages PARSER_SOURCE_DIR)
    sitting_duck_select_languages()

    set(_adapters "")
    set(_parsers "")
    set(_def "// Generated by cmake/BuiltinLanguages.cmake — do not edit.\n")
    string(APPEND _def "// Built-in languages enabled for this build (canonical registration order).\n")
    string(APPEND _def "// Consumers define SD_BUILTIN_LANGUAGE_TS(name, AdapterClass) and\n")
    string(APPEND _def "// SD_BUILTIN_LANGUAGE_NATIVE(name, AdapterClass) before including this file.\n")
    foreach(_lang IN LISTS SD_ENABLED_LANGUAGES)
        list(APPEND _adapters ${SD_LANG_${_lang}_ADAPTER})
        foreach(_parser IN LISTS SD_LANG_${_lang}_PARSERS)
            list(APPEND _parsers ${PARSER_SOURCE_DIR}/${_parser})
        endforeach()
        string(APPEND _def "SD_BUILTIN_LANGUAGE_${SD_LANG_${_lang}_KIND}(${_lang}, ${SD_LANG_${_lang}_CLASS})\n")
    endforeach()

    set(_def_dir "${CMAKE_CURRENT_BINARY_DIR}/sitting_duck_generated")
    file(MAKE_DIRECTORY "${_def_dir}")
    file(WRITE "${_def_dir}/sitting_duck_builtin_languages.def.tmp" "${_def}")
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${_def_dir}/sitting_duck_builtin_languages.def.tmp"
        "${_def_dir}/sitting_duck_builtin_languages.def")
    file(REMOVE "${_def_dir}/sitting_duck_builtin_languages.def.tmp")

    list(LENGTH SD_ENABLED_LANGUAGES _enabled_count)
    list(LENGTH SITTING_DUCK_ALL_LANGUAGES _total_count)
    string(REPLACE ";" ", " _enabled_pretty "${SD_ENABLED_LANGUAGES}")
    message(STATUS "sitting_duck: building ${_enabled_count}/${_total_count} built-in languages: ${_enabled_pretty}")

    set(SD_ENABLED_LANGUAGES ${SD_ENABLED_LANGUAGES} PARENT_SCOPE)
    set(SD_LANGUAGE_ADAPTER_SOURCES ${_adapters} PARENT_SCOPE)
    set(SD_LANGUAGE_PARSER_SOURCES ${_parsers} PARENT_SCOPE)
    set(SD_LANGUAGE_DEF_INCLUDE_DIR "${_def_dir}" PARENT_SCOPE)
endfunction()
