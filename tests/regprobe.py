#!/usr/bin/env python3
"""Stage 4 register probe: write one capture-block register, watch others.

Usage (root, OBS closed, a capture running as the observer):
  tests/regprobe.py REG VALUE --pci 0000:0b:00.0 [--watch 90,110,5] [--seconds 3]

Rules (docs/SPEC.md, probe rules):
- only registers in ALLOWED may be written;
- each written register is read first and restored on exit, including Ctrl-C;
- refuses to run unless --pci names an 8888:8581 bound to HwsUHDX1Capture;
- refuses to run unless the card's IOMMU domain is translated (DMA or DMA-FQ).
Registers are numbered as in docs/REGISTERS.md: BAR0 + 0x4000 + 4 * n.
"""
import argparse, mmap, os, re, signal, struct, sys, time

ALLOWED = {120, 130}            # output size and output rate (probe 4.3)
BASE = 0x4000

def check_card(pci):
    """Exit unless pci is a VC12-4K bound to this driver (7.6)."""
    if not re.fullmatch(r"[0-9a-f]{4}:[0-9a-f]{2}:[0-9a-f]{2}\.[0-7]", pci):
        sys.exit(f"--pci {pci!r} isn't a PCI address like 0000:0b:00.0")
    dev = f"/sys/bus/pci/devices/{pci}"
    try:
        ids = (open(f"{dev}/vendor").read().strip(), open(f"{dev}/device").read().strip())
    except OSError:
        sys.exit(f"no PCI device {pci}")
    if ids != ("0x8888", "0x8581"):
        sys.exit(f"{pci} is {ids[0]}:{ids[1]}, not the VC12-4K (0x8888:0x8581); refusing")
    driver = os.path.basename(os.path.realpath(f"{dev}/driver")) if os.path.exists(f"{dev}/driver") else None
    if driver != "HwsUHDX1Capture":
        sys.exit(f"{pci} is bound to {driver!r}, not HwsUHDX1Capture; refusing")

def iommu_type(pci):
    g = os.path.realpath(f"/sys/bus/pci/devices/{pci}/iommu_group")
    try:
        return open(os.path.join(g, "type")).read().strip()
    except OSError:
        return None

def fmt(n, v):
    if n in (90, 120):
        return f"{v & 0xffff}x{v >> 16}"
    return f"{v:#x}" if n in (5, 0) else str(v)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("reg", type=int)
    ap.add_argument("value", type=lambda s: int(s, 0))
    ap.add_argument("--watch", default="90,110,5")
    ap.add_argument("--seconds", type=float, default=3)
    ap.add_argument("--pci", required=True, help="the card's PCI address, e.g. 0000:0b:00.0")
    a = ap.parse_args()
    if a.reg not in ALLOWED:
        sys.exit(f"register {a.reg} is not allowed (allowed: {sorted(ALLOWED)})")
    check_card(a.pci)
    t = iommu_type(a.pci)
    if t not in ("DMA", "DMA-FQ"):
        sys.exit(f"IOMMU domain type is {t!r}, not translated; refusing")
    watch = [int(x) for x in a.watch.split(",") if x]
    fd = os.open(f"/sys/bus/pci/devices/{a.pci}/resource0", os.O_RDWR | os.O_SYNC)
    m = mmap.mmap(fd, 0x5000, mmap.MAP_SHARED, mmap.PROT_READ | mmap.PROT_WRITE)
    rd = lambda n: struct.unpack_from("<I", m, BASE + 4 * n)[0]
    wr = lambda n, v: struct.pack_into("<I", m, BASE + 4 * n, v)
    orig = rd(a.reg)

    def restore(*_):
        wr(a.reg, orig)
        print(f"restored reg {a.reg} = {fmt(a.reg, orig)} (reads {fmt(a.reg, rd(a.reg))})")
        sys.exit(0)
    signal.signal(signal.SIGINT, restore)
    signal.signal(signal.SIGTERM, restore)

    before = {n: rd(n) for n in watch}
    print(f"reg {a.reg}: {fmt(a.reg, orig)} -> writing {fmt(a.reg, a.value)}; before: "
          + ", ".join(f"{n}={fmt(n, v)}" for n, v in before.items()))
    try:
        wr(a.reg, a.value)
        print(f"  reads back {fmt(a.reg, rd(a.reg))}")
        last, t0 = dict(before), time.monotonic()
        while time.monotonic() - t0 < a.seconds:
            now = {n: rd(n) for n in watch}
            if now != last:
                print(f"  +{time.monotonic() - t0:5.2f}s " + ", ".join(
                    f"{n}={fmt(n, v)}" for n, v in now.items() if v != last[n]))
                last = now
            time.sleep(0.02)
        print("  after: " + ", ".join(f"{n}={fmt(n, v)}" for n, v in last.items()))
    finally:
        restore()

if __name__ == "__main__":
    main()
