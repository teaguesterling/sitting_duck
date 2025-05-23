cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME duckdb_ast)

# Tree-sitter is our main dependency for AST parsing
find_package(unofficial-tree-sitter CONFIG REQUIRED)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)

set(EXTENSION_SOURCES 
    src/duckdb_ast_extension.cpp
    src/ast_parser.cpp
    src/read_ast_function.cpp
    src/grammars.cpp
    grammars/tree-sitter-python/src/parser.c
    grammars/tree-sitter-python/src/scanner.c)
    # Temporarily disabled while fixing issues:
    # src/ast_type.cpp
    # src/read_ast_objects_function.cpp
    # src/ast_helper_functions.cpp

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Link tree-sitter library
target_link_libraries(${EXTENSION_NAME} unofficial::tree-sitter::tree-sitter)
target_link_libraries(${LOADABLE_EXTENSION_NAME} unofficial::tree-sitter::tree-sitter)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
