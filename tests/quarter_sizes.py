#!/usr/bin/env python3
"""Check which input sizes fit the DMA buffers.

Mirrors SetVideoFormteSize() in src/hws_video.c: a frame is DMA'd in four
quarters, each into its own buffer whose last MAX_AUDIO_CAP_SIZE bytes hold
the audio ring. Exits non-zero if the expectations below don't hold.
"""
import sys

MAX_AUDIO_CAP_SIZE = 10 * 1024
BUF_4M = 0x400000      # MAX_MM_VIDEO_SIZE
BUF_4M5 = 0x480000     # MAX_MM_VIDEO_SIZE_DCI


def quarters(w, h):
    size = w * h * 2
    q = (size // (4 * 16 * 128)) * 16 * 128
    last = size - (size // (4 * 16 * 128)) * 16 * 128 * 3
    last = (last // (16 * 128) + 1) * 16 * 128
    return q, last


def fits(w, h, buf):
    q, last = quarters(w, h)
    return max(q, last) <= buf - MAX_AUDIO_CAP_SIZE


SIZES = [(1280, 720), (1920, 1080), (2560, 1440), (3440, 1440), (3840, 2160), (4096, 2160)]
ok = True
for w, h in SIZES:
    q, last = quarters(w, h)
    small, large = fits(w, h, BUF_4M), fits(w, h, BUF_4M5)
    print(f"{w:4d}x{h:<4d} quarter {q:9d} last {last:9d}  4 MiB: {'fits' if small else 'TOO BIG'}  "
          f"4.5 MiB: {'fits' if large else 'TOO BIG'}")
    ok &= large
    ok &= small == ((w, h) != (4096, 2160))
print("OK" if ok else "FAIL: expectations not met")
sys.exit(0 if ok else 1)
