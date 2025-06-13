cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME duckdb_ast)

# Tree-sitter is our main dependency for AST parsing
find_package(unofficial-tree-sitter CONFIG REQUIRED)

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)

# Find Python for script execution
find_package(Python3 COMPONENTS Interpreter)

# Generate embedded SQL macros header before building
if(Python3_FOUND)
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_sql_macros.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE EMBED_SQL_RESULT
    )
    
    if(NOT EMBED_SQL_RESULT EQUAL 0)
        message(WARNING "Failed to generate embedded SQL macros header")
    endif()
else()
    message(WARNING "Python not found - using existing embedded SQL macros header")
endif()

set(EXTENSION_SOURCES 
    src/duckdb_ast_extension.cpp
    src/language_adapter.cpp
    src/read_ast_function.cpp
    src/read_ast_streaming_function.cpp
    src/read_ast_objects_hybrid.cpp
    src/ast_sql_macros.cpp
    src/short_names_function.cpp
    src/parse_ast_function.cpp
    src/unified_ast_backend.cpp
    src/ast_type.cpp
    src/semantic_types.cpp
    src/semantic_type_functions.cpp
    src/ast_file_utils.cpp
    src/ast_supported_languages_function.cpp
    grammars/tree-sitter-python/src/parser.c
    grammars/tree-sitter-python/src/scanner.c
    grammars/tree-sitter-javascript/src/parser.c
    grammars/tree-sitter-javascript/src/scanner.c
    grammars/tree-sitter-cpp/src/parser.c
    grammars/tree-sitter-cpp/src/scanner.c
    grammars/tree-sitter-typescript/typescript/src/parser.c
    grammars/tree-sitter-typescript/typescript/src/scanner.c
    grammars/tree-sitter-sql/src/parser.c
    grammars/tree-sitter-sql/src/scanner.c
    grammars/tree-sitter-go/src/parser.c
    grammars/tree-sitter-ruby/src/parser.c
    grammars/tree-sitter-ruby/src/scanner.c
    # Temporarily disabled due to ABI compatibility issues:
    # grammars/tree-sitter-rust/src/parser.c
    # grammars/tree-sitter-rust/src/scanner.c
    )
    # Removed unused files:
    # src/ast_parser.cpp (replaced by unified_ast_backend.cpp)
    # src/language_handler.cpp (replaced by language_adapter.cpp) 
    # Temporarily disabled while fixing issues:
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
