#!/usr/bin/env bash
# Install a git ref of the driver as its own DKMS version, for every kernel
# that has headers, so `dkms status` shows which build each kernel has and a
# rollback is `dkms install` of an earlier version. AUTOINSTALL is on, and
# DKMS builds the newest version for new kernels, so a new version must sort
# above the installed ones: 1.0.0.230324.<stage>.<n> during development,
# <release>.<n> (e.g. 1.1.0.1) once a release (src/dkms.conf) is installed.
#
# Usage:
#   tools/dkms-install.sh <git-ref> <version>            add, build, install
#   tools/dkms-install.sh --build-only <git-ref> <version>  add and build (a rollback target)
#   tools/dkms-install.sh --remove <version>             remove one version
#   tools/dkms-install.sh --status                        version and srcversion per kernel
#
# A new build takes effect after a reboot; `modprobe -r` can hang while
# PipeWire holds the card's ALSA device.
set -euo pipefail
NAME=HwsUHDX1CaptureForLinux
MOD=HwsUHDX1Capture
REPO=$(cd "$(dirname "$0")/.." && pwd)

status() {
    dkms status "$NAME" 2>/dev/null
    for kb in /lib/modules/*/build; do
        k=$(basename "$(dirname "$kb")")
        printf '%s: srcversion %s\n' "$k" "$(modinfo -k "$k" -F srcversion "$MOD" 2>/dev/null || echo none)"
    done
    printf 'running: %s\n' "$(cat /sys/module/$MOD/srcversion 2>/dev/null || echo not loaded)"
}

[ "${1:-}" = "--status" ] && { status; exit 0; }
if [ "${1:-}" = "--remove" ]; then
    [ $# -eq 2 ] || { sed -n '2,17p' "$0"; exit 2; }
    sudo dkms remove -m "$NAME" -v "$2" --all
    sudo rm -rf "/usr/src/$NAME-$2"
    status
    exit 0
fi
install=1
[ "${1:-}" = "--build-only" ] && { install=0; shift; }
[ $# -eq 2 ] || { sed -n '2,17p' "$0"; exit 2; }
ref=$1 ver=$2
[[ $ver =~ ^[0-9]+(\.[0-9]+)+$ ]] || { echo "version must be dotted numbers, e.g. 1.0.0.230324.3.0" >&2; exit 2; }
newest=$(dkms status "$NAME" 2>/dev/null | sed -nE 's|^[^/,]+[/,] *([0-9.]+).*|\1|p' | sort -V | tail -1)
if [ -n "$newest" ] && [ "$(printf '%s\n%s\n' "$newest" "$ver" | sort -V | tail -1)" != "$ver" ]; then
    echo "warning: $ver sorts below the installed $newest, so DKMS will build $newest for new kernels" >&2
fi
commit=$(git -C "$REPO" rev-parse --short "$ref^{commit}")
dir=/usr/src/$NAME-$ver
[ -e "$dir" ] && { echo "$dir exists; pick a new version" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
# Sources only: some commits also carry build output (.o, .ko, .cmd,
# .mod.c), which kbuild would reuse instead of compiling the new source.
git -C "$REPO" archive "$commit" src | tar -x -C "$tmp" --strip-components=1
find "$tmp" -mindepth 1 \( -name '*.mod.c' -o \! \( -name '*.c' -o -name '*.h' -o -name Makefile -o -name dkms.conf \) \) -delete
sed -i "s/^PACKAGE_VERSION=.*/PACKAGE_VERSION=$ver/" "$tmp/dkms.conf"
echo "$commit" >"$tmp/SOURCE_COMMIT"
sudo mkdir "$dir"
sudo cp -a "$tmp"/. "$dir"/
sudo dkms add -m "$NAME" -v "$ver"
for kb in /lib/modules/*/build; do
    k=$(basename "$(dirname "$kb")")
    [ -e "$kb/Makefile" ] || { echo "skipping $k: its build link points nowhere (headers removed)" >&2; continue; }
    sudo dkms build -m "$NAME" -v "$ver" -k "$k"
    [ $install -eq 1 ] && sudo dkms install -m "$NAME" -v "$ver" -k "$k" --force
done
echo "== $NAME $ver from $commit"
status
