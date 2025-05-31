# SQL Macro files to embed
set(SQL_MACRO_FILES
    01_core_primitives.sql
    02_ast_get.sql
    03_ast_find.sql
    04_ast_to.sql
)

# Generate header file with embedded SQL
set(EMBEDDED_SQL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/embedded_sql_macros.hpp")

file(WRITE ${EMBEDDED_SQL_HEADER} 
"// Auto-generated file - DO NOT EDIT
// Generated from SQL macro files in src/sql_macros/
#pragma once

#include <string>
#include <vector>
#include <utility>

namespace duckdb {

static const std::vector<std::pair<std::string, std::string>> EMBEDDED_SQL_MACROS = {
")

foreach(SQL_FILE ${SQL_MACRO_FILES})
    file(READ ${SQL_FILE} SQL_CONTENT)
    # Escape quotes and newlines
    string(REPLACE "\\" "\\\\" SQL_CONTENT "${SQL_CONTENT}")
    string(REPLACE "\"" "\\\"" SQL_CONTENT "${SQL_CONTENT}")
    string(REPLACE "\n" "\\n" SQL_CONTENT "${SQL_CONTENT}")
    
    file(APPEND ${EMBEDDED_SQL_HEADER} 
"    {\"${SQL_FILE}\", R\"SQLMACRO(
${SQL_CONTENT}
)SQLMACRO\"},
")
endforeach()

file(APPEND ${EMBEDDED_SQL_HEADER} 
"};

} // namespace duckdb
")

# Make the header available to parent scope
set(EMBEDDED_SQL_MACROS_HEADER ${EMBEDDED_SQL_HEADER} PARENT_SCOPE)