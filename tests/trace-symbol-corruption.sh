#!/bin/bash
# Trace symbol corruption through the busybox build process
# This test identifies WHERE and WHY fork/getpid get corrupted
#
# Usage: ./tests/trace-symbol-corruption.sh [bb-build-dir]

set -e

BB_BUILD="${1:-build/wasm/bb-build}"
WASM="${2:-build/wasm/busybox.wasm}"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "========================================================"
echo "SYMBOL CORRUPTION TRACER"
echo "========================================================"
echo ""

# Helper to check exports in a file
check_exports() {
    local file="$1"
    local name="$2"

    if [[ ! -f "$file" ]]; then
        echo -e "${YELLOW}SKIP${NC}: $name - file not found"
        return
    fi

    echo "--- $name ---"

    local dump=$(wasm-objdump -x "$file" 2>&1)

    # Check for BAD aliases (corruption)
    local bad_fork=$(echo "$dump" | grep '<vfork> -> "fork"' || true)
    local bad_getpid=$(echo "$dump" | grep '<setsid> -> "getpid"' || true)

    # Check for GOOD exports
    local good_fork=$(echo "$dump" | grep '<fork> -> "fork"' || true)
    local good_getpid=$(echo "$dump" | grep '<getpid> -> "getpid"' || true)

    if [[ -n "$bad_fork" ]]; then
        echo -e "${RED}CORRUPT${NC}: fork -> vfork (fork() will call vfork's code!)"
    elif [[ -n "$good_fork" ]]; then
        echo -e "${GREEN}OK${NC}: fork -> fork"
    else
        echo -e "${YELLOW}MISSING${NC}: no fork export"
    fi

    if [[ -n "$bad_getpid" ]]; then
        echo -e "${RED}CORRUPT${NC}: getpid -> setsid (getpid() will call setsid's code!)"
    elif [[ -n "$good_getpid" ]]; then
        echo -e "${GREEN}OK${NC}: getpid -> getpid"
    else
        echo -e "${YELLOW}MISSING${NC}: no getpid export"
    fi
    echo ""
}

echo "STEP 1: Check yos-stubs.o (our source of truth)"
echo "------------------------------------------------"
check_exports "$BB_BUILD/yos-stubs.o" "yos-stubs.o"

echo "STEP 2: Check applets/built-in.o (after -r merge)"
echo "------------------------------------------------"
check_exports "$BB_BUILD/applets/built-in.o" "applets/built-in.o"
echo "NOTE: If exports are MISSING here, the -r (relocatable) link stripped them."
echo "      This is a problem because yos-stubs.o exports get lost."
echo ""

echo "STEP 3: Check final busybox.wasm"
echo "------------------------------------------------"
check_exports "$WASM" "busybox.wasm"
echo ""

echo "STEP 4: Identify corruption source"
echo "------------------------------------------------"

# Check if libc.a has any fork/getpid related objects
LIBC="build/_deps/wasi-sdk/share/wasi-sysroot/lib/wasm32-wasip1/libc.a"
if [[ -f "$LIBC" ]]; then
    echo "Scanning libc.a for fork/getpid/vfork/setsid symbols..."
    found=0
    for obj in $(ar t "$LIBC" 2>/dev/null); do
        content=$(ar p "$LIBC" "$obj" 2>/dev/null | wasm-objdump -x - 2>&1 || true)
        if echo "$content" | grep -qE '(fork|getpid|vfork|setsid)'; then
            if [[ $found -eq 0 ]]; then
                echo -e "${YELLOW}Found in libc.a:${NC}"
                found=1
            fi
            echo "  $obj:"
            echo "$content" | grep -E '(fork|getpid|vfork|setsid)' | sed 's/^/    /'
        fi
    done
    if [[ $found -eq 0 ]]; then
        echo "  No fork/getpid symbols in libc.a"
    fi
fi
echo ""

echo "STEP 5: Check link command"
echo "------------------------------------------------"
if [[ -f "$BB_BUILD/busybox_unstripped.out" ]]; then
    echo "Final link command from busybox_unstripped.out:"
    head -5 "$BB_BUILD/busybox_unstripped.out" | sed 's/^/  /'
fi
echo ""

echo "========================================================"
echo "DIAGNOSIS"
echo "========================================================"

# Final diagnosis
dump=$(wasm-objdump -x "$WASM" 2>&1 || true)
bad_fork=$(echo "$dump" | grep '<vfork> -> "fork"' || true)
bad_getpid=$(echo "$dump" | grep '<setsid> -> "getpid"' || true)

if [[ -n "$bad_fork" || -n "$bad_getpid" ]]; then
    echo -e "${RED}SYMBOL CORRUPTION DETECTED${NC}"
    echo ""
    echo "Problem: The final binary has wrong export mappings."
    echo "  - 'fork' points to vfork's implementation"
    echo "  - 'getpid' points to setsid's implementation"
    echo ""
    echo "When busybox calls fork(), it actually runs vfork() code."
    echo "When busybox calls getpid(), it actually runs setsid() code."
    echo "This causes crashes because the wrong function executes."
    echo ""
    echo "Root cause: wasm-ld with --allow-multiple-definition"
    echo "keeps duplicate export entries and the WRONG one wins."
    echo ""
    echo "The corruption likely comes from:"
    echo "  1. wasi-libc weak aliases in libc.a, OR"
    echo "  2. Duplicate linking of yos-stubs.o (once via -r merge, once direct)"
    echo ""
    exit 1
else
    echo -e "${GREEN}NO CORRUPTION DETECTED${NC}"
    exit 0
fi
