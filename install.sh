#!/usr/bin/env bash
# Install the AVMatrix VC12-4K driver (HwsUHDX1Capture) through DKMS.
#
# Copies the sources to /usr/src/HwsUHDX1CaptureForLinux-<version>, then
# builds and installs the module for every installed kernel that has its
# headers. DKMS rebuilds it for kernels installed later. Run it again to
# reinstall; uninstall.sh removes everything this or older installers put
# on the system.
set -euo pipefail
[ "$(id -u)" -eq 0 ] || exec sudo -- "$0" "$@"
# Ubuntu's DKMS asks for a Secure Boot key password through debconf; with
# no terminal to ask on, let it skip that instead of waiting forever
[ -t 0 ] || export DEBIAN_FRONTEND=noninteractive

NAME=HwsUHDX1CaptureForLinux
MODULE=HwsUHDX1Capture
HERE=$(cd "$(dirname "$0")" && pwd)
VER=$(sed -n 's/^PACKAGE_VERSION=//p' "$HERE/src/dkms.conf")
[ -n "$VER" ] || { echo "no PACKAGE_VERSION in $HERE/src/dkms.conf" >&2; exit 1; }

# in a subshell: os-release defines NAME too
# shellcheck source=/dev/null
os=$(. /etc/os-release 2>/dev/null; echo "${ID:-} ${ID_LIKE:-}")
hint() {
    case " $os " in
        *" fedora "*|*" rhel "*|*" centos "*) echo "sudo dnf install dkms gcc make kernel-devel-$(uname -r)" ;;
        *" debian "*|*" ubuntu "*) echo "sudo apt install dkms gcc make linux-headers-$(uname -r)" ;;
        *" arch "*) echo "sudo pacman -Syu dkms gcc make linux-headers, then reboot if the kernel was updated" ;;
        *) echo "install dkms, gcc, make and the headers for kernel $(uname -r)" ;;
    esac
}
for tool in dkms make gcc; do
    command -v "$tool" >/dev/null || { echo "$tool is missing. Install the build tools first:" >&2; hint >&2; exit 1; }
done
if [ ! -e "/lib/modules/$(uname -r)/build/Makefile" ]; then
    echo "The headers for the running kernel ($(uname -r)) are missing. Install them first:" >&2
    hint >&2
    exit 1
fi

src=/usr/src/$NAME-$VER
echo "Installing $NAME $VER"
if [ -d "/var/lib/dkms/$NAME/$VER" ]; then
    echo "  removing the earlier install of $VER"
    dkms remove -m "$NAME" -v "$VER" --all >/dev/null
fi
rm -rf "$src"
mkdir -p "$src"
# sources only: build output in the tree would be reused instead of compiled
find "$HERE/src" -maxdepth 1 -type f \( -name '*.c' -o -name '*.h' -o -name Makefile -o -name dkms.conf \) \
    ! -name '*.mod.c' -exec cp -t "$src" {} +
dkms add -m "$NAME" -v "$VER" >/dev/null

built=()
for kb in /lib/modules/*/build; do
    k=$(basename "$(dirname "$kb")")
    if [ ! -e "$kb/Makefile" ]; then
        echo "  skipping kernel $k: its headers aren't installed"
        continue
    fi
    echo "  building for kernel $k"
    # output stays on the terminal: Ubuntu may ask for a key password here
    if ! dkms install -m "$NAME" -v "$VER" -k "$k" --force; then
        echo "  build failed for $k; the log is under /var/lib/dkms/$NAME/$VER/build/make.log" >&2
        exit 1
    fi
    built+=("$k")
done
echo "Installed for: ${built[*]}"

# Secure Boot only loads modules signed with an enrolled key.
enrol=0
if command -v mokutil >/dev/null && mokutil --sb-state 2>/dev/null | grep -qi "SecureBoot enabled"; then
    # the certificate DKMS signed with: Ubuntu's shim-signed key or DKMS's
    # own; when both exist, the one whose serial matches the module's
    key="" serial=$(modinfo -F sig_key "$MODULE" 2>/dev/null | tr -d ':' | tr 'a-f' 'A-F')
    for c in /var/lib/dkms/mok.pub /var/lib/shim-signed/mok/MOK.der; do
        [ -e "$c" ] || continue
        [ -z "$key" ] && key=$c
        form=pem
        [ "${c##*.}" = der ] && form=der
        cs=$(openssl x509 -in "$c" -inform "$form" -noout -serial 2>/dev/null | sed 's/^serial=//')
        [ -n "$serial" ] && [ "$cs" = "$serial" ] && { key=$c; break; }
    done
    state=""
    [ -n "$key" ] && state=$(mokutil --test-key "$key" 2>/dev/null || true)
    if [ -z "$key" ]; then
        echo
        echo "Secure Boot is on, but no DKMS signing key was found; see the README."
    elif grep -qi "already enrolled" <<<"$state"; then
        :
    elif grep -qi "enrollment request" <<<"$state"; then
        enrol=1
        cat <<EOF

Secure Boot is on, and the signing key ($key) is queued for enrolment.
Reboot. When the blue MOK manager screen appears, press a key within
10 seconds, then choose "Enroll MOK", "Continue", "Yes", enter the
password you chose, and "Reboot". If the screen times out, run
  sudo mokutil --import $key
and reboot again.
EOF
    else
        enrol=1
        cat <<EOF

Secure Boot is on, and the key DKMS signs the module with isn't enrolled yet:
  1. sudo mokutil --import $key
     (choose a one-time password)
  2. Reboot. When the blue MOK manager screen appears, press a key within
     10 seconds, then choose "Enroll MOK", "Continue", "Yes", enter that
     password, and "Reboot". If it times out, repeat step 1.
Until then the kernel refuses the module ("Key was rejected by service").
EOF
    fi
fi

# Minimal kernels (cloud and server images) leave out the media and sound
# modules the driver needs.
missing=()
for m in videodev videobuf2-vmalloc snd-pcm; do
    modinfo "$m" >/dev/null 2>&1 || missing+=("$m")
done
if [ ${#missing[@]} -gt 0 ]; then
    echo
    echo "This kernel lacks modules the driver needs: ${missing[*]}. Install them:"
    case " $os " in
        *" fedora "*|*" rhel "*|*" centos "*) echo "  sudo dnf install kernel-modules-$(uname -r)" ;;
        *" ubuntu "*) echo "  sudo apt install linux-modules-extra-$(uname -r)" ;;
        *) echo "  the kernel's full module package for $(uname -r)" ;;
    esac
    echo "then run: sudo modprobe $MODULE"
elif [ $enrol -eq 1 ]; then
    echo "The driver loads after the key is enrolled and the PC has rebooted."
elif [ -d "/sys/module/$MODULE" ]; then
    echo "The driver is already loaded. Reboot to use the version just installed ($VER)."
elif modprobe "$MODULE" 2>/dev/null; then
    echo "Loaded. Check with: v4l2-ctl --list-devices; arecord -l"
else
    echo "Not loaded yet. Reboot, or run: sudo modprobe $MODULE"
fi
