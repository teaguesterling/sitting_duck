# tsx — built-in language module for sitting_duck: TypeScript with JSX.
#
# The parser is tree-sitter-typescript's own tsx grammar. Like every other
# language, its sources are read from PARSER_SOURCE_DIR (generated_parsers/ by
# default), so a build needs no grammar submodule.
#
# tsx's parser.c is LANGUAGE_VERSION 14 -- upstream tree-sitter-typescript's own
# generated file, byte-identical to the submodule's -- while the other
# tree-sitter languages here are 15. Both load: the runtime accepts a range.
# It is 14 because `tree-sitter generate` fails on BOTH dialects of this grammar
# at the pinned CLI, so the 0.26.0 regeneration that produced the rest cannot be
# produced for tsx. See tracker/bugs/063 and scripts/generate_all_parsers.sh.

sitting_duck_language(tsx TSXAdapter TS
    ADAPTER tsx_adapter.cpp
    PARSERS tree-sitter-typescript/tsx/src/parser.c
            tree-sitter-typescript/tsx/src/scanner.c)
