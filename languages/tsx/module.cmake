# tsx — built-in language module for sitting_duck: TypeScript with JSX.
#
# The parser is tree-sitter-typescript's own tsx grammar, compiled from the
# grammars/ submodule rather than a regenerated copy in generated_parsers/.

sitting_duck_language(tsx TSXAdapter TS
    ADAPTER tsx_adapter.cpp
    PARSERS ../grammars/tree-sitter-typescript/tsx/src/parser.c
            ../grammars/tree-sitter-typescript/tsx/src/scanner.c)
