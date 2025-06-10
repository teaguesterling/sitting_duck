#!/usr/bin/env python3
"""
File Function Analyzer
Provides a detailed view of all functions in a single file using the DuckDB AST extension.

Usage:
  python scripts/analyze_file_functions.py <file_path> [--db <db_path>] [--output <format>]
  
Examples:
  python scripts/analyze_file_functions.py src/semantic_type_functions.cpp
  python scripts/analyze_file_functions.py src/unified_ast_backend.cpp --output json
  python scripts/analyze_file_functions.py "src/*.cpp" --db my_ast.db
"""

import argparse
import json
import sys
import subprocess
import tempfile
import os
from pathlib import Path

def run_duckdb_query(db_path, query):
    """Run a DuckDB query and return the result."""
    try:
        cmd = ['./build/release/duckdb', db_path, '-c', query]
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error running DuckDB query: {e}", file=sys.stderr)
        print(f"stderr: {e.stderr}", file=sys.stderr)
        return None

def parse_file_to_db(file_path, temp_db):
    """Parse a file and load it into a temporary database."""
    print(f"Parsing {file_path}...", file=sys.stderr)
    
    # Load extension and create table
    setup_query = """
    LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';
    CREATE TABLE nodes AS SELECT * FROM read_ast('{}');
    """.format(file_path)
    
    result = run_duckdb_query(temp_db, setup_query)
    if result is None:
        return False
    return True

def analyze_functions(db_path, file_path):
    """Analyze functions in the database for the given file."""
    
    # Load the semantic extraction macros
    macros_path = Path(__file__).parent.parent / 'extract_functions_macros.sql'
    
    analysis_query = f"""
    .read {macros_path}
    
    -- Get detailed function information for the specific file
    WITH func_declarators AS (
        SELECT 
            file_path,
            node_id,
            parent_id,
            start_line,
            end_line,
            children_count,
            descendant_count
        FROM nodes
        WHERE type = 'function_declarator'
          AND semantic_type = 112
          AND file_path = '{file_path}'
    ),
    declarator_info AS (
        SELECT 
            d.file_path,
            d.node_id,
            d.start_line,
            d.end_line,
            d.parent_id,
            d.descendant_count,
            -- Extract name from identifier child
            MAX(CASE 
                WHEN c.type = 'identifier' THEN c.peek
                WHEN c.type = 'qualified_identifier' THEN c.peek
            END) as full_name,
            -- Count parameters from parameter_list children
            SUM(CASE WHEN c.type = 'parameter_list' THEN c.children_count ELSE 0 END) as param_count
        FROM func_declarators d
        JOIN nodes c ON c.parent_id = d.node_id AND c.file_path = d.file_path
        GROUP BY d.file_path, d.node_id, d.start_line, d.end_line, d.parent_id, d.descendant_count
    ),
    definition_info AS (
        SELECT 
            di.file_path,
            di.full_name,
            di.start_line,
            di.end_line,
            di.param_count,
            di.descendant_count,
            p.type as parent_type,
            p.start_line as def_start_line,
            p.end_line as def_end_line,
            p.descendant_count as def_complexity,
            p.peek as function_signature
        FROM declarator_info di
        LEFT JOIN nodes p ON p.node_id = di.parent_id AND p.file_path = di.file_path
    ),
    function_parameters AS (
        SELECT 
            di.node_id,
            di.full_name,
            param_decl.sibling_index,
            -- Get parameter type
            MAX(CASE 
                WHEN param_child.type IN ('type_identifier', 'primitive_type') 
                     AND param_child.sibling_index = 0 
                THEN param_child.peek
            END) as param_type,
            -- Get parameter name  
            MAX(CASE 
                WHEN param_child.type = 'identifier' 
                     AND param_child.sibling_index > 0
                THEN param_child.peek
            END) as param_name
        FROM declarator_info di
        JOIN nodes param_list ON param_list.parent_id = di.node_id 
            AND param_list.type = 'parameter_list'
            AND param_list.file_path = di.file_path
        JOIN nodes param_decl ON param_decl.parent_id = param_list.node_id 
            AND param_decl.type = 'parameter_declaration'
            AND param_decl.file_path = di.file_path
        LEFT JOIN nodes param_child ON param_child.parent_id = param_decl.node_id
            AND param_child.file_path = di.file_path
        GROUP BY di.node_id, di.full_name, param_decl.node_id, param_decl.sibling_index
    )
    SELECT 
        def.full_name as function_name,
        -- Extract class name from qualified names
        CASE 
            WHEN def.full_name LIKE '%::%' THEN 
                REGEXP_EXTRACT(def.full_name, '^(.+)::[^:]+$', 1)
            ELSE NULL
        END as class_name,
        -- Extract simple function name
        CASE 
            WHEN def.full_name LIKE '%::%' THEN 
                REGEXP_EXTRACT(def.full_name, '::([^:]+)$', 1)
            ELSE def.full_name
        END as simple_name,
        -- Determine if it's a method or function
        CASE 
            WHEN def.full_name LIKE '%::%' THEN 'method'
            ELSE 'function'
        END as function_type,
        -- Use definition boundaries if available, else declarator
        COALESCE(def.def_start_line, def.start_line) as start_line,
        COALESCE(def.def_end_line, def.end_line) as end_line,
        COALESCE(def.def_end_line, def.end_line) - COALESCE(def.def_start_line, def.start_line) + 1 as line_count,
        def.param_count as parameter_count,
        -- Use definition complexity if available
        COALESCE(def.def_complexity, def.descendant_count) as complexity,
        def.parent_type,
        substring(def.function_signature, 1, 100) as signature_preview,
        -- Aggregate parameters
        array_agg(param.param_type || ' ' || COALESCE(param.param_name, '') 
                  ORDER BY param.sibling_index) FILTER (WHERE param.param_type IS NOT NULL) as parameters
    FROM definition_info def
    LEFT JOIN function_parameters param ON param.node_id = def.node_id AND param.full_name = def.full_name
    WHERE def.full_name IS NOT NULL
    GROUP BY def.full_name, def.start_line, def.end_line, def.def_start_line, def.def_end_line, 
             def.param_count, def.descendant_count, def.def_complexity, def.parent_type, def.function_signature
    ORDER BY start_line;
    """
    
    return run_duckdb_query(db_path, analysis_query)

