#!/usr/bin/env python3
"""
Simple Python API for AST parquet indexes
"""
import duckdb
import os
from typing import List, Dict, Optional, Tuple

class AST:
    def __init__(self):
        self.conn = duckdb.connect()
        
    def index(self, lang: str, *patterns: str) -> None:
        """Create a parquet index for files matching pattern(s)."""
        if not patterns:
            raise ValueError("At least one pattern is required")
            
        index_path = f".index-{lang}.parquet"
        print(f"Creating {index_path} with {len(patterns)} pattern(s)...")
        
        if len(patterns) == 1:
            # Single pattern
            query = f"SELECT * FROM read_ast('{patterns[0]}', peek_mode := 'none')"
        else:
            # Multiple patterns with UNION ALL
            queries = [f"SELECT * FROM read_ast('{pattern}', peek_mode := 'none')" for pattern in patterns]
            query = " UNION ALL ".join(queries)
        
        self.conn.execute(f"""
            COPY ({query}) TO '{index_path}' (FORMAT PARQUET, CODEC 'ZSTD', COMPRESSION_LEVEL 22)
        """)
        print(f"Created {index_path}")
    
    def funcs(self, file_path: str) -> List[Dict]:
        """List all functions in a file."""
        # Detect language from extension
        lang = os.path.splitext(file_path)[1].lstrip('.')
        index = f".index-{lang}.parquet"
        
        if not os.path.exists(index):
            raise FileNotFoundError(f"No index found: {index}. Create with ast.index('{lang}', pattern)")
        
        results = self.conn.execute(f"""
            WITH f AS (
                SELECT node_id, parent_id, start_line, end_line, descendant_count
                FROM read_parquet('{index}')
                WHERE file_path = '{file_path}' AND type = 'function_declarator' AND semantic_type = 112
            ),
            n AS (
                SELECT f.*, MAX(CASE WHEN c.type IN ('identifier','qualified_identifier') THEN c.name END) as name
                FROM f
                JOIN read_parquet('{index}') c ON c.parent_id = f.node_id AND c.file_path = '{file_path}'
                GROUP BY f.node_id, f.parent_id, f.start_line, f.end_line, f.descendant_count
            )
            SELECT name, start_line, end_line, descendant_count
            FROM n WHERE name IS NOT NULL ORDER BY start_line
        """).fetchall()
        
        return [
            {'name': r[0], 'start': r[1], 'end': r[2], 'complexity': r[3]}
            for r in results
        ]
    
    def find(self, function_name: str, lang: Optional[str] = None) -> List[Dict]:
        """Find a function across all indexes or specific language."""
        index_pattern = f".index-{lang}.parquet" if lang else ".index-*.parquet"
        results = self.conn.execute(f"""
            WITH m AS (
                SELECT DISTINCT i.file_path, i.start_line, i.end_line, i.descendant_count
                FROM read_parquet('{index_pattern}') i
                WHERE i.type IN ('function_declarator','function_definition') 
                  AND (i.semantic_type = 112 OR i.semantic_type IS NULL)
                  AND EXISTS (
                      SELECT 1 FROM read_parquet('{index_pattern}') n
                      WHERE n.parent_id = i.node_id AND n.file_path = i.file_path
                        AND n.type IN ('identifier','qualified_identifier') AND n.name LIKE '%{function_name}%'
                  )
            )
            SELECT file_path, start_line, end_line, descendant_count
            FROM m ORDER BY file_path
        """).fetchall()
        
        return [
            {'file': r[0], 'start': r[1], 'end': r[2], 'complexity': r[3]}
            for r in results
        ]
    
    def src(self, function_name: str) -> Optional[str]:
        """Get source code for a function."""
        result = self.conn.execute(f"""
            WITH m AS (
                SELECT i.file_path, COALESCE(p.start_line, i.start_line) as start_line, 
                       COALESCE(p.end_line, i.end_line) as end_line
                FROM read_parquet('.index-*.parquet') i
                LEFT JOIN read_parquet('.index-*.parquet') p ON p.node_id = i.parent_id AND p.file_path = i.file_path
                WHERE i.type = 'function_declarator' AND i.semantic_type = 112
                  AND EXISTS (
                      SELECT 1 FROM read_parquet('.index-*.parquet') n
                      WHERE n.parent_id = i.node_id AND n.file_path = i.file_path
                        AND n.name LIKE '%{function_name}%'
                  )
                LIMIT 1
            )
            SELECT file_path, start_line, end_line FROM m
        """).fetchone()
        
        if result:
            file_path, start, end = result
            with open(file_path, 'r') as f:
                lines = f.readlines()
                return ''.join(lines[start-1:end])
        return None
    
    def search(self, term: str, lang: Optional[str] = None) -> List[Dict]:
        """Search for names containing term."""
        index_pattern = f".index-{lang}.parquet" if lang else ".index-*.parquet"
        results = self.conn.execute(f"""
            SELECT file_path, name, type, start_line
            FROM read_parquet('{index_pattern}')
            WHERE name LIKE '%{term}%' 
              AND type IN ('function_declarator','function_definition','class_definition','struct_declaration','variable_declaration')
            ORDER BY file_path, start_line
            LIMIT 100
        """).fetchall()
        
        return [
            {'file': r[0], 'name': r[1], 'type': r[2], 'line': r[3]}
            for r in results
        ]
    
    def classes(self, term: str, lang: Optional[str] = None) -> List[Dict]:
        """Find classes/structs containing term."""
        index_pattern = f".index-{lang}.parquet" if lang else ".index-*.parquet"
        results = self.conn.execute(f"""
            SELECT file_path, name, start_line, end_line, descendant_count
            FROM read_parquet('{index_pattern}')
            WHERE type IN ('class_definition', 'class_declaration', 'struct_declaration', 'interface_declaration')
              AND name LIKE '%{term}%'
            ORDER BY file_path, start_line
            LIMIT 100
        """).fetchall()
        
        return [
            {'file': r[0], 'name': r[1], 'start': r[2], 'end': r[3], 'complexity': r[4]}
            for r in results
        ]
    
    def complex(self, threshold: int = 100) -> List[Dict]:
        """Find complex functions exceeding threshold."""
        results = self.conn.execute(f"""
            WITH f AS (
                SELECT file_path, node_id, parent_id, descendant_count
                FROM read_parquet('.index-*.parquet')
                WHERE type = 'function_declarator' AND semantic_type = 112 AND descendant_count >= {threshold}
            )
            SELECT f.file_path, 
                   MAX(CASE WHEN n.type IN ('identifier','qualified_identifier') THEN n.name END) as name,
                   f.descendant_count
            FROM f
            JOIN read_parquet('.index-*.parquet') n ON n.parent_id = f.node_id AND n.file_path = f.file_path
            GROUP BY f.file_path, f.descendant_count
            ORDER BY f.descendant_count DESC
        """).fetchall()
        
        return [
            {'file': r[0], 'name': r[1], 'complexity': r[2]}
            for r in results
        ]
    
    def stats(self) -> List[Dict]:
        """Get statistics for all indexes."""
        import glob
        
        stats = []
        for index_path in glob.glob('.index-*.parquet'):
            lang = index_path.replace('.index-', '').replace('.parquet', '')
            info = self.conn.execute(f"""
                SELECT 
                    COUNT(DISTINCT file_path) as files,
                    COUNT(*) as nodes,
                    COUNT(*) FILTER (WHERE type IN ('function_declarator','function_definition') 
                                      AND (semantic_type = 112 OR semantic_type IS NULL)) as functions
                FROM read_parquet('{index_path}')
            """).fetchone()
            
            stats.append({
                'lang': lang,
                'files': info[0],
                'nodes': info[1],
                'functions': info[2],
                'size_mb': round(os.path.getsize(index_path) / 1024 / 1024, 2)
            })
        
        return sorted(stats, key=lambda x: x['nodes'], reverse=True)


