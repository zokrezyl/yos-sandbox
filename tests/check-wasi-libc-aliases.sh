#!/bin/bash
# Check wasi-libc for conflicting symbol aliases
# Usage: ./tests/check-wasi-libc-aliases.sh [path/to/wasi-sysroot]

set -e

SYSROOT="${1:-build/_deps/wasi-sdk/share/wasi-sysroot}"
TARGET="wasm32-wasip1"
LIBDIR="$SYSROOT/lib/$TARGET"

if [[ ! -d "$LIBDIR" ]]; then
    echo "ERROR: $LIBDIR not found"
    exit 1
fi

echo "Checking wasi-libc for alias corruption sources"
echo "Sysroot: $SYSROOT"
echo "============================================"

# Check each library/object for problematic aliases
check_file() {
    local f="$1"
    local name=$(basename "$f")
    local aliases=$(wasm-objdump -x "$f" 2>&1 | grep -E '<(vfork|setsid)> -> "(fork|getpid)"' || true)
    if [[ -n "$aliases" ]]; then
        echo "FOUND ALIAS in $name:"
        echo "$aliases"
        return 1
    fi
    return 0
}

FOUND=0

# Check CRT files
for f in "$LIBDIR"/crt*.o; do
    [[ -f "$f" ]] || continue
    if ! check_file "$f"; then
        FOUND=1
    fi
done

# Check libc.a members
echo "Checking libc.a..."
for obj in $(ar t "$LIBDIR/libc.a" 2>/dev/null); do
    content=$(ar p "$LIBDIR/libc.a" "$obj" 2>/dev/null | wasm-objdump -x - 2>&1 || true)
    aliases=$(echo "$content" | grep -E '<(vfork|setsid)> -> "(fork|getpid)"' || true)
    if [[ -n "$aliases" ]]; then
        echo "FOUND ALIAS in libc.a:$obj:"
        echo "$aliases"
        FOUND=1
    fi
done

# Check wasi-emulated-* libraries
for lib in "$LIBDIR"/libwasi-emulated-*.a; do
    [[ -f "$lib" ]] || continue
    name=$(basename "$lib")
    echo "Checking $name..."
    for obj in $(ar t "$lib" 2>/dev/null); do
        content=$(ar p "$lib" "$obj" 2>/dev/null | wasm-objdump -x - 2>&1 || true)
        aliases=$(echo "$content" | grep -E '<(vfork|setsid)> -> "(fork|getpid)"' || true)
        if [[ -n "$aliases" ]]; then
            echo "FOUND ALIAS in $name:$obj:"
            echo "$aliases"
            FOUND=1
        fi
    done
done

echo "============================================"
if [[ $FOUND -eq 0 ]]; then
    echo "No problematic aliases found in wasi-libc"
    exit 0
else
    echo "Problematic aliases found - these will corrupt busybox exports"
    exit 1
fi
