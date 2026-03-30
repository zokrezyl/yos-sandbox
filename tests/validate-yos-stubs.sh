#!/bin/bash
# Validate yos-stubs.o has correct symbols before final link
# Usage: ./tests/validate-yos-stubs.sh [path/to/yos-stubs.o]

set -e

OBJ="${1:-build/wasm/bb-build/yos-stubs.o}"
FAIL=0

fail() {
    echo "FAIL: $1"
    FAIL=1
}

pass() {
    echo "PASS: $1"
}

if [[ ! -f "$OBJ" ]]; then
    echo "ERROR: $OBJ not found"
    exit 1
fi

echo "Validating: $OBJ"
echo "============================================"

DUMP=$(wasm-objdump -x "$OBJ" 2>&1)

# Test 1: fork function exists and exports as "fork"
if echo "$DUMP" | grep -q '<fork> -> "fork"'; then
    pass "fork exports as fork"
else
    fail "fork does not export as fork"
fi

# Test 2: getpid function exists and exports as "getpid"
if echo "$DUMP" | grep -q '<getpid> -> "getpid"'; then
    pass "getpid exports as getpid"
else
    fail "getpid does not export as getpid"
fi

# Test 3: vfork function exists and exports as "vfork"
if echo "$DUMP" | grep -q '<vfork> -> "vfork"'; then
    pass "vfork exports as vfork"
else
    fail "vfork does not export as vfork"
fi

# Test 4: setsid function exists and exports as "setsid"
if echo "$DUMP" | grep -q '<setsid> -> "setsid"'; then
    pass "setsid exports as setsid"
else
    fail "setsid does not export as setsid"
fi

# Test 5: fork imports yos_fork
if echo "$DUMP" | grep -q 'yos_fork.*env.yos_fork'; then
    pass "yos_fork import present"
else
    fail "yos_fork import missing"
fi

# Test 6: getpid imports yos_getpid
if echo "$DUMP" | grep -q 'yos_getpid.*env.yos_getpid'; then
    pass "yos_getpid import present"
else
    fail "yos_getpid import missing"
fi

# Test 7: NO alias corruption in object file
if echo "$DUMP" | grep -q '<vfork> -> "fork"'; then
    fail "fork aliased to vfork in object file"
else
    pass "no fork->vfork alias in object file"
fi

if echo "$DUMP" | grep -q '<setsid> -> "getpid"'; then
    fail "getpid aliased to setsid in object file"
else
    pass "no getpid->setsid alias in object file"
fi

echo "============================================"
if [[ $FAIL -eq 0 ]]; then
    echo "All tests passed"
    exit 0
else
    echo "Some tests failed"
    exit 1
fi
