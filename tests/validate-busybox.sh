#!/bin/bash
# Validate busybox.wasm build output
# Usage: ./tests/validate-busybox.sh [path/to/busybox.wasm]

set -e

WASM="${1:-build/wasm/busybox.wasm}"
FAIL=0

fail() {
    echo "FAIL: $1"
    FAIL=1
}

pass() {
    echo "PASS: $1"
}

# Check file exists
if [[ ! -f "$WASM" ]]; then
    echo "ERROR: $WASM not found"
    exit 1
fi

echo "Validating: $WASM"
echo "============================================"

# Get full objdump
OBJDUMP=$(wasm-objdump -x "$WASM" 2>&1)

# Get exports
EXPORTS=$(echo "$OBJDUMP" | grep -E '^\s*-\s*func\[[0-9]+\].*->' || true)

# Get imports - busybox uses "yos." module
IMPORTS=$(echo "$OBJDUMP" | grep -E '<yos\.' || true)

# Test 1: fork should NOT alias to vfork
if echo "$EXPORTS" | grep -q '<vfork> -> "fork"'; then
    fail "fork is aliased to vfork (symbol corruption)"
else
    pass "fork is not aliased to vfork"
fi

# Test 2: getpid should NOT alias to setsid
if echo "$EXPORTS" | grep -q '<setsid> -> "getpid"'; then
    fail "getpid is aliased to setsid (symbol corruption)"
else
    pass "getpid is not aliased to setsid"
fi

# Test 3: Check key YOS imports are present
if echo "$IMPORTS" | grep -q 'yos.fork'; then
    pass "yos.fork import present"
else
    fail "yos.fork import missing"
fi

if echo "$IMPORTS" | grep -q 'yos.getpid'; then
    pass "yos.getpid import present"
else
    fail "yos.getpid import missing"
fi

if echo "$IMPORTS" | grep -q 'yos.execve'; then
    pass "yos.execve import present"
else
    fail "yos.execve import missing"
fi

if echo "$IMPORTS" | grep -q 'yos.waitpid\|yos.wait'; then
    pass "yos.wait/waitpid import present"
else
    fail "yos.wait/waitpid import missing"
fi

if echo "$IMPORTS" | grep -q 'yos.open'; then
    pass "yos.open import present"
else
    fail "yos.open import missing"
fi

if echo "$IMPORTS" | grep -q 'yos.read'; then
    pass "yos.read import present"
else
    fail "yos.read import missing"
fi

if echo "$IMPORTS" | grep -q 'yos.write'; then
    pass "yos.write import present"
else
    fail "yos.write import missing"
fi

# Test 4: Check main export exists
if echo "$OBJDUMP" | grep -qF '-> "main"'; then
    pass "main export present"
else
    fail "main export missing"
fi

# Test 5: __wasm_call_ctors must NOT point to __stdio_exit
CTORS_EXPORT=$(echo "$OBJDUMP" | grep -- '-> "__wasm_call_ctors"' || true)
if echo "$CTORS_EXPORT" | grep -q '<__stdio_exit>'; then
    fail "__wasm_call_ctors points to __stdio_exit (FATAL: init runs cleanup code!)"
else
    pass "__wasm_call_ctors not corrupted"
fi

# Test 6: Check file size is reasonable
SIZE=$(stat -c%s "$WASM" 2>/dev/null || stat -f%z "$WASM")
if [[ $SIZE -gt 100000 ]]; then
    pass "binary size reasonable ($SIZE bytes)"
else
    fail "binary suspiciously small ($SIZE bytes)"
fi

# Test 7: Count imports
IMPORT_COUNT=$(echo "$IMPORTS" | wc -l)
if [[ $IMPORT_COUNT -gt 50 ]]; then
    pass "sufficient imports ($IMPORT_COUNT yos.* functions)"
else
    fail "too few imports ($IMPORT_COUNT yos.* functions)"
fi

echo "============================================"
if [[ $FAIL -eq 0 ]]; then
    echo "All tests passed"
    exit 0
else
    echo "Some tests failed"
    exit 1
fi
