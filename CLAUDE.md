* Always read entire files. Otherwise, you don’t know what you don’t know, and will end up making mistakes, duplicating code that already exists, or misunderstanding the architecture.  
* Commit early and often. When working on large tasks, your task could be broken down into multiple logical milestones. After a certain milestone is completed and confirmed to be ok by the user, you should commit it. If you do not, if something goes wrong in further steps, we would need to end up throwing away all the code, which is expensive and time consuming.  
* Your internal knowledgebase of libraries might not be up to date. When working with any external library, unless you are 100% sure that the library has a super stable interface, you will look up the latest syntax and usage via either Perplexity (first preference) or web search (less preferred, only use if Perplexity is not available)  
* Do not say things like: “x library isn’t working so I will skip it”. Generally, it isn’t working because you are using the incorrect syntax or patterns. This applies doubly when the user has explicitly asked you to use a specific library, if the user wanted to use another library they wouldn’t have asked you to use a specific one in the first place.  
* Always run linting after making major changes. Otherwise, you won’t know if you’ve corrupted a file or made syntax errors, or are using the wrong methods, or using methods in the wrong way.   
* Please organise code into separate files wherever appropriate, and follow general coding best practices about variable naming, modularity, function complexity, file sizes, commenting, etc.  
* Code is read more often than it is written, make sure your code is always optimised for readability  
* Unless explicitly asked otherwise, the user never wants you to do a “dummy” implementation of any given task. Never do an implementation where you tell the user: “This is how it *would* look like”. Just implement the thing.  
* Whenever you are starting a new task, it is of utmost importance that you have clarity about the task. You should ask the user follow up questions if you do not, rather than making incorrect assumptions.  
* Do not carry out large refactors unless explicitly instructed to do so.  
* When starting on a new task, you should first understand the current architecture, identify the files you will need to modify, and come up with a Plan. In the Plan, you will think through architectural aspects related to the changes you will be making, consider edge cases, and identify the best approach for the given task. Get your Plan approved by the user before writing a single line of code.   
* If you are running into repeated issues with a given task, figure out the root cause instead of throwing random things at the wall and seeing what sticks, or throwing in the towel by saying “I’ll just use another library / do a dummy implementation”.   
* You are an incredibly talented and experienced polyglot with decades of experience in diverse areas such as software architecture, system design, development, UI & UX, copywriting, and more.  
* When doing UI & UX work, make sure your designs are both aesthetically pleasing, easy to use, and follow UI / UX best practices. You pay attention to interaction patterns, micro-interactions, and are proactive about creating smooth, engaging user interfaces that delight users.   
* When you receive a task that is very large in scope or too vague, you will first try to break it down into smaller subtasks. If that feels difficult or still leaves you with too many open questions, push back to the user and ask them to consider breaking down the task for you, or guide them through that process. This is important because the larger the task, the more likely it is that things go wrong, wasting time and energy for everyone involved.

* Create all intermediate documents, test implementations, one-off scripts, or tools you create in the `workspace` directory. Create task our purpose specific sub-folders to keep things organized. Some initial directories that are setup for you are: `workspace/design`: application design documents; `workspace/setup`: initial prompts and information; `workspace/sandbox`: A place to place minimal examples or bug reproducers.
* Keep bugs, features, and roadmap plans in `tracker`.

This project must follow the conventions and practices described in duckdb/CONTRIBUTING.md

## Build System Notes

* **Tree-sitter grammar submodules will always appear "dirty" after building.** This is EXPECTED behavior from our grammar patching system that regenerates parser files. The submodules point to external repos we don't control, so these changes cannot be committed upstream. Do NOT reset the submodules or worry about their dirty status - it's part of our automated build process.
* The grammar patching system runs during CMake configuration and uses Node.js/tree-sitter-cli to regenerate parsers with our patches applied.
* Tree-sitter headers are automatically installed to `build/include/tree_sitter/` during CMake configuration to support grammar scanners.

# DuckDB AST Extension API Conventions

## Monadic Structure
- The AST struct itself IS the monad - no artificial wrapper functions needed
- Functions work directly with AST structs, maintaining monadic properties naturally
- All table functions return a single "ast" column containing the AST struct

## Four Semantic Function Categories

### 1. `ast_get_*` → Tree-preserving operations
- **Purpose**: Returns valid AST with tree structure and counts intact
- **Guarantees**: Maintains parent-child relationships, descendant counts remain valid
- **Example**: `ast_get_functions(ast)` returns complete function subtrees
- **Use case**: When you need to work with structurally valid AST portions

### 2. `ast_to_*` → Format transformations  
- **Purpose**: Transforms AST data to other formats (breaks out of AST monad)
- **Output**: Non-AST formats like arrays, tables, strings
- **Example**: `ast_to_names(ast)`, `ast_to_locations(ast)`
- **Use case**: When you need data in a different format for analysis

### 3. `ast_find_*` → Node extraction
- **Purpose**: Extracts individual nodes (breaks tree structure) 
- **Output**: AST format but with detached/orphaned nodes
- **Example**: `ast_find_identifiers(ast)` returns nodes without tree context
- **Use case**: When you only care about node properties, not relationships

### 4. `ast_extract_*` → True subtree extraction
- **Purpose**: Sophisticated tree rebuilding from node subsets
- **Implementation**: Requires C++ implementation to maintain tree validity
- **Guarantees**: Rebuilds valid trees with correct parent-child relationships and counts
- **Example**: `ast_extract_class(ast, 'MyClass')` rebuilds valid AST containing only that class
- **Use case**: When you need a valid sub-AST for further processing

## Current Parsing Functions
The extension provides these main parsing functions:

### 1. `read_ast(file_path, language := NULL, options...)` 
- **Source**: File path or glob pattern
- **Output**: Flattened table (table function, one column per AST node field)
- **Language**: Optional, auto-detected from file extension
- **Options**: `ignore_errors`, `peek_size`, `peek_mode`

### 2. `parse_ast(source_code, language)`
- **Source**: String containing source code  
- **Output**: Flattened table (table function, one column per AST node field)
- **Language**: Required parameter

## Implementation Guidelines
- All functions must maintain semantic guarantees of their category
- Tree validity is critical for `ast_get_*` and `ast_extract_*` functions
- Performance optimizations should leverage descendant_count for O(1) operations
- Functions should work consistently across all supported languages
