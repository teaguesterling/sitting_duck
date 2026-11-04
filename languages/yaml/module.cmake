# yaml — built-in language module for sitting_duck.
#
# Declared here so that the language is one unit: its adapter translation
# unit, its tree-sitter parser sources and its adapter class name. The
# ADAPTER path is relative to THIS file, so the directory stays
# self-describing when M3 extracts languages/ into its own repository.
#
# cmake/BuiltinLanguages.cmake includes this file and owns registration
# order; this file must declare exactly one language, named `yaml`.

# yaml's parser is ABI 13 (the runtime's TREE_SITTER_MIN_COMPATIBLE_LANGUAGE_VERSION),
# not 15 like the rest, and its external scanner is C++ (scanner.cc, which
# #includes schema.generated.cc -- tree-sitter allows only one scanner file).
# Both are fine to compile and load; see scripts/generate_all_parsers.sh for why
# this grammar cannot currently be regenerated (#233).

sitting_duck_language(yaml YAMLAdapter TS
    ADAPTER yaml_adapter.cpp
    PARSERS tree-sitter-yaml/src/parser.c
            tree-sitter-yaml/src/scanner.cc)
