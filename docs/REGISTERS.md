# VC12-4K registers

This file lists what is known about the card's registers and where each fact comes from:

- **driver**: this driver (`src/`), which descends from the vendor's own code.
- **windows**: static analysis of AVMatrix's Windows driver, `hwsuhdx1pro.sys` 6.0.0.4 (2026-03-17, x64, SHA-256 `022c4515…5d91`). See [Windows driver](#windows-driver) below. The binary isn't in this repo; addresses like `0x140005e10` are its function entry points.
- **mainline**: the in-tree `hws` driver (`drivers/media/pci/hws/hws_reg.h`) for AVMatrix's multi-channel capture cards. It uses the same register layout but doesn't support this card.
- **measured**: read on this card (FPGA version 122) while it captured a Switch 2 at 3840×2160, 60 fps. See [results.md](results.md).

Stage 5 of [SPEC.md](SPEC.md) extends this file. Record negative results too, so nobody repeats a probe.

All registers are 32 bits wide, in BAR0.

## PCIe bridge

| Offset | Access | Meaning | Source |
|---|---|---|---|
| 0x134 | W | Interrupt enable. Written 0x3FFFF at init. | driver, windows, mainline |
| 0x138 | — | Named "interrupt decode". Not used. | driver, mainline |
| 0x144 | R | PCIe link status:<br>- bit 0: link rate (1 = 5 GT/s, 0 = 2.5 GT/s)<br>- bits 2:1: width (1 = x2, 2 = x4, 3 = x8)<br><br>Windows shows it on the no-signal frame as "PCIE(5G-X4)". | windows |
| 0x148 | — | Named "bridge enable". The write to it is commented out. | driver |
| 0x208 + 8·i | R/W | Address-translation entry i. The upper 32 bits of a host bus address go at +0, its 512 MiB-aligned lower part at +4. The entry maps the card's window i, which starts at (i + 1) · 0x20000000, onto host memory. | driver, windows, mainline |

