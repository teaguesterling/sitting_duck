# tsx — built-in language module for sitting_duck: TypeScript with JSX.
#
# The parser is tree-sitter-typescript's own tsx grammar. Like every other
# language, its sources are read from PARSER_SOURCE_DIR (generated_parsers/ by
# default), so a build needs no grammar submodule.
#
# tsx's parser.c is LANGUAGE_VERSION 15, the same as every other tree-sitter
# language here except graphql (14) and yaml (13). It was ABI 14 for one day:
# when tsx landed, neither dialect of this grammar could be regenerated at all,
# so the file was upstream's own. tracker/bugs/063 has the cause and the fix.

sitting_duck_language(tsx TSXAdapter TS
    ADAPTER tsx_adapter.cpp
    PARSERS tree-sitter-typescript/tsx/src/parser.c
            tree-sitter-typescript/tsx/src/scanner.c)
