# Built-in language selection for sitting_duck.
#
# Every built-in language is declared once, in its own
# `languages/<name>/module.cmake`, with everything the build needs to include or
# exclude it as a unit: its adapter translation unit, its vendored tree-sitter
# parser sources, and the C++ adapter class name used by the generated
# registration table. This file includes those modules and owns the ORDER.
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
#                       ADAPTER <adapter.cpp, relative to the calling module>
#                       [PARSERS <parser sources relative to PARSER_SOURCE_DIR>...])
#
# ADAPTER is resolved against the directory of the file that CALLS this macro,
# not against the project root. Measured: inside a macro, CMAKE_CURRENT_LIST_DIR
# is the included module's own directory. That is what lets
# languages/<name>/module.cmake say `ADAPTER <name>_adapter.cpp` and stay
# correct when M3 extracts languages/ into its own repository.
macro(sitting_duck_language NAME CLASS KIND)
    cmake_parse_arguments(_SDL "" "ADAPTER" "PARSERS" ${ARGN})
    list(APPEND SITTING_DUCK_ALL_LANGUAGES ${NAME})
    set(SD_LANG_${NAME}_CLASS ${CLASS})
    set(SD_LANG_${NAME}_KIND ${KIND})
    set(SD_LANG_${NAME}_ADAPTER "${CMAKE_CURRENT_LIST_DIR}/${_SDL_ADAPTER}")
    set(SD_LANG_${NAME}_PARSERS ${_SDL_PARSERS})
endmacro()

#############################
# Language modules, in registration order
#############################
#
# THIS LIST IS REGISTRATION ORDER. It must match the historical order of
# LanguageAdapterRegistry::InitializeDefaultAdapters; user-supplied language
# lists are re-sorted into this order so a subset build registers languages in
# the same relative order as a full build.
#
# It is written out rather than globbed on purpose. `file(GLOB)` returns the
# directories sorted alphabetically, which is NOT registration order, and the
# reorder would be invisible: every language would still be present, so every
# count and every set comparison would still pass. `ast_supported_languages()`
# cannot catch it either -- it returns hash-map order, not registration order
# (measured; tracker/features/054-v2-m2-in-tree-layout.md §2a). The generated
# sitting_duck_builtin_languages.def is the only observable record of this
# order, and scripts/verify_layout_invariance.py checks both against
# spec/layout/layout.yaml.
set(SITTING_DUCK_LANGUAGE_MODULES
    python javascript cpp typescript sql duckdb go ruby markdown java php html
    css c rust json bash swift r kotlin csharp lua hcl graphql toml zig dart
    yaml)

set(SITTING_DUCK_LANGUAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/languages"
    CACHE PATH "Directory holding the per-language module.cmake files")

set(SITTING_DUCK_ALL_LANGUAGES "")
foreach(_sdl_name IN LISTS SITTING_DUCK_LANGUAGE_MODULES)
    set(_sdl_module "${SITTING_DUCK_LANGUAGES_DIR}/${_sdl_name}/module.cmake")
    if(NOT EXISTS "${_sdl_module}")
        message(FATAL_ERROR
            "sitting_duck: ${_sdl_name} is listed in SITTING_DUCK_LANGUAGE_MODULES "
            "but ${_sdl_module} does not exist")
    endif()

    # A module that declares nothing is the failure this check exists for: the
    # include would succeed, the language would vanish from every count, and a
    # subset build of the survivors would look entirely healthy.
    list(LENGTH SITTING_DUCK_ALL_LANGUAGES _sdl_before)
    include("${_sdl_module}")
    list(LENGTH SITTING_DUCK_ALL_LANGUAGES _sdl_after)

    math(EXPR _sdl_added "${_sdl_after} - ${_sdl_before}")
    if(NOT _sdl_added EQUAL 1)
        message(FATAL_ERROR
            "sitting_duck: ${_sdl_module} declared ${_sdl_added} languages; "
            "each module.cmake must call sitting_duck_language() exactly once")
    endif()
    list(GET SITTING_DUCK_ALL_LANGUAGES ${_sdl_before} _sdl_declared)
    if(NOT _sdl_declared STREQUAL _sdl_name)
        message(FATAL_ERROR
            "sitting_duck: ${_sdl_module} declared language `${_sdl_declared}`, "
            "but its directory names it `${_sdl_name}`")
    endif()
    if(NOT EXISTS "${SD_LANG_${_sdl_name}_ADAPTER}")
        message(FATAL_ERROR
            "sitting_duck: ${_sdl_module} names adapter "
            "${SD_LANG_${_sdl_name}_ADAPTER}, which does not exist")
    endif()
endforeach()

list(LENGTH SITTING_DUCK_ALL_LANGUAGES _sdl_total)
list(LENGTH SITTING_DUCK_LANGUAGE_MODULES _sdl_expected)
if(NOT _sdl_total EQUAL _sdl_expected)
    message(FATAL_ERROR
        "sitting_duck: ${_sdl_expected} modules listed but ${_sdl_total} "
        "languages declared")
endif()
unset(_sdl_name)
unset(_sdl_module)
unset(_sdl_before)
unset(_sdl_after)
unset(_sdl_added)
unset(_sdl_declared)
unset(_sdl_total)
unset(_sdl_expected)

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
