#!/usr/bin/env python3
"""
AST Parquet Index Manager

A Python utility for managing parquet-based AST indexes with the DuckDB AST extension.
Provides programmatic access to index creation, updates, and queries.
"""

import duckdb
import os
import sys
import argparse
import json
from pathlib import Path
from typing import List, Dict, Optional, Tuple
import time

class ASTIndexManager:
    """Manages parquet-based AST indexes for code analysis."""
    
    def __init__(self, duckdb_path: str = "./build/release/duckdb"):
        """Initialize the index manager with DuckDB connection."""
        self.conn = duckdb.connect()
        
        # Load the extension - skip for now due to platform mismatch
        # self.conn.execute("LOAD './build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension'")
        
        # Load SQL macros
        for sql_file in ["ast-navigator.sql", "ast-nav-parquet.sql"]:
            if os.path.exists(sql_file):
                with open(sql_file, 'r') as f:
                    self.conn.execute(f.read())
    
    def create_index(self, file_pattern: str, file_type: Optional[str] = None, 
                    index_path: Optional[str] = None) -> Dict[str, any]:
        """Create a parquet index for the given file pattern."""
        if not file_type:
            # Extract from pattern
            import re
            match = re.search(r'\.([^.]+)$', file_pattern)
            file_type = match.group(1) if match else 'unknown'
        
        if not index_path:
            index_path = f".index-{file_type}.parquet"
        
        print(f"Creating index: {index_path}")
        start_time = time.time()
        
        # Create the index with optimal compression
        self.conn.execute(f"""
            COPY (
                SELECT * FROM read_ast('{file_pattern}', peek_mode := 'none')
            ) TO '{index_path}' (FORMAT PARQUET, CODEC 'ZSTD', COMPRESSION_LEVEL 22)
        """)
        
        elapsed = time.time() - start_time
        
        # Get statistics
        stats = self.get_index_stats(index_path)
        stats['creation_time'] = elapsed
        
        return stats
    
    def get_index_stats(self, index_path: str) -> Dict[str, any]:
        """Get statistics for an index file."""
        result = self.conn.execute(f"""
            SELECT 
                COUNT(DISTINCT file_path) as indexed_files,
                COUNT(*) as total_nodes,
                COUNT(*) FILTER (WHERE type = 'function_declarator' AND semantic_type = 112) as total_functions,
                COUNT(*) FILTER (WHERE type IN ('class_definition', 'class_declaration')) as total_classes
            FROM read_parquet('{index_path}')
        """).fetchone()
        
        file_size = os.path.getsize(index_path) if os.path.exists(index_path) else 0
        
        return {
            'index_path': index_path,
            'indexed_files': result[0],
            'total_nodes': result[1],
            'total_functions': result[2],
            'total_classes': result[3],
            'file_size_mb': round(file_size / 1024 / 1024, 2)
        }
    
    def update_index(self, file_pattern: str, index_path: str) -> Dict[str, any]:
        """Update an existing index with new or modified files."""
        temp_path = f"{index_path}.tmp"
        
        # Get list of new files
        new_files = self.conn.execute(f"""
            WITH existing AS (
                SELECT DISTINCT file_path FROM read_parquet('{index_path}')
            )
            SELECT COUNT(*) as new_file_count
            FROM (
                SELECT DISTINCT file_path 
                FROM read_ast('{file_pattern}', peek_mode := 'none')
                WHERE file_path NOT IN (SELECT file_path FROM existing)
            )
        """).fetchone()[0]
        
        if new_files == 0:
            return {'status': 'no_updates', 'new_files': 0}
        
        print(f"Updating index with {new_files} new files...")
        
        # Create merged index
        self.conn.execute(f"""
            COPY (
                SELECT * FROM read_parquet('{index_path}')
                WHERE file_path NOT IN (
                    SELECT DISTINCT file_path 
                    FROM read_ast('{file_pattern}', peek_mode := 'none')
                )
                UNION ALL
                SELECT * FROM read_ast('{file_pattern}', peek_mode := 'none')
            ) TO '{temp_path}' (FORMAT PARQUET, CODEC 'ZSTD', COMPRESSION_LEVEL 22)
        """)
        
        # Replace old index
        os.rename(temp_path, index_path)
        
        return {
            'status': 'updated',
            'new_files': new_files,
            'index_path': index_path
        }
    
    def get_file_functions(self, file_path: str, index_path: str) -> List[Dict]:
        """Get all functions in a specific file."""
        results = self.conn.execute(f"""
            SELECT 
                function_name,
                type,
                start_line,
                end_line,
                line_count,
                complexity
            FROM ast_file_functions('{file_path}', '{index_path}')
            ORDER BY start_line
        """).fetchall()
        
        return [
            {
                'name': r[0],
                'type': r[1],
                'start_line': r[2],
                'end_line': r[3],
                'line_count': r[4],
                'complexity': r[5]
            }
            for r in results
        ]
    
    def find_function(self, function_name: str, index_path: str = '.index-*.parquet',
                     file_pattern: str = '%') -> List[Dict]:
        """Find a function with detailed analysis."""
        results = self.conn.execute(f"""
            SELECT 
                file_path,
                function_name,
                definition_type,
                start_line,
                end_line,
                line_count,
                complexity,
                param_count,
                local_vars,
                calls_made,
                complexity_per_line
            FROM ast_find_function_detail('{function_name}', '{index_path}', '{file_pattern}')
        """).fetchall()
        
        return [
            {
                'file_path': r[0],
                'function_name': r[1],
                'definition_type': r[2],
                'location': {'start': r[3], 'end': r[4]},
                'metrics': {
                    'line_count': r[5],
                    'complexity': r[6],
                    'param_count': r[7],
                    'local_vars': r[8],
                    'calls_made': r[9],
                    'complexity_per_line': r[10]
                }
            }
            for r in results
        ]
    
    def get_function_source_info(self, function_name: str, 
                                file_path: Optional[str] = None,
                                index_path: Optional[str] = None) -> Optional[Dict]:
        """Get source location info for a function."""
        file_param = f"'{file_path}'" if file_path else "NULL"
        index_param = f"'{index_path}'" if index_path else "NULL"
        
        result = self.conn.execute(f"""
            SELECT 
                file_path,
                function_name,
                start_line,
                end_line
            FROM ast_get_function_source('{function_name}', {file_param}, {index_param})
            LIMIT 1
        """).fetchone()
        
        if result:
            return {
                'file_path': result[0],
                'function_name': result[1],
                'start_line': result[2],
                'end_line': result[3],
                'extract_command': f"sed -n '{result[2]},{result[3]}p' {result[0]}"
            }
        return None
    
    def find_complex_functions(self, index_path: str = '.index-*.parquet',
                              min_complexity: int = 100) -> List[Dict]:
        """Find functions exceeding complexity threshold."""
        results = self.conn.execute(f"""
            SELECT 
                file_path,
                function_name,
                start_line,
                end_line,
                complexity
            FROM ast_index_complex_functions('{index_path}', {min_complexity})
            ORDER BY complexity DESC
        """).fetchall()
        
        return [
            {
                'file_path': r[0],
                'function_name': r[1],
                'start_line': r[2],
                'end_line': r[3],
                'complexity': r[4]
            }
            for r in results
        ]
    
    def search(self, search_term: str, search_type: str = 'function',
               index_pattern: str = '.index-*.parquet') -> List[Dict]:
        """Quick search across indexes."""
        results = self.conn.execute(f"""
            SELECT 
                file_path,
                name,
                type,
                start_line,
                end_line,
                language
            FROM ast_quick_find('{search_term}', '{search_type}')
            ORDER BY file_path, start_line
        """).fetchall()
        
        return [
            {
                'file_path': r[0],
                'name': r[1],
                'type': r[2],
                'start_line': r[3],
                'end_line': r[4],
                'language': r[5]
            }
            for r in results
        ]
    
    def list_indexes(self) -> List[Dict]:
        """List all available index files."""
        results = self.conn.execute("""
            SELECT 
                index_path,
                file_type,
                file_size_mb,
                last_modified
            FROM ast_list_indexes()
            ORDER BY file_type
        """).fetchall()
        
        return [
            {
                'index_path': r[0],
                'file_type': r[1],
                'file_size_mb': r[2],
                'last_modified': r[3]
            }
            for r in results
        ]


