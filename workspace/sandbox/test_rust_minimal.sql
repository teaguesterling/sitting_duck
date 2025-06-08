-- Test Rust parser with minimal code
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test 1: Very simple Rust function
SELECT * FROM read_ast('fn main() {}', 'rust');

-- Test 2: With print statement
SELECT * FROM read_ast('fn main() { println!("Hello"); }', 'rust');

-- Test 3: Simple struct
SELECT * FROM read_ast('struct User { name: String }', 'rust');