-- Test the new parse_ast function
LOAD 'build/release/extension/duckdb_ast/duckdb_ast.duckdb_extension';

-- Test 1: Simple Python function
SELECT parse_ast('def hello(): pass', 'python') as python_ast;

-- Test 2: Simple JavaScript function  
SELECT parse_ast('function hello() {}', 'javascript') as js_ast;

-- Test 3: Simple C++ function
SELECT parse_ast('int main() { return 0; }', 'cpp') as cpp_ast;

-- Test 4: Simple Rust function (the one that was causing issues)
SELECT parse_ast('fn main() {}', 'rust') as rust_ast;