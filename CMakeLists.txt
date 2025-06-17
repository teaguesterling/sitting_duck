cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME sitting_duck)

# Tree-sitter is our main dependency for AST parsing
# Build tree-sitter from submodule (lib subdirectory contains CMakeLists.txt)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build tree-sitter as static library" FORCE)
add_subdirectory(third_party/tree-sitter/lib)

# Build tree-sitter CLI if Cargo is available
find_program(CARGO_EXECUTABLE cargo)
if(CARGO_EXECUTABLE)
    set(TREE_SITTER_CLI "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter/target/release/tree-sitter")
    add_custom_command(
        OUTPUT ${TREE_SITTER_CLI}
        COMMAND ${CARGO_EXECUTABLE} build --release --bin tree-sitter
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter"
        COMMENT "Building tree-sitter CLI with Cargo"
        VERBATIM
    )
    add_custom_target(tree-sitter-cli ALL DEPENDS ${TREE_SITTER_CLI})
    # Install the CLI binary
    install(PROGRAMS ${TREE_SITTER_CLI} DESTINATION bin)
    message(STATUS "Tree-sitter CLI will be built at: ${TREE_SITTER_CLI}")
    
    # Function to generate parser for a grammar submodule with optional dependencies and patches
    function(generate_parser GRAMMAR_DIR PARSER_PATH SCANNER_PATH)
        get_filename_component(GRAMMAR_NAME ${GRAMMAR_DIR} NAME)
        set(GRAMMAR_JS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/${GRAMMAR_DIR}/grammar.js")
        set(PARSER_C "${CMAKE_CURRENT_SOURCE_DIR}/grammars/${PARSER_PATH}")
        
        # Only generate if grammar.js exists
        if(EXISTS ${GRAMMAR_JS})
            # Create a list of output files for this grammar
            set(GENERATED_FILES ${PARSER_C})
            
            # Add scanner.c if specified
            if(NOT "${SCANNER_PATH}" STREQUAL "")
                set(SCANNER_C "${CMAKE_CURRENT_SOURCE_DIR}/grammars/${SCANNER_PATH}")
                list(APPEND GENERATED_FILES ${SCANNER_C})
            endif()
            
            # Build dependency list (CLI + grammar.js + any previously generated parsers)
            set(GENERATION_DEPS ${TREE_SITTER_CLI} ${GRAMMAR_JS})
            
            # Add dependency on C grammar for C++ (since C++ requires C)
            if("${GRAMMAR_NAME}" STREQUAL "tree-sitter-cpp")
                list(APPEND GENERATION_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-c/src/parser.c")
                list(APPEND GENERATION_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-c/grammar.js")
            endif()
            
            # Add dependency on JavaScript grammar for TypeScript
            if("${GRAMMAR_NAME}" STREQUAL "typescript")
                list(APPEND GENERATION_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-javascript/src/parser.c")
                list(APPEND GENERATION_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/grammars/tree-sitter-javascript/grammar.js")
            endif()
            
            # Apply patches before generation using dedicated script
            set(PATCH_STATUS_DIR "${CMAKE_CURRENT_BINARY_DIR}/patch_status")
            set(PATCH_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/scripts/apply_grammar_patches.sh")
            
            # Apply patches if script exists
            if(EXISTS ${PATCH_SCRIPT})
                set(PATCH_COMMANDS 
                    COMMAND ${PATCH_SCRIPT} "${GRAMMAR_NAME}" "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/patches" "${PATCH_STATUS_DIR}")
            else()
                set(PATCH_COMMANDS "")
            endif()
            
            # Generate parser with patches
            add_custom_command(
                OUTPUT ${GENERATED_FILES}
                ${PATCH_COMMANDS}
                COMMAND ${TREE_SITTER_CLI} generate
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/grammars/${GRAMMAR_DIR}"
                DEPENDS ${GENERATION_DEPS}
                COMMENT "Applying patches and generating parser for ${GRAMMAR_NAME}"
                VERBATIM
            )
            
            # Add files to global list for target dependency
            set(ALL_GENERATED_PARSERS ${ALL_GENERATED_PARSERS} ${GENERATED_FILES} PARENT_SCOPE)
            
            message(STATUS "Parser generation configured for ${GRAMMAR_NAME}")
        endif()
    endfunction()
    
    # Generate parsers for all grammar submodules (order matters for dependencies)
    # C must be generated before C++ since C++ depends on C grammar
    generate_parser("tree-sitter-c" "tree-sitter-c/src/parser.c" "")
    generate_parser("tree-sitter-cpp" "tree-sitter-cpp/src/parser.c" "tree-sitter-cpp/src/scanner.c")
    # TypeScript builds on JavaScript
    generate_parser("tree-sitter-javascript" "tree-sitter-javascript/src/parser.c" "tree-sitter-javascript/src/scanner.c")
    generate_parser("tree-sitter-typescript/typescript" "tree-sitter-typescript/typescript/src/parser.c" "tree-sitter-typescript/typescript/src/scanner.c")
    # Independent grammars
    generate_parser("tree-sitter-python" "tree-sitter-python/src/parser.c" "tree-sitter-python/src/scanner.c")
    generate_parser("tree-sitter-sql" "tree-sitter-sql/src/parser.c" "tree-sitter-sql/src/scanner.c")
    generate_parser("tree-sitter-go" "tree-sitter-go/src/parser.c" "")
    generate_parser("tree-sitter-ruby" "tree-sitter-ruby/src/parser.c" "tree-sitter-ruby/src/scanner.c")
    generate_parser("tree-sitter-markdown/tree-sitter-markdown" "tree-sitter-markdown/tree-sitter-markdown/src/parser.c" "tree-sitter-markdown/tree-sitter-markdown/src/scanner.c")
    generate_parser("tree-sitter-java" "tree-sitter-java/src/parser.c" "")
    generate_parser("tree-sitter-php/php" "tree-sitter-php/php/src/parser.c" "tree-sitter-php/php/src/scanner.c")
    generate_parser("tree-sitter-html" "tree-sitter-html/src/parser.c" "tree-sitter-html/src/scanner.c")
    generate_parser("tree-sitter-css" "tree-sitter-css/src/parser.c" "tree-sitter-css/src/scanner.c")
    generate_parser("tree-sitter-rust" "tree-sitter-rust/src/parser.c" "tree-sitter-rust/src/scanner.c")
    
    # Create a target for all generated parsers
    if(ALL_GENERATED_PARSERS)
        add_custom_target(generate-parsers DEPENDS ${ALL_GENERATED_PARSERS})
        list(LENGTH ALL_GENERATED_PARSERS PARSER_COUNT)
        message(STATUS "Generated parser target created with ${PARSER_COUNT} files")
    endif()
    
else()
    message(WARNING "Cargo not found - tree-sitter CLI will not be built and parsers will not be auto-generated")
endif()

# Add tree-sitter to the DuckDB export set
install(TARGETS tree-sitter
        EXPORT "${DUCKDB_EXPORT_SET}"
        LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
        ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)
# Include tree-sitter headers - both public API and internal headers
include_directories(third_party/tree-sitter/lib/include)
include_directories(third_party/tree-sitter/lib/src)

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

# Install tree-sitter headers to build directory for grammar scanners
set(TREE_SITTER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter/lib/src")
set(BUILD_TREE_SITTER_INCLUDE "${CMAKE_BINARY_DIR}/include")
set(BUILD_TREE_SITTER_HEADER_DIR "${BUILD_TREE_SITTER_INCLUDE}/tree_sitter")

# Create tree_sitter directory in build and copy all needed headers
file(MAKE_DIRECTORY "${BUILD_TREE_SITTER_HEADER_DIR}")

# Copy the headers that grammars commonly need
file(COPY "${TREE_SITTER_SRC_DIR}/parser.h" DESTINATION "${BUILD_TREE_SITTER_HEADER_DIR}")
file(COPY "${TREE_SITTER_SRC_DIR}/array.h" DESTINATION "${BUILD_TREE_SITTER_HEADER_DIR}")
file(COPY "${TREE_SITTER_SRC_DIR}/alloc.h" DESTINATION "${BUILD_TREE_SITTER_HEADER_DIR}")

# Add build include directory to include path
include_directories("${BUILD_TREE_SITTER_INCLUDE}")

set(EXTENSION_SOURCES 
    src/duckdb_ast_extension.cpp
    src/language_adapter.cpp
    src/language_adapter_registry_init.cpp
    src/language_adapters/python_adapter.cpp
    src/language_adapters/javascript_adapter.cpp
    src/language_adapters/cpp_adapter.cpp
    src/language_adapters/typescript_adapter.cpp
    src/language_adapters/sql_adapter.cpp
    src/language_adapters/go_adapter.cpp
    src/language_adapters/ruby_adapter.cpp
    src/language_adapters/markdown_adapter.cpp
    src/language_adapters/java_adapter.cpp
    src/language_adapters/html_adapter.cpp
    src/language_adapters/css_adapter.cpp
    src/language_adapters/c_adapter.cpp
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
    grammars/tree-sitter-markdown/tree-sitter-markdown/src/parser.c
    grammars/tree-sitter-markdown/tree-sitter-markdown/src/scanner.c
    grammars/tree-sitter-java/src/parser.c
    grammars/tree-sitter-php/php/src/parser.c
    grammars/tree-sitter-php/php/src/scanner.c
    grammars/tree-sitter-html/src/parser.c
    grammars/tree-sitter-html/src/scanner.c
    grammars/tree-sitter-css/src/parser.c
    grammars/tree-sitter-css/src/scanner.c
    grammars/tree-sitter-c/src/parser.c
    grammars/tree-sitter-rust/src/parser.c
    grammars/tree-sitter-rust/src/scanner.c
    )
    # Removed unused files:
    # src/ast_parser.cpp (replaced by unified_ast_backend.cpp)
    # src/language_handler.cpp (replaced by language_adapter.cpp) 
    # Temporarily disabled while fixing issues:
    # src/read_ast_objects_function.cpp
    # src/ast_helper_functions.cpp

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Make extension targets depend on generated parsers
if(TARGET generate-parsers)
    add_dependencies(${EXTENSION_NAME} generate-parsers)
    add_dependencies(${LOADABLE_EXTENSION_NAME} generate-parsers)
    message(STATUS "Extension targets will depend on generated parsers")
endif()

# Link tree-sitter library
target_link_libraries(${EXTENSION_NAME} tree-sitter)
target_link_libraries(${LOADABLE_EXTENSION_NAME} tree-sitter)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