# Simple CLI
if __name__ == '__main__':
    import sys
    
    ast = AST()
    
    if len(sys.argv) < 2:
        print("Usage: ast.py <command> [args...]")
        print("Commands: index, funcs, find, src, search, complex, stats")
        sys.exit(1)
    
    cmd = sys.argv[1]
    
    try:
        if cmd == 'index' and len(sys.argv) >= 4:
            ast.index(sys.argv[2], *sys.argv[3:])
        
        elif cmd == 'funcs' and len(sys.argv) >= 3:
            for func in ast.funcs(sys.argv[2]):
                print(f"{func['name']:40} {func['start']:>6}-{func['end']:<6} complexity: {func['complexity']}")
        
        elif cmd == 'find' and len(sys.argv) >= 3:
            for loc in ast.find(sys.argv[2]):
                print(f"{loc['file']}:{loc['start']}-{loc['end']} (complexity: {loc['complexity']})")
        
        elif cmd == 'src' and len(sys.argv) >= 3:
            code = ast.src(sys.argv[2])
            if code:
                print(code)
            else:
                print(f"Function '{sys.argv[2]}' not found")
        
        elif cmd == 'search' and len(sys.argv) >= 3:
            for item in ast.search(sys.argv[2]):
                print(f"{item['file']}:{item['line']} {item['name']} ({item['type']})")
        
        elif cmd == 'complex':
            threshold = int(sys.argv[2]) if len(sys.argv) >= 3 else 100
            for func in ast.complex(threshold):
                print(f"{func['complexity']:>6} {func['file']} {func['name']}")
        
        elif cmd == 'stats':
            for stat in ast.stats():
                print(f"{stat['lang']:>10}: {stat['files']:>4} files, {stat['nodes']:>8} nodes, {stat['functions']:>5} functions ({stat['size_mb']}MB)")
        
        else:
            print(f"Unknown command or missing arguments: {cmd}")
            
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)