The translation entries match the layout of the `AXIBAR2PCIEBAR_nU`/`nL` registers in AMD/Xilinx's AXI Bridge for PCI Express (PG194, offsets 0x208–0x234). Register 0x144 matches the bridge's PHY status register at the same offset: link rate in bit 0, width in bits 2:1. The chip markings confirm it: the FPGA is an AMD/Xilinx Artix-7 XC7A35T (see [Hardware](#hardware)).

## Capture block

"Register n" means BAR0 + 0x4000 + 4·n.
- `ch` is the input channel. This card has one.
- `i` is the DMA buffer. There are four, and each frame arrives as four quarters, one per buffer.
- The end of each buffer holds an audio ring.

| n | Access | Meaning | Source | Measured |
|---|---|---|---|---|
| 0 | R/W | **Read:** global status.<br>- 0xFFFFFFFF means the card is gone.<br>- Bit 0 is set while capture runs; Windows skips its start sequence when it's set.<br>- Bit 2 means the card raised the interrupt; Windows checks it before reading register 1.<br>- Mainline names bit 3 "DMA busy".<br><br>**Write:**<br>- Hardware probe: 0x00, wait 100 µs, 0x10, wait 500 µs.<br>- Start: 0x00, program the DMA addresses, 0x3FFFF to BAR0 + 0x134, 0x80000000, wait 500 µs, 0x80FFFFFF, 0x13. This driver writes the same values without the 500 µs pause.<br>- Stop: 0x10. | driver, windows, mainline | 0x0000000B |
| 1 | R/W | Interrupt status:<br>- bit 0: video half done (toggle in register 32)<br>- bit 8: HDMI audio packet done (toggle in register 40)<br>- bit 12: line-in audio packet done (toggle in register 44; PCI ID 8591 only)<br><br>Writing the handled bits back acknowledges them. Mainline acknowledges at BAR0 + 0x8004 instead. | driver, windows | |
| 2 | R/W | Video capture enable, bit i | driver, windows | |
| 3 | R/W | Audio capture enable, bit i. Windows also clears bit 4 (line-in) on stop. | driver, windows | |
| 4 | R | Read once at probe and only logged by Windows. Mainline calls it main control, where bit 0 enables interrupts. | windows, mainline | 0x00000001 |
| 5 | R | Bit ch: signal present. Bit 8 + ch: interlaced. | driver, windows, mainline | 0x00000001 |
| 8 | R | Bit ch: HDCP detected. While it's set, Windows delivers a frame reading "HDCP Protection" instead of the video, and this driver (stage 4.1) black frames. It stayed 0 with a PS5 whose HDCP setting was on, while the card delivered the normal picture. | windows, mainline, measured | 0 (Switch 2; PS5 with HDCP on) |
| 9 | W | A buffer size in 16-byte units. Windows writes 0x50000 (5 MiB) on FPGA version 122 and later. Mainline writes 1920 × 1080 × 2 / 16. This driver never writes it, so it holds its reset value.<br><br>Measured: the card wrote 2 KiB at offset 0x440000 (register 9 × 16) of DMA buffer 3, beyond the frame data, during a session with OBS starts, stops and a suspend with capture running. When it writes there is still being measured. | windows, mainline, measured | 0x00044000 (4.25 MiB) | This driver now sets it to the largest programmed quarter (from `a3fe62e`); the canary then shows only a 2 KiB block at register 9 × 16 on a resolution change.
| 16 + i | R/W | Buffer i's address as the card sees it: (i + 1) · 0x20000000 plus the low 29 bits of the bus address.<br><br>Probe 4.4 measured that the card picks up a new value for every frame. A value written after quarter i's half-done interrupt applies to that quarter in the next frame. 2 KiB-aligned addresses work. The card writes exactly the frame's bytes; the 2 KiB rounding of quarter 3's length stays unwritten. ([results.md](results.md#44-zero-copy-probe-driver-5c8ee4f)) | driver, windows, mainline | |
| 24 + i | R/W | HDMI audio ring address of buffer i, in the same form. The driver writes 24–27: 24 and 27 keep the value, but 25 and 26 read back 0, so 25 and 26 are probably other registers on this card (mainline names 25 a buffer base and 26 a DMA address). | driver, windows | 24, 27: as written; 25, 26: 0 |
| 26 + ch | W | Mainline: per-channel DMA address, rewritten before every frame in its zero-copy path. On this card it falls inside the audio-address range above. | mainline | 0 |
| 28 | W | Line-in audio ring address (PCI ID 8591 only) | windows | |
| 32 + i | R | Bit 0: which half of buffer i the DMA engine is filling | driver, windows, mainline | toggles |
| 40 + i | R | Bit 0: HDMI audio toggle for channel i | driver, windows | |
| 44 | R | Bit 0: line-in audio toggle (PCI ID 8591 only) | windows | |
| 50 + i | R/W | Length of quarter i, in 16-byte units, computed from the *input* size. Windows makes quarters 0–2 each (w·h·2 / 8192, rounded down) × 2048 bytes and gives quarter 3 the rest, rounded up to 2 KiB. | driver, windows, mainline | |
| 88 | R | Device info:<br>- bits 15:8: FPGA version<br>- bits 23:16: sub-version<br>- bits 25:24: port ID<br>- bits 31:28: output format; with 1, the copy pitch is width × 12 / 8 instead of width × 2<br><br>Bits 7:0 are unexplained. Windows decodes it the same way as this driver. Mainline reads the version from bits 7:0 and the sub-version from bits 15:8, which would give 255 and 122 on this card. A mainline port (stage 4.6) must use this driver's layout for 8581. | driver, windows, mainline | 0x00007AFF: version 122, sub-version 0, port 0, format 0 |
| 90 + 2·ch | R | Input size: width in bits 15:0, height in bits 31:16. The height is per field when the input is interlaced.<br><br>Windows accepts progressive input up to 4096 × 2200, and interlaced input only as 1920 × 540, 720 × 288 or 720 × 240 fields. After a size change it waits 100 ms before reprogramming the quarter lengths. | driver, windows, mainline | 3840×2160 |
| 91 + 2·ch | R/W | Picture: saturation << 24, hue << 16, contrast << 8, brightness. Hue reads back but has no visible effect.<br><br>Windows writes this register only on FPGA versions above 122. On 122 and earlier it applies brightness, contrast and saturation in software instead. This card is version 122, and writing the register visibly changes its image. | driver, windows, mainline | |
| 110 + ch | R | Input frame rate in whole fps. Windows reads it on FPGA 122 and later. | windows, mainline | 60 |
| 120 + ch | R/W | Output size (h << 16 \| w). On FPGA 122 and later, Windows writes the app's requested size here on every status poll, as mainline does. Probe 4.3: writes read back but change nothing observable (input size, input rate, delivered frames). | windows, mainline, measured | 1920×1080 |
| 130 + ch | R/W | Output frame rate. On FPGA 122 and later, Windows writes the app's requested rate here on every status poll and doesn't skip frames itself. Probe 4.3: writing 30 left the frame interrupt rate at 60 fps. | windows, mainline, measured | 60 |

## Not yet known

- What bits other than those listed do in registers 0, 1 and 0x144.
- What register 4 holds; Windows only logs it.
- Bits 7:0 of register 88, which read 0xFF here.
- Every register not listed above, notably 54–87 and 92–109. The Windows driver touches none of them.
- Whether the card gives the host any access to its HDMI receiver: EDID, InfoFrames, audio format. The Windows driver has no such access (see below).

## Windows driver

Package `VC12-4K_WIN10_LOW_20260317.zip` from AVMatrix:
- `VC12-4K.inf`, DriverVer 03/17/2026, 6.0.0.4;
- `hwsuhdx1pro.sys` for x64 and x86;
- the `DrvInstall` wizards, which only install the INF.

The `.sys` version resource reads "MV360", "AHD PCI-E DS", `mv360hws.sys`, 6.0.0.1, so it's a generic code base shared with other capture products. It was analysed statically with REA (Ghidra 12.1.4), and nothing was run.

What it shows beyond the table above:
- **Devices.** One INF and one driver cover PCI IDs 8888:8581 (this card), 8888:8591, 8888:85A1, 1F33:8581 and 1F33:8591. The 8591 adds a line-in audio input (the `LineIn%d` interface).
- **Formats.** It offers YUY2 only, in 47 modes from 640×480 to 4096×2160. Rates go up to 60 fps at 4K and 2560×1440, 144 fps at 1440p, and 240 fps at 1080p and 720p. It also offers 3440×1440, 2560×2048 and 2560×1920.
- **Audio.** 48 kHz, 16-bit, stereo only.
- **Scaling and rates.** When the app wants the input size, a frame is copied once into the app's buffer. Otherwise it's copied to a staging buffer and scaled on the CPU, as in this driver. There is no zero-copy path.
- **Buffers.** Four 5 MiB common buffers, with no check for crossing a 512 MiB window. Each keeps its audio in the last 40 KiB: HDMI audio as two 4096-byte packets at 40 KiB from the end, and line-in at 20 KiB from the end of buffer 0. This driver reserves 10 KiB, which holds the two HDMI audio packets.
- **Controls.** The only property set is VideoProcAmp:
  - brightness, contrast, hue, saturation: 0–255, default 128;
  - sharpness: 0–255, default 0, stored but never applied.

  A leftover VideoEncoder property set has two 1–240 values (default 60) that nothing uses. There's no video-decoder status, crossbar or vendor-specific property set.
- **Status polling.** A system thread polls the input every 50 ms; this driver polls every 250 ms. With no signal, every tenth poll wakes each open stream to deliver a "No Video" frame. The same thread tears the device down after a surprise removal.
- **Interrupts.** Windows handles only channel 0. In its status poll and interrupt handler, it reads back every register it writes and treats 0xFFFFFFFF as a removed card.
- **Absent.** No EDID, I²C, InfoFrame, HDR, audio-format or temperature access. Every BAR0 offset the driver uses is in the tables above.

## Hardware

Read from photos of both sides of a VC12-4K with the heatsink off (PCB `VC12-4K_V1.1`, dated 2023-10-24, week 2344; label serial VC12D230911002), 2026-10-19:

| Part | Marking | Role |
|---|---|---|
| FPGA | Xilinx Artix-7 XC7A35T, FGG484, date code 2313 | PCIe bridge, DMA engines, the capture block registers |
| HDMI chip | GSCoolink GSV2011, date code 2302 | HDMI 2.0 transceiver between the HDMI in and loop-out connectors and the FPGA |
| SPI flash | Macronix MX25L3233F (32 Mbit, 4 MiB), on the back | Almost certainly the FPGA configuration bitstream |
| 48-pin QFP | No visible marking | Probably a microcontroller that configures the GSV2011 |
| SOIC-8 by the QFP | Not legible in the photos | Unknown; possibly the microcontroller's memory or an EEPROM |
| SOIC-8, back, near the edge connector | No visible marking | Unknown |
| Small QFN/SOT parts | MPS and others | Power regulators |

No DRAM is visible on either side.

What follows from the parts, as inferences:
- **The FPGA can't buffer frames.** The XC7A35T has 1.8 Mbit of block RAM (about 30 lines of 4K YUYV), and there is no external memory. Every frame goes straight to host memory, which is why the frame arrives as four quarters through four DMA buffers. A scaler that only needs line buffers would fit; one that needs whole frames would not.
- **HDMI reaches the FPGA as a parallel bus.** The XC7A35T has 4 GTP transceivers, and PCIe Gen2 x4 uses all of them. So HDMI can't be terminated in the FPGA: the GSV2011 receives it and passes video to the FPGA over the many parallel traces between them. A reseller page describes the GSV1011, its pin-compatible HDMI 1.4 sibling, as having an LVDS/TTL video bus and I²S/SPDIF/TDM audio.
- **EDID, InfoFrames, HDCP and colour handling live in the GSV2011.** The same page lists colour-space conversion, deinterlacing and down-conversion for the GSV1011. They are configured by whatever programs the GSV2011, probably the unmarked microcontroller, and not by the host. The Windows driver has no I²C or EDID access, and no host register for these was found. The fixed full-range output (stage 4.5) and the 1920×540 interlaced field format are presumably set there.
- **The GSV2011 has public specification excerpts, without the register map.** GSCoolink's *GSV2011 Product Specification* Rev. 1.0 (4 pages) and Rev. 1.4 (8 pages, of 53) are published by the distributor Onway Tech. They describe:
  - an HDMI 2.0b / 1.4b repeater (receiver plus loop-out transmitter), controlled over I²C (SCL/SDA);
  - HDCP 1.4 and 2.2/2.3;
  - 600 MHz pixel clock, 4K60 4:4:4 8-bit;
  - a bidirectional LVDS bus (up to 15 pairs, 1.5 Gbit/s per lane) or a 48-bit TTL bus, "compatible with series FPGAs' LVDS standard";
  - a colour-space converter, dither, 4:2:0↔4:4:4 conversion, a deinterlacer and a **downscaler** ("this mode can also downscale all the 4K to 2K for LVDS/TTL output");
  - HDR10/HDR12/Dolby Vision/HLG support;
  - audio extraction up to 8-channel I²S or TDM at 192 kHz, or 2-channel S/PDIF.

  The EDID, register and control chapters are not in the excerpts. The card uses a fraction of this: it delivers 8-bit 4:2:2 and 48 kHz stereo, presumably as the microcontroller configures it.
- **Registers 120 and 130 may drive the GSV2011's downscaler.** The FPGA has no frame memory, but the HDMI chip can downscale 4K to 2K before the video reaches it. The Windows driver writes the requested size to register 120 every poll, yet sizes the DMA from the input-size register. Both fit if the microcontroller reads register 120, sets up the downscaler, and the input-size register then reports the smaller size. Stage 4.3's probe tests this: write 1920×1080 to register 120 during a 4K stream and watch whether register 90 changes.

## EDID

Read 2026-10-19 by connecting a PC's HDMI output to the card's input (`/sys/class/drm/card1-HDMI-A-2/edid`, 256 bytes, decoded with `edid-decode`). This is what the card tells a source it accepts. It probably lives in the microcontroller's firmware; the host has no way to change it.

| Field | Value |
|---|---|
| Identity | Manufacturer "UHD", model 8193, made week 30 of 2020, name "UHD"; EDID 1.3 plus one CTA-861.3 block |
| Preferred mode | 3840×2160 at 60 Hz (594 MHz) |
| Other detailed modes | 1920×1080 at 120 Hz, 2560×1440 at 59.95 and 144.01 Hz (595.5 MHz), 1920×1080 at 239.96 Hz (571 MHz) |
| CTA video modes | 3840×2160 at 24/25/30/50/60 Hz; 4096×2160 at 25/30/50/60 Hz; 1920×1080 at 24/25/30/50/60/120 Hz and 1080i50/60; 2560×1080 at 60 Hz; 1280×720 at 50/60 Hz; 720×576, 720×480, 640×480, 576i, 480i. **No 2560×1440 at 120 Hz.** |
| Range limits | 50–240 Hz vertical, 30–222 kHz horizontal, 600 MHz pixel clock |
| HDMI | HDMI 2.0 (HF-VSDB): 600 MHz TMDS character rate, SCDC; deep colour 30/36-bit 4:4:4 and 10/12-bit 4:2:0; 4:2:0 at 4K50/60 |
| VRR | VRRmin 4 Hz, VRRmax 120 Hz |
| Colour | YCbCr 4:4:4 and 4:2:2; BT.2020 YCC and RGB colorimetry; HDR static metadata: SDR gamma and SMPTE ST 2084 (PQ) |
| Quantization | RGB and YCbCr quantization both selectable (QS = 1, QY = 1): the source chooses the range and signals it in the AVI InfoFrame |
| Audio | LPCM only: 2 channels, 48 kHz, 16/20/24-bit |

What follows:
- **High refresh rates.** Sources can pick 1080p120 (VIC 63), 1080p240, 1440p144 and 4096×2160p60, but not 1440p120 (stage 4.8).
- **Colour range.** Because the range is selectable, the console decides it. The full-range result with the Switch 2 on Automatic is probably the console's choice passed through, not something fixed in the card (stage 4.5).
- **HDR and VRR.** A console with HDR on will send PQ/BT.2020, and one with VRR on will send variable frame timing. The card delivers 8-bit 4:2:2 and the driver cannot see the InfoFrames, so HDR shows washed out, and the driver's fixed-rate assumptions don't hold under VRR. Turn both off on the console for capture (H.1).
- **Audio.** Sources send 48 kHz stereo at most. Multichannel console audio would need a different EDID, which the host can't set.

## Vendor ancestry

This driver's names come from Windows:
- `DispatchCreate`;
- the `StartKSThread` kernel-streaming thread, a name the Windows driver also prints;
- `READ_REGISTER_ULONG` and `WRITE_REGISTER_ULONG`, borrowed from the Windows HAL.

The Windows driver's probe, start, stop and DMA-address sequences match this driver's register for register. That confirms they share a code base.
