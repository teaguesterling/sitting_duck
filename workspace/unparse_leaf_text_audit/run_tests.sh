#!/usr/bin/env bash
# Run each sqllogictest file in its OWN unittest invocation and check its exit
# code individually.
#
# This is not stylistic. `build/release/test/unittest a.test b.test`
# concatenates its arguments into one nonexistent filter, runs nothing, and
# exits 0 -- a silent false green. One file per invocation is the only safe
# way to get a real per-file result.
set -u

BIN=build/release/test/unittest
pass=0
fail=0
failed_files=()

for t in "$@"; do
    if [ ! -f "$t" ]; then
        echo "MISSING $t"
        fail=$((fail + 1))
        failed_files+=("$t (missing)")
        continue
    fi
    out=$("$BIN" "$t" 2>&1)
    rc=$?
    assertions=$(printf '%s\n' "$out" | grep -oE '[0-9]+ assertion' | head -1)
    if [ $rc -eq 0 ]; then
        echo "PASS  rc=0 ${assertions:-?} $t"
        pass=$((pass + 1))
    else
        echo "FAIL  rc=$rc $t"
        printf '%s\n' "$out" | tail -15 | sed 's/^/      | /'
        fail=$((fail + 1))
        failed_files+=("$t")
    fi
done

echo "-----------------------------------------------"
echo "passed: $pass   failed: $fail"
if [ $fail -gt 0 ]; then
    echo "failing files:"
    for f in "${failed_files[@]}"; do echo "  - $f"; done
    exit 1
fi
exit 0
