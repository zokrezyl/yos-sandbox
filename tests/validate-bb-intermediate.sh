#!/bin/bash
# Check busybox intermediate objects for alias corruption
# Usage: ./tests/validate-bb-intermediate.sh [path/to/bb-build]

set -e

BB_BUILD="${1:-build/wasm/bb-build}"
FAIL=0

fail() {
    echo "FAIL: $1"
    FAIL=1
}

pass() {
    echo "PASS: $1"
}

if [[ ! -d "$BB_BUILD" ]]; then
    echo "ERROR: $BB_BUILD not found"
    exit 1
fi

echo "Checking busybox intermediate objects"
echo "Build dir: $BB_BUILD"
echo "============================================"

# Check applets/built-in.o (where yos-stubs.o gets merged)
if [[ -f "$BB_BUILD/applets/built-in.o" ]]; then
    echo "Checking applets/built-in.o..."
    DUMP=$(wasm-objdump -x "$BB_BUILD/applets/built-in.o" 2>&1)

    if echo "$DUMP" | grep -q '<vfork> -> "fork"'; then
        fail "applets/built-in.o: fork aliased to vfork"
    else
        pass "applets/built-in.o: no fork->vfork alias"
    fi

    if echo "$DUMP" | grep -q '<setsid> -> "getpid"'; then
        fail "applets/built-in.o: getpid aliased to setsid"
    else
        pass "applets/built-in.o: no getpid->setsid alias"
    fi

    # Check correct exports exist
    if echo "$DUMP" | grep -q '<fork> -> "fork"'; then
        pass "applets/built-in.o: fork exports correctly"
    else
        fail "applets/built-in.o: fork export missing or wrong"
    fi

    if echo "$DUMP" | grep -q '<getpid> -> "getpid"'; then
        pass "applets/built-in.o: getpid exports correctly"
    else
        fail "applets/built-in.o: getpid export missing or wrong"
    fi
else
    echo "SKIP: applets/built-in.o not found"
fi

# Check libbb/lib.a for any process stubs
if [[ -f "$BB_BUILD/libbb/lib.a" ]]; then
    echo "Checking libbb/lib.a..."
    for obj in $(ar t "$BB_BUILD/libbb/lib.a" 2>/dev/null); do
        content=$(ar p "$BB_BUILD/libbb/lib.a" "$obj" 2>/dev/null | wasm-objdump -x - 2>&1 || true)
        if echo "$content" | grep -qE '<(vfork|setsid)> -> "(fork|getpid)"'; then
            fail "libbb/lib.a:$obj has corrupted aliases"
            echo "$content" | grep -E '<(vfork|setsid)> -> "(fork|getpid)"'
        fi
    done
    pass "libbb/lib.a: no corrupted aliases"
fi

# Check shell/lib.a
if [[ -f "$BB_BUILD/shell/lib.a" ]]; then
    echo "Checking shell/lib.a..."
    for obj in $(ar t "$BB_BUILD/shell/lib.a" 2>/dev/null); do
        content=$(ar p "$BB_BUILD/shell/lib.a" "$obj" 2>/dev/null | wasm-objdump -x - 2>&1 || true)
        if echo "$content" | grep -qE '<(vfork|setsid)> -> "(fork|getpid)"'; then
            fail "shell/lib.a:$obj has corrupted aliases"
            echo "$content" | grep -E '<(vfork|setsid)> -> "(fork|getpid)"'
        fi
    done
    pass "shell/lib.a: no corrupted aliases"
fi

echo "============================================"
if [[ $FAIL -eq 0 ]]; then
    echo "All intermediate objects OK"
    exit 0
else
    echo "Some intermediate objects have corrupted aliases"
    exit 1
fi
