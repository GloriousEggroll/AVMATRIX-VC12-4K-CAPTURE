#!/usr/bin/env bash
# Stage 6.8 and 6.10: capture at a fixed size while someone switches the
# source's output resolution (OBS closed, root via sudo).
#
# Usage: tests/switchtest.sh WIDTH HEIGHT [CHANGES]
#
# Streams WIDTH x HEIGHT through framecheck.py and logs each input-size
# change the driver sees (the "no usable signal" between two sizes is not a
# change), with how long the frame counter then stalled. Stops after CHANGES
# changes (default 40: 20 each way) or on Ctrl-C, then prints the frame
# check, the zero-copy counters and the kernel log lines that would show an
# overrun. LOG=path keeps the v4l2-ctl log.
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
BDF=${BDF:-0000:0b:00.0}
DEV=${DEV:-/dev/video0}
DBG=/sys/kernel/debug/hws-$BDF
w=${1:?width} h=${2:?height} want=${3:-40}
OUT=$(mktemp -d)
trap '[ -n "${LOG:-}" ] && [ -e "$OUT/cap.log" ] && cp "$OUT/cap.log" "$LOG"; rm -rf "$OUT"' EXIT

# OBS may stay open to show the source (WITH_OBS=1). It then usually holds
# the zero-copy buffers and this capture takes the copy path.
[ -z "${WITH_OBS:-}" ] && pgrep -x obs >/dev/null && { echo "close OBS first, or set WITH_OBS=1"; exit 1; }
st() { sudo -n cat "$DBG/status"; }
st | grep -q "engine restarts" || { echo "the loaded driver has no restart counter; load 1.0.0.230324.6.6 or later"; exit 1; }
count() { journalctl -k -b --no-pager | grep -c "$1" || true; }
pats=(IO_PAGE_FAULT "stray DMA write" "WARNING:" "BUG:" "lost its run state")
declare -A base
for p in "${pats[@]}"; do base[$p]=$(count "$p"); done
echo "zero_copy=$(cat /sys/module/HwsUHDX1Capture/parameters/zero_copy) debug_dma=$(cat /sys/module/HwsUHDX1Capture/parameters/debug_dma), capturing ${w}x${h}"
echo "before: $(st | grep -E '^(engine restarts|zero-copy)' | paste -sd' ')"

# Another app streaming (OBS) can keep the card at its own size; check the
# size this stream really gets, since framecheck cuts frames by it.
v4l2-ctl -d "$DEV" --set-fmt-video=width="$w",height="$h",pixelformat=YUYV >/dev/null 2>&1
read -r gw gh < <(v4l2-ctl -d "$DEV" --get-fmt-video | awk -F'[ /:]+' '/Width\/Height/{print $3, $4}')
[ "$gw" = "$w" ] && [ "$gh" = "$h" ] || echo "note: the card stays at ${gw}x${gh} (another app holds the format); capturing that"
w=$gw h=$gh
# framecheck reads the log only after stdin closes, when v4l2-ctl has exited
# shellcheck disable=SC2094
v4l2-ctl -d "$DEV" --set-fmt-video=width="$w",height="$h",pixelformat=YUYV \
    --stream-mmap=4 --stream-count=10000000 --stream-to=- --verbose 2>"$OUT/cap.log" \
  | python3 -I "$HERE/framecheck.py" - "$OUT/cap.log" $((w * h * 2)) >"$OUT/check" &
sleep 1
cap=$(pgrep -n -x v4l2-ctl)

frames() { st | awk '/^frames:/{sub(",","",$2); print $2}'; }
input() { st | awk '/^input:/{print $2}'; }
stop=0
trap 'stop=1' INT
cur=$(input) n=0
until [[ $cur =~ ^[0-9]+x[0-9]+$ ]]; do sleep 0.05; cur=$(input); done
echo "input $cur; switch the source now ($want changes, Ctrl-C to stop early)"
while [ "$stop" = 0 ] && [ "$n" -lt "$want" ] && kill -0 "$cap" 2>/dev/null; do
    sleep 0.05
    now=$(input)
    # a switch passes through "no usable signal"; count only size to size
    [[ $now =~ ^[0-9]+x[0-9]+$ ]] || continue
    [ "$now" = "$cur" ] && continue
    t0=$(date +%s.%N) f0=$(frames)
    n=$((n + 1))
    # the frame counter also counts no-signal frames, so this is how long
    # the counter stalled, not the time to a picture
    while [ "$(frames)" = "$f0" ] && [ "$stop" = 0 ]; do sleep 0.01; done
    printf '%2d  %s -> %s  frame counter moved after %.0f ms\n' "$n" "$cur" "$now" \
        "$(echo "($(date +%s.%N) - $t0) * 1000" | bc)"
    cur=$now
done
trap - INT
kill -TERM "$cap" 2>/dev/null   # background jobs ignore SIGINT
wait
sed 's/^/  /' "$OUT/check"
echo "after: $(st | grep -E '^(engine restarts|zero-copy)' | paste -sd' ')"
st | grep -E "^frames:|stray writes"
for p in "${pats[@]}"; do printf '%s +%d, ' "$p" $(( $(count "$p") - ${base[$p]} )); done
echo
