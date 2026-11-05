#!/usr/bin/env python3
"""Check frame delivery from a v4l2-ctl capture.

From a file:
    v4l2-ctl -d /dev/video0 --stream-mmap=4 --stream-count=300 \
        --stream-to=cap.raw --verbose 2> cap.log
    framecheck.py cap.raw cap.log

Without writing frames to disk (raw path "-", frame size in bytes required):
    v4l2-ctl ... --stream-to=- --verbose 2> cap.log | framecheck.py - cap.log 4147200
The log is read after stdin closes, when v4l2-ctl has exited.

Reports the delivered frame rate, how many frames are byte-for-byte
duplicates of the previous one, timestamp spacing, and sequence gaps.
A healthy 59.94 fps source gives ~59.94 fps, no duplicates, deltas near
16.68 ms and no bursts.
"""
import hashlib
import re
import statistics
import sys

LINE = re.compile(r"seq:\s*(\d+) bytesused: (\d+) ts: ([\d.]+)")
BURST_MS = 4.0  # two deliveries closer than this cannot be separate 60 fps frames


def parse_log(path):
    frames = []
    with open(path, errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if m:
                frames.append((int(m[1]), int(m[2]), float(m[3])))
    return frames


def hash_frames(raw, sizes):
    hashes = []
    for size in sizes:
        data = raw.read(size)
        if len(data) < size:
            break
        hashes.append(hashlib.sha1(data).digest())
    return hashes


def main():
    args = sys.argv[1:]
    if len(args) not in (2, 3) or (args[0] == "-" and len(args) != 3):
        sys.exit(__doc__)
    raw_path, log_path = args[:2]

    if raw_path == "-":
        # hash fixed-size frames as they stream in; the log is complete
        # only after v4l2-ctl exits, which is when stdin reaches EOF
        size = int(args[2])
        hashes = hash_frames(sys.stdin.buffer, iter(lambda: size, None))
        frames = parse_log(log_path)
    else:
        frames = parse_log(log_path)
        with open(raw_path, "rb") as raw:
            hashes = hash_frames(raw, [s for _, s, _ in frames])

    if len(frames) < 2:
        sys.exit("fewer than 2 frames in the log")

    seqs = [s for s, _, _ in frames]
    ts = [t for _, _, t in frames]
    deltas = [(b - a) * 1000 for a, b in zip(ts, ts[1:])]
    span = ts[-1] - ts[0]
    dup_prev = sum(1 for a, b in zip(hashes, hashes[1:]) if a == b)
    gaps = sum(b - a - 1 for a, b in zip(seqs, seqs[1:]) if b - a > 1)

    print(f"frames            {len(frames)} over {span:.3f} s")
    print(f"delivered fps     {(len(frames) - 1) / span:.2f}")
    print(f"unique frames     {len(set(hashes))} of {len(hashes)}")
    print(f"same as previous  {dup_prev}")
    print(f"delta ms          min {min(deltas):.2f}  median {statistics.median(deltas):.2f}  "
          f"max {max(deltas):.2f}  stdev {statistics.pstdev(deltas):.2f}")
    print(f"burst deliveries  {sum(1 for d in deltas if d < BURST_MS)} (delta < {BURST_MS} ms)")
    print(f"sequence gaps     {gaps}")


if __name__ == "__main__":
    main()
