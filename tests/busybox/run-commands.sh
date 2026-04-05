#!/bin/bash
# Batch test runner for busybox commands
# Tracks success/fail, skips known-good unless code changed
# Debug output goes to YOS_TRACE_FILE per command

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
YOS="$ROOT_DIR/build/yos"
BUSYBOX="$ROOT_DIR/build/wasm/busybox.wasm"
TRACE_DIR="$ROOT_DIR/tmp/traces"
STATUS_FILE="$ROOT_DIR/tmp/busybox-status.txt"
CODE_HASH_FILE="$ROOT_DIR/tmp/code-hash.txt"

# Commands to test: "name|args"
COMMANDS=(
    "true|"
    "false|"
    "echo|hello"
    "pwd|"
    "id|"
    "whoami|"
    "hostname|"
    "uname|-a"
    "cat|/etc/hostname"
    "ls|/"
    "ls|-la /"
    "ls|/tmp"
    "stat|/"
    "test|-d /"
    "env|"
    "printenv|"
)

mkdir -p "$TRACE_DIR"

# Compute hash of relevant source files
compute_code_hash() {
    find "$ROOT_DIR/src" -name '*.cpp' -o -name '*.hpp' | sort | xargs cat | md5sum | cut -d' ' -f1
}

# Load previous status
declare -A PREV_STATUS
if [[ -f "$STATUS_FILE" ]]; then
    while IFS='|' read -r name status; do
        PREV_STATUS["$name"]="$status"
    done < "$STATUS_FILE"
fi

# Check if code changed
CODE_HASH=$(compute_code_hash)
PREV_HASH=""
if [[ -f "$CODE_HASH_FILE" ]]; then
    PREV_HASH=$(cat "$CODE_HASH_FILE")
fi

CODE_CHANGED=0
if [[ "$CODE_HASH" != "$PREV_HASH" ]]; then
    CODE_CHANGED=1
    echo "Code changed, re-running all tests"
    echo "$CODE_HASH" > "$CODE_HASH_FILE"
fi

# Run tests
declare -A NEW_STATUS
PASSED=0
FAILED=0
SKIPPED=0

echo "=========================================="
echo "Busybox Command Tests"
echo "=========================================="
printf "%-20s %-10s %s\n" "COMMAND" "STATUS" "TRACE"
echo "------------------------------------------"

for cmd_spec in "${COMMANDS[@]}"; do
    IFS='|' read -r name args <<< "$cmd_spec"

    # Skip if previously passed and code unchanged
    if [[ "$CODE_CHANGED" -eq 0 && "${PREV_STATUS[$name]:-}" == "PASS" ]]; then
        NEW_STATUS["$name"]="PASS"
        printf "%-20s %-10s %s\n" "$name" "SKIP" "(was PASS)"
        SKIPPED=$((SKIPPED + 1))
        continue
    fi

    TRACE_FILE="$TRACE_DIR/${name}.trace"

    # Run command with trace
    set +e
    if [[ -n "$args" ]]; then
        YOS_DEBUG=1 YOS_TRACE_FILE="$TRACE_FILE" "$YOS" "$BUSYBOX" "$name" $args >"$TRACE_DIR/${name}.stdout" 2>"$TRACE_FILE"
    else
        YOS_DEBUG=1 YOS_TRACE_FILE="$TRACE_FILE" "$YOS" "$BUSYBOX" "$name" >"$TRACE_DIR/${name}.stdout" 2>"$TRACE_FILE"
    fi
    EXIT_CODE=$?
    set -e

    # Determine status (false is expected to return 1)
    if [[ "$name" == "false" ]]; then
        if [[ $EXIT_CODE -eq 1 ]]; then
            STATUS="PASS"
        else
            STATUS="FAIL"
        fi
    else
        if [[ $EXIT_CODE -eq 0 ]]; then
            STATUS="PASS"
        else
            STATUS="FAIL"
        fi
    fi

    NEW_STATUS["$name"]="$STATUS"

    if [[ "$STATUS" == "PASS" ]]; then
        printf "%-20s \e[32m%-10s\e[0m %s\n" "$name" "PASS" ""
        PASSED=$((PASSED + 1))
    else
        printf "%-20s \e[31m%-10s\e[0m %s\n" "$name" "FAIL($EXIT_CODE)" "$TRACE_FILE"
        FAILED=$((FAILED + 1))
    fi
done

# Save new status
> "$STATUS_FILE"
for name in "${!NEW_STATUS[@]}"; do
    echo "${name}|${NEW_STATUS[$name]}" >> "$STATUS_FILE"
done

echo "------------------------------------------"
echo "PASSED: $PASSED  FAILED: $FAILED  SKIPPED: $SKIPPED"
echo "=========================================="

if [[ $FAILED -gt 0 ]]; then
    echo ""
    echo "Failed command traces in: $TRACE_DIR/"
    exit 1
fi
