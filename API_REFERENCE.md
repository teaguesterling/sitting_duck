# Sitting Duck API Reference

**Complete reference for the Sitting Duck DuckDB extension - 71 functions for comprehensive AST analysis.**

## Table of Contents

1. [Core Functions (16)](#core-functions-16)
2. [SQL Macros (55)](#sql-macros-55)
3. [Semantic Type System](#semantic-type-system)
4. [Language Support](#language-support)
5. [Common Patterns](#common-patterns)

---

## Core Functions (16)

### Table Functions (3)

#### `read_ast(file_path, [language])`
**Main parsing function** - Reads files and returns flattened AST table.

**Parameters:**
- `file_path` (VARCHAR): File path or glob pattern (`'src/*.py'`, `'**/*.js'`)
- `language` (VARCHAR, optional): Language override (auto-detected if omitted)

**Returns:** Table with complete AST node data
- `node_id`, `parent_id` (BIGINT): Node relationships
- `type`, `name` (VARCHAR): Node type and extracted name
- `file_path` (VARCHAR): Source file
- `start_line`, `end_line`, `start_column`, `end_column` (INTEGER): Location
- `depth`, `sibling_index` (INTEGER): Tree position
- `children_count`, `descendant_count` (INTEGER): Tree metrics
- `semantic_type` (UTINYINT): Cross-language semantic classification
- `peek` (VARCHAR): Source code preview

**Examples:**
```sql
-- Parse all Python files
SELECT * FROM read_ast('**/*.py');

-- Find all functions
SELECT name, start_line FROM read_ast('main.py') 
WHERE type LIKE '%function%';
```

#### `parse_ast(source_code, language)`
**String parsing** - Parses source code string.

**Parameters:**
- `source_code` (VARCHAR): Source code to parse
- `language` (VARCHAR): Programming language

**Returns:** Same schema as `read_ast`

**Example:**
```sql
SELECT * FROM parse_ast('def hello(): return "world"', 'python');
```

### Utility Functions (1)

#### `ast_supported_languages()`
**Language metadata** - Returns supported languages and their properties.

**Returns:** Table with:
- `language` (VARCHAR): Language identifier
- `aliases` (LIST): Alternative names
- `extensions` (LIST): File extensions

**Example:**
```sql
SELECT * FROM ast_supported_languages();
```

### Semantic Type Functions (12)

#### `semantic_type_to_string(type_code)`
Converts semantic type code to human-readable name.

**Example:**
```sql
SELECT semantic_type_to_string(112);  -- 'DEFINITION_FUNCTION'
```

#### `semantic_type_code(type_name)`
Converts semantic type name to numeric code.

**Example:**
```sql
SELECT semantic_type_code('DEFINITION_FUNCTION');  -- 112
```

#### `get_super_kind(type_code)` / `get_kind(type_code)`
Get hierarchical classification for semantic types.

#### `kind_code(kind_name)` / `is_kind(type_code, kind_name)`
Work with kind-level classification.

#### Predicate Functions
- `is_definition(type_code)` - Check if node is a definition
- `is_call(type_code)` - Check if node is a function call  
- `is_control_flow(type_code)` - Check if node is control flow
- `is_identifier(type_code)` - Check if node is an identifier
- `is_semantic_type(type_code, pattern)` - Pattern matching

#### `get_searchable_types()`
Returns list of semantic types useful for indexing.

---

## SQL Macros (55)

**Comprehensive AST analysis library loaded automatically at extension startup.**

### Core Primitives (10 macros)

#### Indexing & Structure
- `ast_with_indices(nodes)` - Add indices for filtering operations
- `ast_extract_subtrees(nodes, filtered)` - Extract complete subtrees
- `ast_update(ast, new_nodes)` - Update AST with new node array
- `ast_pack(file_path, language, nodes)` - Create AST struct

#### Filtering Operations
- `ast_filter_by_type(nodes, type)` - Filter by exact type
- `ast_filter_by_types(nodes, types)` - Filter by type list
- `ast_filter_by_name(nodes, name)` - Filter by node name
- `ast_filter_by_depth(nodes, depth)` - Filter by tree depth
- `ast_get_by_type(ast, type)` - Get complete subtrees by type
- `ast_get_by_types(ast, types)` - Get complete subtrees by type list

### AST Get Functions (8 macros)

**Tree-preserving operations** that return valid ASTs with complete subtrees.

#### Basic Operations
- `ast_get_types(ast, types)` - Get nodes by type list
- `ast_get_type(ast, type)` - Get nodes by single type
- `ast_get_depth(ast, depth)` - Get nodes at specific depth
- `ast_get_name(ast, name)` - Get nodes with specific name

#### Language-Agnostic Semantic Functions
- `ast_get_functions(ast)` - Get all function definitions
- `ast_get_classes(ast)` - Get all class definitions  
- `ast_get_imports(ast)` - Get all import/include statements
- `ast_get_top_level(ast)` - Get top-level nodes (depth 1)

### AST Find Functions (11 macros)

**Node extraction operations** that return detached nodes (breaks tree structure).

#### Basic Find Operations
- `ast_find_types(ast, types)` - Find nodes by type list
- `ast_find_type(ast, type)` - Find nodes by single type
- `ast_find_depth(ast, depth)` - Find nodes at specific depth
- `ast_find_name(ast, name)` - Find nodes by name
- `ast_find_identifiers(ast)` - Find all identifiers
- `ast_find_literals(ast)` - Find all literal values

#### Semantic Find Operations
- `ast_find_calls(ast)` - Find function/method calls
- `ast_find_declarations(ast)` - Find variable declarations
- `ast_find_control_flow(ast)` - Find control flow statements
- `ast_find_leaves(ast)` - Find leaf nodes (no children)
- `ast_find_parents(ast)` - Find parent nodes (have children)

### AST Transform Functions (11 macros)

**Format transformations** that break out of AST monad.

#### Basic Transformations
- `ast_to_names(ast, [type])` - Extract names as array
- `ast_to_types(ast)` - Extract unique types as array  
- `ast_to_source(ast, [type])` - Extract source snippets
- `ast_to_locations(ast)` - Convert to location table

#### Statistical Transformations
- `ast_to_type_stats(ast)` - Type frequency analysis
- `ast_to_depth_stats(ast)` - Depth distribution analysis
- `ast_to_complexity_metrics(ast)` - Complexity analysis

#### Code Analysis Transformations
- `ast_to_signatures(ast)` - Extract function signatures
- `ast_to_dependencies(ast)` - Extract import/dependency list
- `ast_to_call_edges(ast)` - Build call graph relationships
- `ast_to_summary(ast)` - Overall AST summary statistics

### Taxonomy Functions (15 macros)

**Semantic classification** using the KIND system.

#### Kind-Based Filters
- `ast_filter_by_kind(nodes, kind_value)` - Filter by semantic kind
- `ast_filter_by_kinds(nodes, kind_values)` - Filter by multiple kinds
- `ast_filter_functions_by_kind(nodes)` - Get function-like nodes
- `ast_filter_literals_by_kind(nodes)` - Get literal nodes
- `ast_filter_names_by_kind(nodes)` - Get name/identifier nodes

#### Flag-Based Filters
- `ast_filter_public(nodes)` - Get public nodes
- `ast_filter_builtin(nodes)` - Get builtin nodes
- `ast_filter_keywords(nodes)` - Get keyword nodes

#### Semantic Operations
- `ast_group_by_semantic_id(nodes)` - Group by semantic similarity
- `ast_find_semantic_pattern(nodes, pattern_id)` - Find semantic patterns

#### Complete Operations with Taxonomy
- `ast_get_functions_by_kind(ast)` - Get functions using KIND system
- `ast_get_public_interface(ast)` - Get public interface nodes
- `ast_get_literals_by_kind(ast)` - Get literals using KIND system
- `ast_find_semantic_functions(ast)` - Cross-language function finding
- `ast_find_control_flow_by_kind(ast)` - Find control flow by KIND

---

## Semantic Type System

**8-bit hierarchical classification system** for cross-language analysis.

### Hierarchy (4 levels)

1. **Super Kinds** (top level):
   - `DATA_STRUCTURE` (0x00-0x3F): Literals, names, types
   - `COMPUTATION` (0x40-0x7F): Operations, definitions  
   - `CONTROL_EFFECTS` (0x80-0xBF): Flow control, organization
   - `META_EXTERNAL` (0xC0-0xFF): Metadata, imports, syntax

2. **Examples by Category**:
   - **Data**: `LITERAL_STRING=0x00`, `NAME_IDENTIFIER=0x04`, `TYPE_PRIMITIVE=0x08`
   - **Computation**: `COMPUTATION_CALL=0x40`, `DEFINITION_FUNCTION=0x44`
   - **Control**: `FLOW_CONDITIONAL=0x80`, `FLOW_LOOP=0x81`
   - **Meta**: `EXTERNAL_IMPORT=0xC0`, `METADATA_COMMENT=0xC4`

### Usage Patterns

```sql
-- Find all definitions across languages
SELECT * FROM read_ast('**/*.*') 
WHERE (semantic_type & 240) = 112;

-- Find public functions using taxonomy
SELECT ast_to_names(ast_get_functions_by_kind(
    ast_filter_public(ast.nodes)
)) FROM read_ast_objects('*.py');
```

---

## Language Support

**13 languages** with automatic file extension detection:

| Language | Extensions | Grammar Source |
|----------|------------|----------------|
| **Python** | `.py` | tree-sitter-python |
| **JavaScript** | `.js`, `.mjs`, `.cjs` | tree-sitter-javascript |
| **TypeScript** | `.ts`, `.tsx` | tree-sitter-typescript |
| **C++** | `.cpp`, `.cc`, `.cxx`, `.hpp`, `.h` | tree-sitter-cpp |
| **C** | `.c`, `.h` | tree-sitter-c |
| **Java** | `.java` | tree-sitter-java |
| **Go** | `.go` | tree-sitter-go |
| **Ruby** | `.rb` | tree-sitter-ruby |
| **PHP** | `.php` | tree-sitter-php |
| **HTML** | `.html`, `.htm` | tree-sitter-html |
| **CSS** | `.css` | tree-sitter-css |
| **Rust** | `.rs` | tree-sitter-rust |
| **Markdown** | `.md`, `.markdown` | tree-sitter-markdown |

---

## Common Patterns

### Project Analysis
```sql
-- Find all function definitions in a project
SELECT file_path, name, start_line
FROM read_ast('**/*.py')
WHERE semantic_type = semantic_type_code('DEFINITION_FUNCTION');

-- Complexity analysis
SELECT name, descendant_count as complexity
FROM read_ast('**/*.js') 
WHERE type LIKE '%function%' AND descendant_count > 50
ORDER BY complexity DESC;
```

### Cross-Language Search
```sql
-- Find all imports across languages
SELECT file_path, peek as import_statement
FROM read_ast('**/*.*')
WHERE semantic_type = semantic_type_code('EXTERNAL_IMPORT');

-- Public interface extraction
WITH ast_data AS (
    SELECT ast_pack(file_path, 'python', 
        list(struct_pack(node_id, type, name, semantic_type, start_line, 
                        end_line, parent_id, depth, children_count, 
                        descendant_count, peek))) as ast
    FROM read_ast('**/*.py')
)
SELECT ast_to_names(ast_get_public_interface(ast))
FROM ast_data;
```

### Using SQL Macros with Flat Tables
```sql
-- Convert flat table to AST struct for macro usage
WITH ast_struct AS (
    SELECT ast_pack(
        file_path, 
        'python',
        list(struct_pack(
            node_id, type, name, start_line, end_line, 
            parent_id, depth, children_count, descendant_count, 
            semantic_type, peek
        ) ORDER BY node_id)
    ) as ast
    FROM read_ast('main.py')
    GROUP BY file_path
)
SELECT ast_to_signatures(ast)
FROM ast_struct;
```

### Performance Optimization
```sql
-- Index frequently searched nodes
CREATE TABLE code_index AS
SELECT * FROM read_ast('**/*.*')
WHERE semantic_type = ANY(get_searchable_types());

-- Efficient pattern searches
SELECT * FROM code_index 
WHERE is_definition(semantic_type) 
  AND name LIKE '%test%';
```

---

## API Design Philosophy

**Monadic Structure:** The AST struct IS the monad - functions work directly with AST structs maintaining tree relationships.

**Four Function Categories:**
1. **`ast_get_*`** → Tree-preserving (maintains valid AST structure)
2. **`ast_find_*`** → Node extraction (breaks tree, returns detached nodes)  
3. **`ast_to_*`** → Format transformations (exits AST monad)
4. **`ast_filter_*`** → Low-level filtering primitives

**SQL-First:** Comprehensive functionality available through SQL macros, with optional Python CLI for orchestration.

---

*Total: **71 functions** (16 core C++ + 55 SQL macros) for complete AST analysis capabilities.*