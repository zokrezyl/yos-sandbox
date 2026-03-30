#!/bin/bash
# Runtime tests for busybox.wasm
# Usage: ./tests/run-busybox.sh [path/to/yos] [path/to/busybox.wasm]

set -e

YOS="${1:-build/yos}"
WASM="${2:-build/wasm/busybox.wasm}"
FAIL=0

fail() {
    echo "FAIL: $1"
    FAIL=1
}

pass() {
    echo "PASS: $1"
}

if [[ ! -x "$YOS" ]]; then
    echo "ERROR: $YOS not found or not executable"
    exit 1
fi

if [[ ! -f "$WASM" ]]; then
    echo "ERROR: $WASM not found"
    exit 1
fi

echo "Runtime tests: $YOS $WASM"
echo "============================================"

# Test 1: busybox --help should not crash
OUTPUT=$(timeout 5 "$YOS" "$WASM" --help 2>&1 || true)
if echo "$OUTPUT" | grep -q "unreachable"; then
    fail "busybox --help crashed with unreachable"
    echo "  Output: $OUTPUT"
elif echo "$OUTPUT" | grep -qi "busybox\|usage\|applet"; then
    pass "busybox --help runs without crash"
else
    fail "busybox --help produced unexpected output"
    echo "  Output: $OUTPUT"
fi

# Test 2: busybox echo test
OUTPUT=$(timeout 5 "$YOS" "$WASM" echo hello 2>&1 || true)
if echo "$OUTPUT" | grep -q "unreachable"; then
    fail "busybox echo crashed"
elif echo "$OUTPUT" | grep -q "hello"; then
    pass "busybox echo works"
else
    fail "busybox echo produced unexpected output: $OUTPUT"
fi

# Test 3: busybox true (should exit 0)
if timeout 5 "$YOS" "$WASM" true 2>&1; then
    pass "busybox true exits successfully"
else
    fail "busybox true failed"
fi

# Test 4: busybox false (should exit non-zero)
if timeout 5 "$YOS" "$WASM" false 2>&1; then
    fail "busybox false should exit non-zero"
else
    pass "busybox false exits with error (expected)"
fi

echo "============================================"
if [[ $FAIL -eq 0 ]]; then
    echo "All runtime tests passed"
    exit 0
else
    echo "Some runtime tests failed"
    exit 1
fi
