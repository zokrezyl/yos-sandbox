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

# Get exports using wasm-objdump
EXPORTS=$(wasm-objdump -x "$WASM" 2>&1 | grep -E '^\s*-\s*func\[[0-9]+\].*->' || true)

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

# Test 3: Check YOS imports are present
IMPORTS=$(wasm-objdump -x "$WASM" 2>&1 | grep -E '<env\.yos_' || true)

if echo "$IMPORTS" | grep -q 'env.yos_fork'; then
    pass "yos_fork import present"
else
    fail "yos_fork import missing"
fi

if echo "$IMPORTS" | grep -q 'env.yos_getpid'; then
    pass "yos_getpid import present"
else
    fail "yos_getpid import missing"
fi

if echo "$IMPORTS" | grep -q 'env.yos_exec'; then
    pass "yos_exec import present"
else
    fail "yos_exec import missing"
fi

if echo "$IMPORTS" | grep -q 'env.yos_wait'; then
    pass "yos_wait import present"
else
    fail "yos_wait import missing"
fi

# Test 4: Check fork export exists and points to correct function
FORK_EXPORT=$(wasm-objdump -x "$WASM" 2>&1 | grep -- '-> "fork"' || true)
if [[ -n "$FORK_EXPORT" ]]; then
    # Extract function name that exports "fork"
    FORK_FUNC=$(echo "$FORK_EXPORT" | sed -n 's/.*<\([^>]*\)> -> "fork".*/\1/p')
    if [[ "$FORK_FUNC" == "fork" ]]; then
        pass "fork export points to fork function"
    else
        fail "fork export points to '$FORK_FUNC' instead of 'fork'"
    fi
else
    fail "fork export not found"
fi

# Test 5: Check getpid export exists and points to correct function
GETPID_EXPORT=$(wasm-objdump -x "$WASM" 2>&1 | grep -- '-> "getpid"' || true)
if [[ -n "$GETPID_EXPORT" ]]; then
    GETPID_FUNC=$(echo "$GETPID_EXPORT" | sed -n 's/.*<\([^>]*\)> -> "getpid".*/\1/p')
    if [[ "$GETPID_FUNC" == "getpid" ]]; then
        pass "getpid export points to getpid function"
    else
        fail "getpid export points to '$GETPID_FUNC' instead of 'getpid'"
    fi
else
    fail "getpid export not found"
fi

# Test 6: Sanity check - _start export exists
if wasm-objdump -x "$WASM" 2>&1 | grep -q -- '-> "_start"'; then
    pass "_start export present"
else
    fail "_start export missing"
fi

# Test 7: __wasm_call_ctors must NOT point to __stdio_exit
CTORS_EXPORT=$(wasm-objdump -x "$WASM" 2>&1 | grep -- '-> "__wasm_call_ctors"' || true)
if echo "$CTORS_EXPORT" | grep -q '<__stdio_exit>'; then
    fail "__wasm_call_ctors points to __stdio_exit (FATAL: init runs cleanup code!)"
else
    pass "__wasm_call_ctors not corrupted"
fi

echo "============================================"
if [[ $FAIL -eq 0 ]]; then
    echo "All tests passed"
    exit 0
else
    echo "Some tests failed"
    exit 1
fi
