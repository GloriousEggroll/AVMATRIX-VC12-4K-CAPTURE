#!/usr/bin/env python3
"""Luma (Y) statistics of raw YUYV frames, to tell full from limited range.

Usage: tests/yrange.py FILE WIDTH HEIGHT
Limited-range video keeps nearly all Y values within 16-235; full range uses
0-255. Capture a few frames first, for example:
  v4l2-ctl --set-fmt-video=width=W,height=H,pixelformat=YUYV --stream-mmap=4 \
           --stream-skip=10 --stream-count=3 --stream-to=FILE
"""
import sys

def main():
    path, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
    n = w * h * 2
    data = open(path, 'rb').read()
    frames = len(data) // n
    if not frames:
        sys.exit(f"{path}: shorter than one {w}x{h} frame")
    hist = [0] * 256
    for f in range(frames):
        y = data[f * n:(f + 1) * n:2]
        for v in range(256):
            hist[v] += y.count(v)
    total = sum(hist)

    def pct(p):
        acc = 0
        for v in range(256):
            acc += hist[v]
            if acc >= total * p:
                return v

    used = [v for v in range(256) if hist[v]]
    print(f"frames {frames}  Y min {used[0]}  max {used[-1]}  p0.1 {pct(0.001)}  p99.9 {pct(0.999)}  "
          f"below 16: {sum(hist[:16]) / total:.3%}  above 235: {sum(hist[236:]) / total:.3%}")

if __name__ == '__main__':
    main()