def format_output(result, output_format, file_path):
    """Format the analysis result."""
    if not result:
        return "No functions found or error occurred."
    
    if output_format == 'json':
        # Parse DuckDB table output and convert to JSON
        lines = result.strip().split('\n')
        if len(lines) < 3:  # Header + separator + at least one row
            return json.dumps({"file": file_path, "functions": []}, indent=2)
        
        # This is a simplified parser - in practice you'd want more robust parsing
        return json.dumps({"file": file_path, "raw_output": result}, indent=2)
    
    elif output_format == 'summary':
        lines = result.strip().split('\n')
        if len(lines) < 3:
            return f"File: {file_path}\nNo functions found.\n"
        
        # Count rows (subtract header and separator)
        function_count = len(lines) - 3
        
        summary = f"""
File Analysis: {file_path}
{'=' * (15 + len(file_path))}

Total Functions: {function_count}

Function Details:
{result}
        """.strip()
        
        return summary
    
    else:  # default/table format
        return f"Function Analysis for: {file_path}\n{'-' * (25 + len(file_path))}\n\n{result}"

def main():
    parser = argparse.ArgumentParser(description='Analyze functions in a source code file using DuckDB AST extension')
    parser.add_argument('file_path', help='Path to the source file to analyze')
    parser.add_argument('--db', help='Path to existing DuckDB database with AST data (optional)')
    parser.add_argument('--output', choices=['table', 'summary', 'json'], default='table', 
                       help='Output format (default: table)')
    parser.add_argument('--temp-dir', help='Directory for temporary files (default: system temp)')
    
    args = parser.parse_args()
    
    # Check if DuckDB executable exists
    if not os.path.exists('./build/release/duckdb'):
        print("Error: DuckDB executable not found at ./build/release/duckdb", file=sys.stderr)
        print("Please build the project first with 'make'", file=sys.stderr)
        sys.exit(1)
    
    # Check if the extension exists
    extension_path = './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension'
    if not os.path.exists(extension_path):
        print(f"Error: DuckDB AST extension not found at {extension_path}", file=sys.stderr)
        print("Please build the extension first", file=sys.stderr)
        sys.exit(1)
    
    # Use provided database or create temporary one
    if args.db:
        db_path = args.db
        temp_db = False
    else:
        # Create temporary database
        temp_dir = args.temp_dir or tempfile.gettempdir()
        temp_db_file = tempfile.NamedTemporaryFile(suffix='.db', dir=temp_dir, delete=False)
        db_path = temp_db_file.name
        temp_db_file.close()
        # Remove the empty file so DuckDB can create a fresh database
        os.unlink(db_path)
        temp_db = True
        
        # Parse the file into the temporary database
        if not parse_file_to_db(args.file_path, db_path):
            if temp_db:
                os.unlink(db_path)
            sys.exit(1)
    
    try:
        # Analyze functions
        result = analyze_functions(db_path, args.file_path)
        
        if result is None:
            print("Error: Failed to analyze functions", file=sys.stderr)
            sys.exit(1)
        
        # Format and print output
        formatted_result = format_output(result, args.output, args.file_path)
        print(formatted_result)
        
    finally:
        # Clean up temporary database
        if temp_db and os.path.exists(db_path):
            os.unlink(db_path)

if __name__ == '__main__':
    main()