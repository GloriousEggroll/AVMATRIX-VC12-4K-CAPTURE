#!/usr/bin/env bash
# Stage 4.4 zero-copy checks (OBS closed, a steady input, root via sudo).
# Each case runs one or two v4l2-ctl streams through framecheck.py and
# reports the IRQ thread's CPU time and the driver's zero-copy counters.
# Ends with zero_copy back to what it was.
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
BDF=${BDF:-0000:0b:00.0}
DEV=${DEV:-/dev/video0}
N=${N:-600}
DBG=/sys/kernel/debug/hws-$BDF
PARAM=/sys/module/HwsUHDX1Capture/parameters/zero_copy
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"; echo "$was" | sudo -n tee "$PARAM" >/dev/null' EXIT

pgrep -x obs >/dev/null && { echo "close OBS first"; exit 1; }
[ -e "$PARAM" ] || { echo "the loaded driver has no zero_copy parameter"; exit 1; }
was=$(cat "$PARAM")
v4l2-ctl -d "$DEV" --set-dv-bt-timings query >/dev/null 2>&1 || true
read -r w h < <(v4l2-ctl -d "$DEV" --query-dv-timings 2>/dev/null | awk '/Active width/{w=$3} /Active height/{h=$3} END{print w, h}')
[ -n "${w:-}" ] || { echo "no input signal"; exit 1; }
size=$((w * h * 2))
irq=$(cat "/sys/bus/pci/devices/$BDF/irq")
tid=$(ps -eo pid=,comm= | awk -v p="irq/$irq-" 'index($2,p)==1{print $1; exit}')
cpu_ms() { awk '{print ($14+$15)*10}' "/proc/$tid/stat"; }
zc() { sudo -n grep "^zero-copy" "$DBG/status"; }
count() { journalctl -k -b --no-pager | grep -c "$1" || true; }
faults0=$(count IO_PAGE_FAULT); stray0=$(count "stray DMA write"); warn0=$(count "WARNING:")
echo "input ${w}x${h}, IRQ $irq (thread $tid), IOMMU $(cat "/sys/bus/pci/devices/$BDF/iommu_group/type")"
echo "register 9: $(sudo -n grep -o 'register 9 [^ ]*' "$DBG/dma")"

stream() {  # name io: one stream through framecheck
    # framecheck reads the log only after stdin closes, when v4l2-ctl has exited
    # shellcheck disable=SC2094
    v4l2-ctl -d "$DEV" --set-fmt-video=width="$w",height="$h",pixelformat=YUYV \
        "--stream-$2=4" --stream-count="$N" --stream-to=- --verbose 2>"$OUT/$1.log" \
      | python3 -I "$HERE/framecheck.py" - "$OUT/$1.log" "$size" | sed "s/^/  $1: /"
}

run() {  # label zero_copy io [second io]
    echo "$2" | sudo -n tee "$PARAM" >/dev/null
    local c0 c1
    c0=$(cpu_ms)
    if [ -n "${4:-}" ]; then
        stream a "$3" & stream b "$4"; wait
    else
        stream a "$3"
    fi
    c1=$(cpu_ms)
    echo "== $1: IRQ thread $((c1 - c0)) ms for $N frames; $(zc)"
}

run "copy path (zero_copy=0, mmap)" 0 mmap
run "zero-copy (mmap)" 1 mmap
run "zero-copy + a second mmap consumer" 1 mmap mmap
run "zero_copy=1, userptr (copy path)" 1 user

echo "== 100 start/stop cycles with zero-copy"
echo 1 | sudo -n tee "$PARAM" >/dev/null
for i in $(seq 100); do
    v4l2-ctl -d "$DEV" --set-fmt-video=width="$w",height="$h",pixelformat=YUYV \
        --stream-mmap=4 --stream-count=$((i % 7 + 1)) --stream-to=/dev/null >/dev/null 2>&1 \
        || echo "  cycle $i failed"
done
echo "  $(zc)"
echo "== kernel log: IO_PAGE_FAULT +$(( $(count IO_PAGE_FAULT) - faults0 )), stray DMA canary +$(( $(count "stray DMA write") - stray0 )), WARNING +$(( $(count "WARNING:") - warn0 ))"
