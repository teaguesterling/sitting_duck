# DuckDB AST Extension API Reference

## Table Functions

### read_ast(file_path, language)
**Type**: Table Function  
**Returns**: TABLE(ast JSON)  
**Description**: Parses a source file and returns the complete AST as a single JSON object.  
**Example**: `SELECT ast FROM read_ast('script.py', 'python');`

### read_ast_objects(file_path, language)
**Type**: Table Function  
**Returns**: TABLE(file_path VARCHAR, language VARCHAR, node_count UBIGINT, max_depth UBIGINT, nodes JSON)  
**Description**: Parses a source file and returns AST data in a structured format with metadata.  
**Example**: `SELECT * FROM read_ast_objects('script.py', 'python');`

## Entrypoint Macro

### ast(input)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Normalizes various input types (NULL, JSON, struct, string) into a standardized JSON array of AST nodes.  
**Example**: `SELECT ast(nodes).find_type('function_definition') FROM read_ast_objects('script.py', 'python');`

## Core Extraction Macros (return JSON arrays)

### ast_find_type(nodes, types)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds all nodes of specified type(s) - accepts single type or array of types.  
**Example**: `SELECT ast_find_type(nodes, 'function_definition') FROM read_ast_objects('script.py', 'python');`

### ast_function_names(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Extracts all function definition names as a JSON array.  
**Example**: `SELECT ast_function_names(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_class_names(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Extracts all class definition names as a JSON array.  
**Example**: `SELECT ast_class_names(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_identifiers(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Extracts all identifier names as a JSON array.  
**Example**: `SELECT ast_identifiers(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_strings(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Extracts all string literal contents as a JSON array (currently returns empty - strings don't have content field).  
**Example**: `SELECT ast_strings(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_at_depth(nodes, depths)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds all nodes at specified depth(s) - accepts single depth or array of depths.  
**Example**: `SELECT ast_at_depth(nodes, [1, 2]) FROM read_ast_objects('script.py', 'python');`

### ast_children_of(nodes, parent_id)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Gets direct children of a node by its ID.  
**Example**: `SELECT ast_children_of(nodes, 0) FROM read_ast_objects('script.py', 'python');`

### ast_function_details(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Extracts detailed information about function definitions including name, line numbers, depth, and ID.  
**Example**: `SELECT ast_function_details(nodes) FROM read_ast_objects('script.py', 'python');`

## Analysis Macros

### ast_type_counts(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Returns a JSON object with counts of each node type.  
**Example**: `SELECT ast_type_counts(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_summary(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Returns comprehensive summary statistics about the AST including total nodes, max depth, and type counts.  
**Example**: `SELECT ast_summary(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_complexity(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Returns complexity metrics including node counts, depth statistics, and lines of code.  
**Example**: `SELECT ast_complexity(nodes) FROM read_ast_objects('script.py', 'python');`

## Filtering Macros (return JSON arrays)

### ast_at_line(nodes, lines)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds nodes that span specified line(s) - accepts single line or array of lines.  
**Example**: `SELECT ast_at_line(nodes, [10, 20, 30]) FROM read_ast_objects('script.py', 'python');`

### ast_in_line_range(nodes, start_line, end_line)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds nodes within a line number range.  
**Example**: `SELECT ast_in_line_range(nodes, 10, 50) FROM read_ast_objects('script.py', 'python');`

### ast_contains_text(nodes, search_text)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds nodes whose name or content contains the search text.  
**Example**: `SELECT ast_contains_text(nodes, 'test') FROM read_ast_objects('script.py', 'python');`

### ast_filter_name_pattern(nodes, pattern, case_sensitive)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Filters nodes by name using SQL LIKE pattern matching.  
**Example**: `SELECT ast_filter_name_pattern(nodes, '%test%', false) FROM read_ast_objects('script.py', 'python');`

### ast_at_depth_range(nodes, min_depth, max_depth)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds nodes within a depth range.  
**Example**: `SELECT ast_at_depth_range(nodes, 2, 4) FROM read_ast_objects('script.py', 'python');`

## Tree Navigation Macros

### ast_parent_of(nodes, child_id)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Gets the parent node of a given node ID.  
**Example**: `SELECT ast_parent_of(nodes, 42) FROM read_ast_objects('script.py', 'python');`

### ast_siblings_of(nodes, node_id, include_self)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Gets sibling nodes of a given node ID.  
**Example**: `SELECT ast_siblings_of(nodes, 42, false) FROM read_ast_objects('script.py', 'python');`

### ast_ancestors_of(nodes, node_id, max_levels)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Gets ancestor nodes up to specified levels using recursive CTE.  
**Example**: `SELECT ast_ancestors_of(nodes, 42, 3) FROM read_ast_objects('script.py', 'python');`

### ast_descendants_of(nodes, node_id, max_levels)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Gets descendant nodes down to specified levels using recursive CTE.  
**Example**: `SELECT ast_descendants_of(nodes, 42, NULL) FROM read_ast_objects('script.py', 'python');`

## Structure-Preserving Macros (return AST objects)

### ast_filter_type(ast_obj, types)
**Type**: Scalar Macro  
**Returns**: STRUCT  
**Description**: Filters AST to only include nodes of specified type(s), preserving file metadata.  
**Example**: `SELECT ast_filter_type(ast_obj, 'function_definition') FROM read_ast_objects('script.py', 'python') AS ast_obj;`

### ast_filter_name(ast_obj, patterns, case_sensitive)
**Type**: Scalar Macro  
**Returns**: STRUCT  
**Description**: Filters AST by name patterns, preserving structure.  
**Example**: `SELECT ast_filter_name(ast_obj, ['%test%'], false) FROM read_ast_objects('script.py', 'python') AS ast_obj;`

### ast_filter_depth(ast_obj, depths)
**Type**: Scalar Macro  
**Returns**: STRUCT  
**Description**: Filters AST to only include nodes at specified depth(s).  
**Example**: `SELECT ast_filter_depth(ast_obj, [1, 2]) FROM read_ast_objects('script.py', 'python') AS ast_obj;`

### ast_filter_depth_range(ast_obj, min_depth, max_depth)
**Type**: Scalar Macro  
**Returns**: STRUCT  
**Description**: Filters AST to nodes within a depth range.  
**Example**: `SELECT ast_filter_depth_range(ast_obj, 2, 4) FROM read_ast_objects('script.py', 'python') AS ast_obj;`

### ast_filter_line(ast_obj, lines, include_partial)
**Type**: Scalar Macro  
**Returns**: STRUCT  
**Description**: Filters AST to nodes on specified line(s).  
**Example**: `SELECT ast_filter_line(ast_obj, [10, 20], true) FROM read_ast_objects('script.py', 'python') AS ast_obj;`

### ast_filter_descendants(ast_obj, node_id, max_levels)
**Type**: Scalar Macro  
**Returns**: STRUCT  
**Description**: Filters AST to only include descendants of a specific node.  
**Example**: `SELECT ast_filter_descendants(ast_obj, 0, 2) FROM read_ast_objects('script.py', 'python') AS ast_obj;`

## Extraction Macros (SQL-friendly types)

### ast_extract_names(nodes, node_type)
**Type**: Scalar Macro  
**Returns**: VARCHAR[]  
**Description**: Extracts names from nodes of a specific type as a VARCHAR array.  
**Example**: `SELECT ast_extract_names(nodes, 'function_definition') FROM read_ast_objects('script.py', 'python');`

### ast_extract_all_names(nodes)
**Type**: Scalar Macro  
**Returns**: VARCHAR[]  
**Description**: Extracts all names from any nodes that have a name field.  
**Example**: `SELECT ast_extract_all_names(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_extract_strings(nodes)
**Type**: Scalar Macro  
**Returns**: VARCHAR[]  
**Description**: Extracts string literal values as a VARCHAR array.  
**Example**: `SELECT ast_extract_strings(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_extract_node_types(nodes)
**Type**: Scalar Macro  
**Returns**: VARCHAR[]  
**Description**: Extracts all unique node types as a VARCHAR array.  
**Example**: `SELECT ast_extract_node_types(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_extract_line_numbers(nodes)
**Type**: Scalar Macro  
**Returns**: INTEGER[]  
**Description**: Extracts all unique starting line numbers.  
**Example**: `SELECT ast_extract_line_numbers(nodes) FROM read_ast_objects('script.py', 'python');`

## Source Code Macros

### ast_source_for_node(nodes, node_id, file_path)
**Type**: Scalar Macro  
**Returns**: VARCHAR  
**Description**: Extracts source code for a specific node by reading the file.  
**Example**: `SELECT ast_source_for_node(nodes, 42, 'script.py') FROM read_ast_objects('script.py', 'python');`

### ast_source_for_function(nodes, function_name, file_path)
**Type**: Scalar Macro  
**Returns**: VARCHAR  
**Description**: Extracts source code for a function by name.  
**Example**: `SELECT ast_source_for_function(nodes, 'main', 'script.py') FROM read_ast_objects('script.py', 'python');`

### ast_source_for_class(nodes, class_name, file_path)
**Type**: Scalar Macro  
**Returns**: VARCHAR  
**Description**: Extracts source code for a class by name.  
**Example**: `SELECT ast_source_for_class(nodes, 'MyClass', 'script.py') FROM read_ast_objects('script.py', 'python');`

### ast_source_context(nodes, node_id, file_path, before_lines, after_lines)
**Type**: Scalar Macro  
**Returns**: VARCHAR  
**Description**: Extracts source code with surrounding context lines.  
**Example**: `SELECT ast_source_context(nodes, 42, 'script.py', 2, 2) FROM read_ast_objects('script.py', 'python');`

## AI/Discoverability Macros

### ast_help()
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Returns documentation about available AST operations and example queries.  
**Example**: `SELECT ast_help();`

### ast_examples()
**Type**: Scalar Macro  
**Returns**: TABLE  
**Description**: Returns a table of example queries with descriptions.  
**Example**: `SELECT * FROM ast_examples();`

### ast_common_patterns()
**Type**: Scalar Macro  
**Returns**: TABLE  
**Description**: Returns common AST query patterns for typical use cases.  
**Example**: `SELECT * FROM ast_common_patterns();`

### ast_explain_node(node)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Provides detailed explanation of a node's structure and relationships.  
**Example**: `SELECT ast_explain_node(node) FROM json_each(nodes) AS node LIMIT 1;`

### ast_search_suggestions(partial_query)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Suggests completions or corrections for partial AST queries.  
**Example**: `SELECT ast_search_suggestions('find all func');`

## Utility Macros

### ast_all_functions(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds both function and method definitions.  
**Example**: `SELECT ast_all_functions(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_all_definitions(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds all definition types (functions, classes, methods).  
**Example**: `SELECT ast_all_definitions(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_all_literals(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds all literal value nodes (string, integer, float, boolean, null).  
**Example**: `SELECT ast_all_literals(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_control_flow(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Finds all control flow statements (if, while, for, try, with).  
**Example**: `SELECT ast_control_flow(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_top_level(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Gets nodes at top levels (depths 0-2).  
**Example**: `SELECT ast_top_level(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_mid_level(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Gets nodes at mid levels (depths 3-5).  
**Example**: `SELECT ast_mid_level(nodes) FROM read_ast_objects('script.py', 'python');`

### ast_deep_nodes(nodes)
**Type**: Scalar Macro  
**Returns**: JSON  
**Description**: Gets deeply nested nodes (depth > 5).  
**Example**: `SELECT ast_deep_nodes(nodes) FROM read_ast_objects('script.py', 'python');`

## Helper Macros (not directly user-facing)

### ensure_varchar_array(val)
**Type**: Scalar Macro  
**Returns**: VARCHAR[]  
**Description**: Ensures a value is a VARCHAR array, wrapping single values if needed.  
**Example**: Internal use only

### ensure_integer_array(val)
**Type**: Scalar Macro  
**Returns**: INTEGER[]  
**Description**: Ensures a value is an INTEGER array, wrapping single values if needed.  
**Example**: Internal use only

### ast_normalize_to_int_array(val)
**Type**: Scalar Macro  
**Returns**: INTEGER[]  
**Description**: Normalizes various integer types to INTEGER array.  
**Example**: Internal use only

### ast_normalize_to_varchar_array(val)
**Type**: Scalar Macro  
**Returns**: VARCHAR[]  
**Description**: Normalizes various string types to VARCHAR array.  
**Example**: Internal use only

## Method Chain Support

All macros can be used with method syntax when wrapped in parentheses:
- `(nodes).ast_find_type('function_definition')`
- `nodes.ast_find_type('function_definition')` (also works!)

The `ast()` entrypoint enables natural chaining:
- `ast(nodes).find_type('function_definition').extract_names().count_elements()`

Additional chain-only methods:
- `count_elements()` - counts elements in a JSON array
- `first_element()` - gets first element of JSON array
- `last_element()` - gets last element of JSON array
- `extract_names()` - extracts name fields from nodes
- `where_type()` - filters to specific node type
- `where_depth()` - filters to specific depth