#!/usr/bin/env bash
# Compile src/ against every installed kernel's headers (/lib/modules/*/build)
# and report errors plus warnings that the base ref does not also produce.
# The driver has many old warnings; this keeps new ones visible.
#
# Usage: tests/build-all-kernels.sh [base-ref]   (default: f40a4d5)
set -uo pipefail
REPO=$(cd "$(dirname "$0")/.." && pwd)
BASE=${1:-f40a4d5}
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

strip_loc() { sed -E 's/^[^ ]*:[0-9]+:[0-9]+: //'; }
warnings() { grep -E 'warning:|error:' "$1" | grep -v 'no previous prototype' | grep -v 'pahole version'; }

rc=0
for kb in /lib/modules/*/build; do
    kv=$(basename "$(dirname "$kb")")
    mkdir -p "$WORK/$kv/new" "$WORK/$kv/base"
    cp "$REPO"/src/*.c "$REPO"/src/*.h "$REPO"/src/Makefile "$WORK/$kv/new/"
    git -C "$REPO" archive "$BASE" src | tar -x -C "$WORK/$kv/base" --strip-components=1
    make -s -C "$kb" M="$WORK/$kv/base" modules >"$WORK/$kv/base.log" 2>&1
    if make -s -C "$kb" M="$WORK/$kv/new" modules >"$WORK/$kv/new.log" 2>&1 \
       && [ -f "$WORK/$kv/new/HwsUHDX1Capture.ko" ]; then st=OK; else st=FAIL; rc=1; fi
    warnings "$WORK/$kv/base.log" | strip_loc | sort -u >"$WORK/$kv/base.w"
    warnings "$WORK/$kv/new.log" | while IFS= read -r l; do
        grep -qxF -- "$(strip_loc <<<"$l")" "$WORK/$kv/base.w" || echo "$l"
    done >"$WORK/$kv/added.w"
    echo "$kv: $st ($(wc -l <"$WORK/$kv/added.w") new warnings/errors vs $BASE)"
    head -20 "$WORK/$kv/added.w"
done
exit $rc
