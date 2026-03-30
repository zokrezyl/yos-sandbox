#!/bin/bash
# Dump all exports from a wasm file for debugging
# Usage: ./tests/dump-exports.sh <file.wasm|file.o>

FILE="$1"
if [[ -z "$FILE" || ! -f "$FILE" ]]; then
    echo "Usage: $0 <file.wasm|file.o>"
    exit 1
fi

echo "All exports in $FILE:"
echo "============================================"
wasm-objdump -x "$FILE" 2>&1 | grep -E '^\s*-\s*func\[[0-9]+\].*->' | head -50
echo "============================================"
