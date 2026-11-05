#!/usr/bin/env python3
"""Find stale-audio replay loops in a raw capture of the card's PipeWire node.

When the card stops sending audio (e.g. while a console changes resolution)
and the driver delivers nothing, PipeWire re-delivers its last buffers of the
card's audio: each sample equals the one 4864 or 5120 frames earlier to within
a few LSB, heard as a buzz. This lists runs of audio, replay LOOP and digital
SILENCE. Before the driver's silence fill a resolution change shows a LOOP run
of 3-4 s; with it, the same stretch is SILENCE.

Record with: pw-record --target alsa_input.pci-0000_0b_00.0.stereo-fallback \
             --rate 48000 --channels 2 --format s16 --raw out.raw
Usage: staleaudio.py out.raw
"""
import sys

import numpy as np

PKT, TOL, QUIET, LAGS = 1024, 8, 64, (4864, 5120)

raw = np.fromfile(sys.argv[1], dtype=np.int16).reshape(-1, 2).astype(np.int32)
runs, last, start = [], None, 0.0
for i in range(len(raw) // PKT):
    s = i * PKT
    pk = raw[s:s + PKT]
    peak = int(np.abs(pk).max())
    if peak == 0:
        kind = "SILENCE"
    elif peak > QUIET and s >= max(LAGS) and np.all(
            np.min([np.abs(pk - raw[s - l:s - l + PKT]) for l in LAGS], axis=0) <= TOL):
        kind = "LOOP"
    else:
        kind = "audio"
    if kind != last:
        if last is not None:
            runs.append((start, s / 48000, last))
        last, start = kind, s / 48000
runs.append((start, len(raw) / 48000, last))
# a loop slips a block about once a second: one odd packet between two LOOP runs
merged = []
for r in runs:
    if (len(merged) >= 2 and r[2] == "LOOP" and merged[-1][2] == "audio"
            and merged[-1][1] - merged[-1][0] < 1.5 * PKT / 48000 and merged[-2][2] == "LOOP"):
        merged.pop()
        merged[-1] = (merged[-1][0], r[1], "LOOP")
    else:
        merged.append(r)
runs = merged
for a, b, k in runs:
    print(f"{a:8.3f}-{b:8.3f}s {b - a:6.3f}s {k}")
loops = [r for r in runs if r[2] == "LOOP"]
print(f"replay loops: {len(loops)}, {sum(b - a for a, b, _ in loops):.2f} s")
