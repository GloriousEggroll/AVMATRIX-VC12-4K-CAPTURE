#!/usr/bin/env bash
# Stage 6.17: log a long OBS capture session (root via sudo for debugfs).
#
# Usage: tests/soak.sh [HOURS] [INTERVAL_MIN]   (defaults 8 and 30)
#
# Start OBS capturing first. Every INTERVAL_MIN minutes, and at the end,
# prints one row: the card's frame rate over the interval, frames lost,
# zero-copy failures, audio gaps the driver filled with silence, OBS's resident memory, available system memory, and
# the IO_PAGE_FAULT, stray-write and PipeWire error counts since the start.
set -uo pipefail
# the card bound to the driver (7.6); BDF=... picks one when there are several
cards=(/sys/bus/pci/drivers/HwsUHDX1Capture/0000:*)
[ -e "${cards[0]}" ] || { echo "no device is bound to HwsUHDX1Capture"; exit 1; }
BDF=${BDF:-$(basename "${cards[0]}")}
[ -e "/sys/bus/pci/drivers/HwsUHDX1Capture/$BDF" ] || { echo "$BDF isn't bound to HwsUHDX1Capture"; exit 1; }
DBG=/sys/kernel/debug/hws-$BDF
hours=${1:-8} every=${2:-30}
st() { sudo -n cat "$DBG/status"; }
num() { st | awk -v k="$1" -v n="$2" 'index($0,k)==1 {gsub(",",""); print $n}'; }
since=$(date '+%Y-%m-%d %H:%M:%S')
kcount() { journalctl -k --since "$since" --no-pager | grep -c "$1" || true; }
pwerr() { journalctl --user --since "$since" --no-pager -u pipewire -u wireplumber -u pipewire-pulse \
    | grep -ciE "error|xrun" || true; }
obs_rss() { local p; p=$(pgrep -n -x obs) && awk '/VmRSS/{printf "%d MiB", $2/1024}' "/proc/$p/status" || echo "not running"; }

pgrep -x obs >/dev/null || { echo "start OBS capturing first"; exit 1; }
echo "soak: ${hours} h, a row every ${every} min, from $since; $(st | grep '^input')"
printf '| time | fps | lost | zero-copy failed | audio gaps | OBS RSS | MemAvailable | IO_PAGE_FAULT | stray writes | PipeWire errors |\n'
printf '|---|---|---|---|---|---|---|---|---|---|\n'
row() {
    local f1 t1
    f1=$(num "frames:" 2) t1=$(date +%s.%N)
    printf '| %s | %s | %s | %s | %s | %s | %s MiB | %s | %s | %s |\n' "$(date +%H:%M)" \
        "$(echo "scale=2; ($f1 - $f0) / ($t1 - $t0)" | bc)" "$(num "frames:" 4)" "$(st | sed -nE 's/^zero-copy:.* failed ([0-9]+).*/\1/p')" \
        "$(st | sed -nE 's/^audio: gaps filled with silence: ([0-9]+).*/\1/p')" \
        "$(obs_rss)" "$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)" \
        "$(kcount IO_PAGE_FAULT)" "$(kcount 'stray DMA write')" "$(pwerr)"
    f0=$f1 t0=$t1
}
f0=$(num "frames:" 2) t0=$(date +%s.%N)
sleep 60
row
end=$(( $(date +%s) + hours * 3600 ))
while [ "$(date +%s)" -lt "$end" ]; do
    sleep $(( every * 60 ))
    row
done