def main():
    """Command-line interface for the index manager."""
    parser = argparse.ArgumentParser(description='AST Parquet Index Manager')
    subparsers = parser.add_subparsers(dest='command', help='Available commands')
    
    # Create index
    create_parser = subparsers.add_parser('create', help='Create a new index')
    create_parser.add_argument('pattern', help='File pattern to index')
    create_parser.add_argument('--type', help='File type (auto-detected if not specified)')
    create_parser.add_argument('--output', help='Output path for index')
    
    # Update index
    update_parser = subparsers.add_parser('update', help='Update existing index')
    update_parser.add_argument('pattern', help='File pattern to index')
    update_parser.add_argument('index', help='Index file to update')
    
    # List indexes
    list_parser = subparsers.add_parser('list', help='List all indexes')
    
    # Get stats
    stats_parser = subparsers.add_parser('stats', help='Get index statistics')
    stats_parser.add_argument('index', help='Index file path')
    
    # Search
    search_parser = subparsers.add_parser('search', help='Search across indexes')
    search_parser.add_argument('term', help='Search term')
    search_parser.add_argument('--type', default='function', 
                              choices=['function', 'class', 'variable'],
                              help='Type to search for')
    
    # Find function
    func_parser = subparsers.add_parser('function', help='Find function details')
    func_parser.add_argument('name', help='Function name')
    func_parser.add_argument('--index', help='Index file to search')
    func_parser.add_argument('--file-pattern', default='%', help='File pattern filter')
    
    # Complex functions
    complex_parser = subparsers.add_parser('complex', help='Find complex functions')
    complex_parser.add_argument('--min-complexity', type=int, default=100,
                               help='Minimum complexity threshold')
    complex_parser.add_argument('--index', default='.index-*.parquet',
                               help='Index pattern')
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return
    
    # Initialize manager
    manager = ASTIndexManager()
    
    # Execute command
    if args.command == 'create':
        stats = manager.create_index(args.pattern, args.type, args.output)
        print(json.dumps(stats, indent=2))
    
    elif args.command == 'update':
        result = manager.update_index(args.pattern, args.index)
        print(json.dumps(result, indent=2))
    
    elif args.command == 'list':
        indexes = manager.list_indexes()
        for idx in indexes:
            print(f"{idx['file_type']:10} {idx['file_size_mb']:8.2f}MB  {idx['index_path']}")
    
    elif args.command == 'stats':
        stats = manager.get_index_stats(args.index)
        print(json.dumps(stats, indent=2))
    
    elif args.command == 'search':
        results = manager.search(args.term, args.type)
        for r in results:
            print(f"{r['file_path']}:{r['start_line']} {r['name']} ({r['type']})")
    
    elif args.command == 'function':
        results = manager.find_function(args.name, args.index or '.index-*.parquet', 
                                      args.file_pattern)
        for r in results:
            print(f"\nFunction: {r['function_name']}")
            print(f"File: {r['file_path']}:{r['location']['start']}-{r['location']['end']}")
            print(f"Type: {r['definition_type']}")
            print("Metrics:")
            for k, v in r['metrics'].items():
                print(f"  {k}: {v}")
    
    elif args.command == 'complex':
        results = manager.find_complex_functions(args.index, args.min_complexity)
        for r in results:
            print(f"{r['complexity']:5d} {r['file_path']}:{r['start_line']} {r['function_name']}")


if __name__ == '__main__':
    main()