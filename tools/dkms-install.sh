#!/usr/bin/env bash
# Install a git ref of the driver as its own DKMS version, for every kernel
# that has headers, so `dkms status` shows which build each kernel has and a
# rollback is `dkms install` of an earlier version. AUTOINSTALL is on, and
# DKMS builds the newest version for new kernels, so versions must sort:
# 1.0.0.230324.<stage>.<n>, e.g. 1.0.0.230324.3.0.
#
# Usage:
#   tools/dkms-install.sh <git-ref> <version>            add, build, install
#   tools/dkms-install.sh --build-only <git-ref> <version>  add and build (a rollback target)
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
install=1
[ "${1:-}" = "--build-only" ] && { install=0; shift; }
[ $# -eq 2 ] || { sed -n '2,15p' "$0"; exit 2; }
ref=$1 ver=$2
[[ $ver =~ ^1\.0\.0\.230324\.[0-9]+\.[0-9]+$ ]] || { echo "version must look like 1.0.0.230324.3.0" >&2; exit 2; }
commit=$(git -C "$REPO" rev-parse --short "$ref^{commit}")
dir=/usr/src/$NAME-$ver
[ -e "$dir" ] && { echo "$dir exists; pick a new version" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
# Sources only: the repository also tracks build output (.o, .ko, .cmd),
# which kbuild would reuse instead of compiling the new source.
git -C "$REPO" archive "$commit" src/Makefile src/dkms.conf src/hws_video.c src/hws.h src/hws_reg.h \
    | tar -x -C "$tmp" --strip-components=1
sed -i "s/^PACKAGE_VERSION=.*/PACKAGE_VERSION=$ver/" "$tmp/dkms.conf"
echo "$commit" >"$tmp/SOURCE_COMMIT"
sudo mkdir "$dir"
sudo cp -a "$tmp"/. "$dir"/
sudo dkms add -m "$NAME" -v "$ver"
for kb in /lib/modules/*/build; do
    k=$(basename "$(dirname "$kb")")
    sudo dkms build -m "$NAME" -v "$ver" -k "$k"
    [ $install -eq 1 ] && sudo dkms install -m "$NAME" -v "$ver" -k "$k" --force
done
echo "== $NAME $ver from $commit"
status
