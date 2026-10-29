#!/bin/sh
# Validate external weights without deleting or substituting any user file.
set -eu
net=${1:-petrified_run4test_e19_l085.nnue}
expected=345d67b733991ae5beb2d3a6ea40d54ea01abb1fb699aa39e6f6406495f4d933
if [ ! -f "$net" ]; then
    echo "Supply run4test using EVALFILE=<path>; missing: $net" >&2
    exit 1
fi
if command -v sha256sum >/dev/null 2>&1; then
    actual=$(sha256sum "$net" | cut -d ' ' -f 1)
elif command -v shasum >/dev/null 2>&1; then
    actual=$(shasum -a 256 "$net" | cut -d ' ' -f 1)
else
    echo "A SHA-256 tool is required" >&2
    exit 1
fi
test "$actual" = "$expected" || { echo "Invalid run4test hash: $net" >&2; exit 1; }
echo "Verified run4test SHA256=$expected"
