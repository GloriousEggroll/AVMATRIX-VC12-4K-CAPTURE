#!/usr/bin/env bash
# Stage 6.17: the resolution matrix, with the PC's GPU driving the card's
# input (OBS closed, root via sudo, KDE for kscreen-doctor).
#
# Usage: tests/modematrix.sh [OUTPUT] [MODE_ID...]
#
# For zero_copy 0 and then 1, walks the modes up and back down. At each
# step it sets the mode with kscreen-doctor, waits for the driver to report
# the new input, and captures N frames (default 300) at the native size
# through framecheck.py. Prints one table row per step, then the kernel
# log lines that would show a fault. Ends with zero_copy and the output's
# mode as they were.
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
BDF=${BDF:-0000:0b:00.0}
DEV=${DEV:-/dev/video0}
N=${N:-300}
DBG=/sys/kernel/debug/hws-$BDF
PARAM=/sys/module/HwsUHDX1Capture/parameters/zero_copy
out=${1:-HDMI-A-2}
shift || true
# 720p60, 1080p60, 1440p60, 2160p60, 1080p120, 1080p240 on this GPU and output
ids=("${@:-39 24 17 1 21 20}")
read -ra ids <<<"${ids[*]}"

# 6.21: three times the PC's GPU (RX 9070) dropped off the PCIe bus during a
# mode change on the output feeding the card, once with nothing capturing.
[ "${HWS_ALLOW_GPU_MODESET:-}" = 1 ] || {
    echo "refusing: GPU mode changes on the card's input have locked up this PC (SPEC 6.21)."
    echo "Set HWS_ALLOW_GPU_MODESET=1 only with the operator's OK."; exit 1; }
pgrep -x obs >/dev/null && { echo "close OBS first"; exit 1; }
was=$(cat "$PARAM")
LOG=$(mktemp)
modes=$(kscreen-doctor -o 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' |
    awk -v o="$out" '$1=="Output:" {f=($3==o)} f && /Modes:/' | tr ' ' '\n')
# the current mode carries a '*'
orig=$(echo "$modes" | grep -m1 '\*' | cut -d: -f1)
trap 'rm -f "$LOG"; echo "$was" | sudo -n tee "$PARAM" >/dev/null; [ -n "$orig" ] && kscreen-doctor "output.$out.mode.$orig" >/dev/null 2>&1' EXIT
mode_of() { echo "$modes" | grep -m1 "^$1:" | cut -d: -f2 | tr -d '*!'; }
st() { sudo -n cat "$DBG/status"; }
field() { st | awk -v k="$1" 'index($0,k)==1'; }
zc_frames() { field "zero-copy:" | sed -E 's/.*frames ([0-9]+).*/\1/'; }
count() { journalctl -k -b --no-pager | grep -c "$1" || true; }
pats=(IO_PAGE_FAULT "stray DMA write" "WARNING:" "BUG:" "lost its run state")
declare -A base
for p in "${pats[@]}"; do base[$p]=$(count "$p"); done

# up, then back down without repeating the top
walk=("${ids[@]}")
for ((i = ${#ids[@]} - 2; i >= 0; i--)); do walk+=("${ids[i]}"); done

printf '| zero_copy | GPU mode | Driver input | fps | delta median / stdev ms | unique | gaps | repeats | errors | zero-copy frames |\n'
printf '|---|---|---|---|---|---|---|---|---|---|\n'
for zc in 0 1; do
    echo "$zc" | sudo -n tee "$PARAM" >/dev/null
    for id in "${walk[@]}"; do
        m=$(mode_of "$id")
        size=${m%@*} w=${m%x*} h=${size#*x}
        kscreen-doctor "output.$out.mode.$id" >/dev/null 2>&1
        # the driver updates the input only while capturing, so poll with a short stream
        got=""
        for _ in $(seq 20); do
            v4l2-ctl -d "$DEV" --stream-mmap=4 --stream-count=3 --stream-to=/dev/null >/dev/null 2>&1
            got=$(field "input:" | awk '{print $2}')
            [ "$got" = "$size" ] && break
            sleep 0.5
        done
        sleep 1
        z0=$(zc_frames)
        # framecheck reads the log only after stdin closes, when v4l2-ctl has exited
        # shellcheck disable=SC2094
        r=$(v4l2-ctl -d "$DEV" --set-fmt-video=width="$w",height="$h",pixelformat=YUYV \
                --stream-mmap=4 --stream-count="$N" --stream-to=- --verbose 2>"$LOG" \
              | python3 -I "$HERE/framecheck.py" - "$LOG" $((w * h * 2)) 2>&1)
        z1=$(zc_frames)
        get() { echo "$r" | awk -v k="$1" 'index($0,k)==1 {sub(k" *",""); print}'; }
        fps=$(get "delivered fps")
        delta=$(get "delta ms" | awk '{print $4" / "$8}')
        uniq=$(get "unique frames" | awk '{print $1"/"$3}')
        gaps=$(get "sequence gaps")
        reps=$(get "sequence repeats" | awk '{print $1}')
        errs=$(get "error buffers" | awk '{print $1}')
        printf '| %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n' \
            "$zc" "$m" "${got:-none}" "${fps:-?}" "${delta:-?}" "${uniq:-?}" "${gaps:-?}" "${reps:-?}" "${errs:-?}" $((z1 - z0))
        [ -n "$fps" ] || printf '    %s\n' "$r"
    done
done
for p in "${pats[@]}"; do printf '%s +%d, ' "$p" $(( $(count "$p") - ${base[$p]} )); done
echo
