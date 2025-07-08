-- Test script to verify segfault fixes
.load build/release/sitting_duck.duckdb_extension

-- Test 1: peek='none' (was causing segfault)
.print "Testing peek='none' (previously caused segfault)..."
SELECT count(*) FROM read_ast('src/unified_ast_backend.hpp', peek:='none');

-- Test 2: peek=0 (was working)
.print "Testing peek=0 (baseline - should work)..."
SELECT count(*) FROM read_ast('src/unified_ast_backend.hpp', peek:=0);

-- Test 3: context='native' with single file (was causing segfault)
.print "Testing context='native' (previously caused segfault)..."
SELECT count(*) FROM read_ast('src/unified_ast_backend.cpp', context:='native');

-- Test 4: Test multiple files with peek='none' to trigger parallel path
.print "Testing parallel processing with peek='none'..."
SELECT count(*) FROM read_ast('src/*.hpp', peek:='none') LIMIT 10;

.print "All tests completed successfully - no segfaults detected!"