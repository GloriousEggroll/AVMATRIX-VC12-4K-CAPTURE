#!/usr/bin/env bash
# Build the module against a distribution's kernel headers, inside that
# distribution's container (stage 7.4). Used by .github/workflows/build.yml
# and locally:
#
#   podman run --rm -v "$PWD:/src:Z" -w /src fedora:latest tools/ci-build.sh
#
# Installs a compiler, perl (W=1 runs kernel-doc) and the distribution's
# kernel headers, then runs make against the header directory. It names that directory explicitly:
# uname -r in a container is the host's kernel. Builds in a copy of src/,
# so the mounted tree stays clean. Prints the kernel version it built for.
set -euo pipefail

# shellcheck source=/dev/null
. /etc/os-release
# the newest of the paths a glob matched
newest() { printf '%s\n' "$@" | sort -V | tail -1; }
echo "== $PRETTY_NAME"
case "$ID" in
    fedora|centos|almalinux|rocky|rhel)
        dnf -y -q install gcc make kernel-devel elfutils-libelf-devel >/dev/null
        kdir=$(newest /usr/src/kernels/*/) ;;
    ubuntu)
        export DEBIAN_FRONTEND=noninteractive
        apt-get -qq update
        apt-get -qq install -y gcc make perl linux-headers-generic >/dev/null
        kdir=$(newest /usr/src/linux-headers-*-generic/) ;;
    debian)
        export DEBIAN_FRONTEND=noninteractive
        apt-get -qq update
        apt-get -qq install -y gcc make perl linux-headers-amd64 >/dev/null
        kdir=$(newest /usr/src/linux-headers-*-amd64/) ;;
    arch)
        pacman -Sy --noconfirm --needed --quiet gcc make linux-headers >/dev/null
        kdir=$(newest /usr/lib/modules/*/build/) ;;
    *)
        echo "unsupported distribution: $ID" >&2; exit 2 ;;
esac
kdir=${kdir%/}
[ -f "$kdir/Makefile" ] || { echo "no kernel headers found" >&2; exit 1; }
kver=$(make -s -C "$kdir" kernelrelease 2>/dev/null || basename "$(dirname "$kdir")")
echo "== headers $kdir ($kver), $(gcc --version | head -1)"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp -r src/. "$work"/
make -C "$kdir" M="$work" modules W=1 2>&1 | tee "$work/build.log"
[ -f "$work/HwsUHDX1Capture.ko" ] || { echo "== FAILED: $PRETTY_NAME, kernel $kver" >&2; exit 1; }
echo "== built HwsUHDX1Capture.ko for $kver on $PRETTY_NAME, $(grep -c 'warning:' "$work/build.log" || true) W=1 warnings"
