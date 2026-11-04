# hcl — built-in language module for sitting_duck.
#
# Declared here so that the language is one unit: its adapter translation
# unit, its tree-sitter parser sources and its adapter class name. The
# ADAPTER path is relative to THIS file, so the directory stays
# self-describing when M3 extracts languages/ into its own repository.
#
# cmake/BuiltinLanguages.cmake includes this file and owns registration
# order; this file must declare exactly one language, named `hcl`.

sitting_duck_language(hcl HCLAdapter TS
    ADAPTER hcl_adapter.cpp
    PARSERS tree-sitter-hcl/src/parser.c
            tree-sitter-hcl/src/scanner.c)
