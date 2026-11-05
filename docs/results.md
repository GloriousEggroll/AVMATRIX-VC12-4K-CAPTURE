# Test results

Results for each stage of [SPEC.md](SPEC.md), from `tests/run-baseline.sh` and `tests/framecheck.py`.

Setup: AVMatrix VC12-4K (FPGA 122) in PCIe 2.0 x4, Ryzen 9 9950X3D, kernel 7.2.10-200.nobara.fc44. Source: Nintendo Switch 2 sending 3840×2160 at 60.00 fps, progressive.

## Stage 0: baseline

Driver `f40a4d5` (upstream `bd17e06` plus the audio BATCH fix), module srcversion `067B017057D5C16BA6B3FCD`. Run on 2026-10-19 with no other app using the card.

### Frame delivery

| Run | Delivered fps | Unique frames | Same as previous | Delta median / max (ms) | Bursts (< 4 ms) |
|---|---|---|---|---|---|
| 1920×1080 (scaled), 4 buffers, 300 frames | **241.87** | 76 / 300 | 224 | 1.09 / 13.88 | 225 |
| 1920×1080 (scaled), 1 buffer, 300 frames | 60.00 | 300 / 300 | 0 | 16.67 / 16.83 | 0 |
| 3840×2160 (native), 4 buffers, 120 frames | **92.72** | 79 / 120 | 41 | 15.62 / 16.80 | 42 |

With 4 buffers, the driver fills every queued buffer with the same frame within about 1 ms per buffer. That is the copy and scale time, during which interrupts are off. The native-4K run shows fewer duplicates only because the reader (hashing 16.6 MB frames) requeues more slowly. With one buffer, delivery is clean at 60.00 fps.

### v4l2-compliance (no streaming tests)

`v4l2-compliance` **hung in the buffer tests**. Kernel stacks showed:
- one thread blocked in `vb2_core_dqbuf`, called through `hws_unlocked_ioctl`, so it holds the device-wide `ioctl_lock`;
- a second thread in `D` state in `hws_unlocked_ioctl`, waiting for that lock.

This is a deadlock. Any app that waits for frames in one thread while another thread controls the device can hang the same way. Killing the process with `SIGKILL` releases it. Spec item 1.9.

Failures reported before the hang:

- `missing bus_info prefix ('HWS')`, twice. Stage 2.6.
- `invalid std` and `invalid attributes for input 0`. Stage 2.1 (DV timings replace the TV standard).
- `expected to find 0 standard controls, got 4` and a missing control in the control map. Stage 2.5 (control framework).
- `has_frmintervals && !cap->capability`: `G_PARM` doesn't report `V4L2_CAP_TIMEPERFRAME`. Stage 2.4.
- `q2.reqbufs(node2, 1) != EBUSY`: a second file handle may allocate buffers while another owns some. This is **by design**: the driver lets several apps capture at once. It is expected to remain.

### Other

| Check | Result |
|---|---|
| "StartKSThread task" state | `D` (counts toward load average) |
| Load average during the run | 3.21 2.73 2.09 |
| `IO_PAGE_FAULT` this boot | 20, unchanged by the video-only test captures. The earlier bursts happened when OBS started and stopped, which also starts and stops audio capture. That points at the audio DMA engine (stage 3.5). |
| 10-minute OBS session | Not yet run (needs the operator). |

## Stage 1: `fix/video-delivery`

Driver `7cc4188`, module srcversion `659BAD3FE2AD102F241DCBD`, after a reboot on 2026-10-19 with no other app using the card. dmesg: `DMA buffers: 4 x 4608 KiB, input up to 4096x2160`.

### Frame delivery (same runs as the baseline)

| Run | Delivered fps | Unique frames | Delta median / max (ms), stdev | Bursts | Baseline fps |
|---|---|---|---|---|---|
| 1920×1080 (scaled), 4 buffers | **60.00** | 285 / 300, then 300 / 300 on a repeat | 16.67 / 20.03, 0.27 | 0 | 241.87 |
| 1920×1080 (scaled), 1 buffer | 60.00 | 300 / 300 | 16.67 / 17.96, 0.19 | 0 | 60.00 |
| 3840×2160 (native), 4 buffers | **60.00** | 120 / 120 | 16.67 / 16.68, 0.00 | 0 | 92.72 |

Every frame has its own sequence number, with no gaps and steady 16.67 ms spacing, so the driver delivers each captured frame once. The repeats in the first 1080p run came from the source. The repeat run was 300 / 300.

### Other checks

| Check | Result |
|---|---|
| Two `v4l2-ctl` streams at once (4 buffers each) | Each 60.00 fps, 300 / 300 unique, no sequence gaps |
| `S_FMT` 3840×2160 from a third process while both stream | `Device or resource busy`; the format stays 1920×1080 |
| 200 open/stream/close cycles beside a 2400-frame stream | 200 / 200 succeeded. The long stream held 60.00 fps with no gaps or bursts, and dmesg had no warnings. Its 490 / 2400 unique frames reflect a static screen during the 40-second run. |
| `v4l2-compliance` (no `-s`) | **Completes** (baseline deadlocked). 48 tests, 41 passed, 7 failed (8 failure lines; `bus_info` is reported twice). All are stage 2 items (`bus_info`, std, input attributes, two control checks, the `G_PARM` capability) except `reqbufs != EBUSY`, which is by design. |
| "StartKSThread task" state | `I` (was `D`) |
| `IO_PAGE_FAULT` this boot | **0**, including OBS starting with console audio running. Before stage 1, every OBS start or stop produced a burst at IOVA 0xfec00000. The change to 4.5 MiB buffers (1.6) moved the buffer layout, so this is a lead for stage 3.5, not proof of a fix. |
| OBS session (operator, ~10 min, 4K source, console audio on the direct path) | The operator reported everything fine. Afterwards PipeWire ERR counters were **0** on the headphone output, the capture card, and the console-audio loopback. Load average 0.51 with OBS running. |

## Stage 2: `feat/input-status`

Driver `5af2e2b`, module srcversion `3A9DCD14A99FB5E1BFA5477`, after a reboot on 2026-10-19, checked while OBS was capturing 3840×2160.

| Check | Result |
|---|---|
| `--query-dv-timings` | 3840×2160 progressive, 594 MHz, 60.00 fps, CTA-861 (VIC 97) |
| `--list-inputs` | `HDMI`, status ok, capability DV timings (was a camera input with an NTSC standard) |
| `--get-parm` | 60/1 with the `timeperframe` capability (measured rate) |
| `--list-formats-ext` | Native 3840×2160 listed first; intervals 60, 30, 20, 15 fps (was 15–240 fps for every size) |
| `bus_info` | `PCI:0000:0b:00.0` |
| `--log-status` | Prints input timings, the measured 1/60 s interval, output size, frames (3673) and drops (0), open and streaming handles, and controls |
| OBS | Shows the DV timings list and the controls |
| Brightness, contrast, saturation | Change the image (operator) and read back in register 91 |
| Hue | Register 91 reads back the written value, but the image does not change. The control is removed and the byte stays at 128 (commit after `5af2e2b`). OBS had saved hue = 0 from the old driver, whose controls did nothing, and re-applied it to the hardware. |
| Source-change events (operator switched the Switch 2 to 1080p) | Signal loss raised an event about 1 s later (QUERY: `ENOLINK`, input: no signal); the 1080p lock raised another about 0.5 s later (1920×1080, 148.5 MHz, 60.00 fps). OBS followed with its DV Timing set to 1920×1080. OBS keeps its own DV Timing selection, so it must match the console's output. |

### After the fixes found during stage 2 (driver `f3da7f7`, srcversion `F0800C600D65554C0E507E3`)

Testing found and fixed four more problems:
- **Release ordering:** a closing handle was counted closed before its queue was released, so a new opener could reset the format and then get `EBUSY`.
- **DV timings filter:** enumerated timings the driver could not deliver were rejected by `S_DV_TIMINGS`; interlaced ones are now excluded too, while the live input's own timings are always accepted.
- **Frame-rate measurement:** it read a missed frame-end interrupt as a lower rate (60 → 59 → 58 fps across short captures), so `QUERY_DV_TIMINGS` synthesized odd timings and `v4l2-ctl` reset the format.
- **Suspend and resume (2.7):** after S3 the card's registers were wiped and a running capture stayed dead.

| Check | Result |
|---|---|
| Frame delivery (`run-baseline.sh`) | 60.00 fps in all three runs, no bursts or gaps; 4K native 120 / 120 unique, delta stdev 0.00 ms |
| Back-to-back 1080p runs (4 and 1 buffers, ×2) | Measured rate stays 60.00; format stays 1920×1080 every run (before the fix: 59, 58 fps and 4K frames) |
| `S_PARM` 30 on a 60 fps source | 30.00 fps, deltas 33.33 ms, stdev 0.00; skipped frames show as sequence gaps |
| `v4l2-compliance` (no `-s`) | **47 / 48** (baseline: deadlock). Only `reqbufs != EBUSY` (multi-reader by design) fails. One warning: no `V4L2_CID_DV_RX_POWER_PRESENT` control. The blocking-wait test passes. |
| Suspend with a capture running | The kernel logged `resumed, card re-initialised`, and the same `v4l2-ctl` capture continued at 60 fps after the wake (4976 → 5155 frames in 3 s). No reboot, no `IO_PAGE_FAULT`. |
| Status thread, IOMMU faults | `I` state; 0 faults |

Correction to the first suspend diagnosis: after the unplanned suspend, starting a new stream did not recover the card. That was because the Switch 2 was not sending a signal at the time (status register 5 read 0); the card appears to ignore register writes without HDMI input. Once the console was restarted, the existing re-init on stream start recovered it. The suspend callbacks are still needed for captures that are running across a suspend, as the test above shows.

### Follow-up checks (driver `b8cb4e6`, srcversion `1DD79E963BB11356BDB7BF8`)

| Check | Result |
|---|---|
| `V4L2_CID_DV_RX_POWER_PRESENT` | Present as `power_present`, read-only bitmask, value 1 with the Switch 2 connected |
| Output quantization (5 frames with content, Switch 2 RGB Range: Automatic) | Y spans 1–254 with values below 16 and above 235, so the card delivers **full-range** YUV. The driver still reports the default (limited) quantization; OBS's Color Range should be Full. |

Registers documented by the mainline `hws` driver (drivers/media/pci/hws/hws_reg.h, for AVMatrix's multi-channel cards), read on this VC12-4K:

| Register | Mainline meaning | Value here |
|---|---|---|
| 8 | HDCP detected (bit per channel) | 0 (Switch 2, no HDCP) |
| 9 | DMA max size | 0x00044000 |
| 25, 26 | buffer base / per-channel DMA address (zero-copy doorbell in mainline) | 0, 0 (unused by this driver) |
| 110 | input frame rate | 60 (integer fps) |
| 120, 130 | programmed output size and rate | 1920×1080, 60 (never written by this driver; frames still arrive at 3840×2160) |

## Stage 3: `rework/single-copy`

Driver `fa5b301`, module srcversion `E3F8F6E0072AD04809ACE89`, after a reboot on 2026-10-19 with no other app using the card. dmesg: `DMA buffers: 4 x 4608 KiB, input up to 4096x2160`, no warnings, 0 `IO_PAGE_FAULT`.

| Check | Result |
|---|---|
| Frame delivery (`run-baseline.sh`) | 60.00 fps in all three runs, every frame unique, no sequence gaps or bursts; 4K native delta stdev 0.00 ms |
| `v4l2-compliance` (no `-s`) | 47 / 48; only the by-design `reqbufs != EBUSY` |
| Two 4K streams at once | Each 60.00 fps, 300 / 300 unique, no gaps; both consumers got the same frames |
| `S_FMT` from a third process during both | `Device or resource busy` |
| 200 open/stream/close cycles (1–4 buffers, 1–5 frames) beside a 2400-frame 4K stream | 200 / 200 succeeded; the long stream held 60.00 fps with no gaps; 0 frames lost; no kernel warnings |
| `S_PARM` 30 | 30.00 fps, deltas 33.33 ms |
| IRQ thread CPU, 600 frames | 4K native: 300 ms (0.5 ms per frame). 4K input scaled to 1080p: 730 ms (1.2 ms per frame). Before: about 1.3 ms in the tasklet plus 1.3–1.9 ms per buffer in the worker, partly with interrupts off. |
| IRQ thread | `irq/202-0000:0b`, SCHED_FIFO 50, on CPU 4 (follows the IRQ affinity) |
| Status thread, IOMMU faults | `I` state; 0 faults |

debugfs `dma`: every video address register reaches its own buffer. Buffer 2 spans IOVA 0xfe800000–0xfec7ffff, so 0xfec00000, where the stray writes faulted before stage 1, is now inside it, in the unused gap past the 4K quarter. The faults may have stopped only because that address became mapped. `8a67155` adds a canary in each buffer's gap to show whether the card still writes there.

An OBS start before the console's signal had locked opened at the 1920×1080 default and kept it once the 4K signal arrived, scaled to 1080p and drawn as a quarter of the 4K canvas. Reopening the source fixes it; H.3 automates that.

### Operator session (driver `8a67155`, srcversion `C7DCAC889AAA7812B57DE3F`)

A 15-minute OBS session at 3840×2160 with console audio, two quick OBS restarts, a suspend with OBS closed (11:28:54–11:29:51, `resumed, card re-initialised`), and OBS reopened at 4K after the wake.

| Check | Result |
|---|---|
| `IO_PAGE_FAULT` | 0 across all of it |
| Canary in each DMA buffer's gap | Untouched in all four buffers, including buffer 2 around 0xfec00000. This build refilled it at every capture start, so it covered only the last OBS start and stop; `df0a168` keeps it across starts. |
| debugfs status | 0 half-queue overflows; 1 lost frame (a bottom half without its top half at a capture start) |
| Measured rate | 16.667 ms on a fresh capture. The value left after the last short OBS session read 17.8 ms; it is only updated while capturing. |

### Operator session 2 (driver `df0a168`, srcversion `47AF3E12696B71724ED6FB9`)

Three OBS starts and stops, then a suspend with OBS capturing (11:35:53–11:36:14).

| Check | Result |
|---|---|
| Suspend with capture running | The picture came back by itself after the wake (operator); `resumed, card re-initialised` |
| Console audio, Switch 2 menu clicks | Sound right, clicks on time (operator); PipeWire ERR 1 on the console-audio loopback |
| OBS picture | Fills the 4K canvas (operator) |
| `IO_PAGE_FAULT` | 0 |
| Frames | 5498, lost 2; the consumer had no buffer queued for 16 |
| Canary | Buffers 0–2 untouched. **Buffer 3: 2 KiB written at offset 0x440000–0x4407ff (IOVA 0xfe440000–0xfe4407ff)**, beyond the 4K quarter (0x3f5000) and before the audio ring. 0x440000 is register 9 (0x44000) × 16. |

The canary covered the whole boot, so the write could come from any of the starts, stops, the suspend or normal streaming. `bf69f64` logs each hit with its context.

### Operator session 3: when the stray write happens (driver `bf69f64`, srcversion `608378952E69078C5C870C5`)

The status thread logged two canary hits, both in buffer 3 at offset 0x440000–0x4407ff, at frames 9 and 24 of the first capture after boot (157 and 413 ms after audio enable). There were none during minutes of streaming, at two later OBS restarts, or at a suspend and resume with capture running. 0 `IO_PAGE_FAULT`.

The kernel logs of two boots before stage 1 hold 10 and 20 faults: 256-byte writes climbing from 0xfec00000 to 0xfec01000 and beyond, one DMA run off the end of a 4 MiB buffer at 0xfe800000.

Diagnosis: until the first status poll after a capture start, the DMA quarter lengths were still those of the 1920×1080 probe default, so the card ran the 4K quarter on to register 9 × 16 (0x440000) and ended with a 2 KiB block there. With 4.5 MiB buffers that is inside the buffer; with the old 4 MiB buffers it was past the end, which produced the faults. `7df0340` programs the lengths before the engine starts and sets register 9 so a run ends inside the buffer.

### Operator session 4: the fix (driver `7df0340`, srcversion `CD2364BD0210131C9174DE0`)

OBS started after boot, then the Switch 2 was switched to 1080p and back to 4K while OBS captured.

| Check | Result |
|---|---|
| First capture start after boot | **No canary hits** (before the fix: the 2 KiB block at 0x440000 every time) |
| Register 9 | 0x47d00: an over-long run stops at offset 0x47d000 |
| Switch to 4K while capturing | The 4K frame DMA'd with the 1080p lengths ran on in buffer 3 to offset 0x47d7ff and stopped there: register 9 × 16 plus the final 2 KiB block, ending where the audio ring starts. Before the fix the same run ended at 0x4407ff. |
| Other hits at the switches | Only inside the buffers: the last 4K frames written after the switch to 1080p (offsets up to 0x3f47ff, the end of a 4K quarter), and 2 KiB blocks just past each quarter's data |
| `IO_PAGE_FAULT` | 0 |
| Frames | 3724, lost 2 (at the switches), no buffer missing for the consumer |

So the card's over-long runs end at register 9 × 16 + 2 KiB, and with register 9 set they stay inside the buffer at every resolution change, also with the 4 MiB fallback buffers (limit 0x3fd000 + 2 KiB, below the audio ring).

## Stage 4.8: input mode survey with a PC source

Driver `7df0340` (DKMS `1.0.0.230324.3.0`). The PC's GPU (AMD, HDMI-A-2) drove the card's input; each mode was set with `kscreen-doctor` and captured at its native size, 4 MMAP buffers, for about 2 s. The desktop on that output was mostly static, so only timing and sequence numbers matter here. 2026-10-19.

| GPU mode | Driver input | Delivered fps | Delta median / stdev (ms) | Sequence gaps |
|---|---|---|---|---|
| 1920×1080 @ 239.96 | 1920×1080 | **239.96** | 4.17 / 0.00 | 0 |
| 2560×1440 @ 144.01 | 2560×1440 | **144.00** | 6.94 / 0.00 | 0 |
| 1920×1080 @ 120.00 | 1920×1080 | **120.00** | 8.33 / 0.00 | 0 |
| 1920×1080 @ 119.88 | 1920×1080 | **119.88** | 8.34 / 0.00 | 0 |
| 3840×2160 @ 60 | 3840×2160 | 60.00 | 16.67 / 0.00 | 0 |
| 3840×2160 @ 50 | 3840×2160 | 50.00 | 20.00 / 0.26 | 0 |
| 3840×2160 @ 30 | 3840×2160 | 30.01 | 33.33 / 0.28 | 0 |
| 2560×1440 @ 59.95 | 2560×1440 | 59.95 | 16.68 / 0.22 | 0 |
| 2560×1080 @ 60 | 2560×1080 | 60.00 | 16.67 / 0.08 | 0 |
| 1920×1080 @ 60 | 1920×1080 | 60.00 | 16.67 / 0.21 | 0 |
| 1280×720 @ 60 | 1280×720 | 60.00 | 16.67 / 0.16 | 0 |
| 4096×2160 @ 60 | **3840×2160** | — | — | — |

- **High refresh rates already work.** Capture at 120, 144 and 240 fps needs no driver change.
- **4096×2160 is scaled by the card.** With the GPU sending 4096×2160p60 (594 MHz, h-total 4400), register 90 reads 3840×2160 and register 110 reads 61. A captured frame shows the whole desktop scaled to 3840 wide and letterboxed (black bars about 67 px top and bottom, matching 3840×2025). The scaling happens before the FPGA, presumably in the GSV2011. The driver never sees 4096-wide frames from this card.
- **DV timings go stale after a mode change.** With no stream running, `QUERY_DV_TIMINGS` reported each new mode with the previous mode's frame rate (for example 2560×1440 as "30.00 fps", 110.6 MHz synthesized). The measured rate only updates while capturing. Stage 4.2 fixes this.
- **Stray DMA.** The only canary hits were at the moment the cable was switched from the console to the PC with OBS capturing (12:35:20). Both were inside the buffers: the end of a 4K quarter in buffer 0, and a run in buffer 3 that stopped at the register 9 limit. 0 `IO_PAGE_FAULT`.

## Stage 4 round 1 (`feat/hw-status`)

Driver `46f7ec9` (DKMS `1.0.0.230324.4.0`, srcversion `78ADE6E8CAD81BA4F1E78AA`), after a reboot on 2026-10-19 with the Switch 2 at 3840×2160p60.

| Check | Result |
|---|---|
| Frame delivery (`run-baseline.sh`) | 60.00 fps in all three runs, every frame unique, no gaps; 4K native delta stdev 0.00 ms |
| `v4l2-compliance` (no `-s`) | 47 / 48 (the by-design `reqbufs != EBUSY`) |
| Status thread, IOMMU faults | `I` state; 0 faults |
| 4.2, before any capture this boot | `--query-dv-timings` 3840×2160 at 60.00 fps and `--get-parm` 60/1, from register 110 (no measurement yet) |
| 4.1, Switch 2 (no HDCP) | Register 8 reads 0; input status ok; debugfs `HDCP: not active`. The PS5 test is still to do. |

### 4.5: colour range from the Switch 2

Three 3840×2160 frames of the home screen per setting, Y (luma) statistics:

| Switch 2 RGB Range | Y min–max | 0.1% / 99.9% points | Below 16 | Above 235 |
|---|---|---|---|---|
| Automatic | 1–254 | 1 / 254 | 1.78% | 4.22% |
| Limited | 1–254 | 1 / 254 | 1.82% | 4.45% |
| Full | 1–254 | 1 / 254 | 1.36% | 4.21% |

The card delivers full range whatever the Switch 2 setting. Either the Switch 2 sends YCbCr, which its RGB Range setting does not affect, or the card's HDMI chip converts every input to full range. A PS5 with its RGB range set to Limited would tell the two apart. `96dd3f1` reports full-range Rec. 709 in the format.

(A first Limited reading taken on the TV Output settings screen gave 0.1% / 99.9% points of 17 / 246. That screen has no full black or white, so the reading reflected the content, not the range.)

### PS5: HDCP and colour range (driver `96dd3f1`, srcversion `72A67248544678E83B7D8D7`)

PS5 connected at 3840×2160p60 with OBS capturing. The operator turned Settings → System → HDMI → Enable HDCP off and back on, then set the RGB range to Limited and went to the home screen.

| Check | Result |
|---|---|
| HDCP on (setting) | The card delivered the normal home screen. Register 8 read 0 throughout, so the driver never blanked and logged no change. The Windows driver blanks on the same register, so it would not have blanked either. Whether HDCP was active on the link can't be seen: register 8 is the only HDCP status the card gives the host. |
| RGB range Limited, home screen (`tests/yrange.py`) | Y min 1, max 254; 59.2% of pixels below 16 (the black background at Y ≈ 1), 1.6% above 235 |
| Signal re-handshakes at each HDCP toggle | OBS logged 31 "select timed out"; each made OBS call `LOG_STATUS`, 293 status dumps this boot. Rate-limited in the next commit. Canary hits only at the register 9 limit, inside the buffers; 0 `IO_PAGE_FAULT` |

**4.5 settled:** a limited-range source has black at Y = 16, but the PS5 on Limited arrives with black at Y ≈ 1. So the card converts its input to full range, presumably in the GSV2011's colour-space converter, whatever the console sends. With the Switch 2 results, the format's full-range label is right for every source tested.

### 4.3: registers 120 and 130 (probe)

Driver `96dd3f1`, OBS closed, a 2700-frame 3840×2160 capture running as the observer, `tests/regprobe.py` (each value read back, restored on exit). 2026-10-19.

| Write | Reads back | Register 90 (input size) | Register 110 (input fps) | Frame interrupts |
|---|---|---|---|---|
| 120 = 3840×2160 | yes | 3840×2160, unchanged for 3 s | 60, unchanged | — |
| 120 = 1920×1080 | yes | unchanged | unchanged | — |
| 120 = 1280×720 | yes | unchanged | unchanged | — |
| 130 = 30 | yes | unchanged | unchanged | 121 halves/s before and during, 120 after restore (60 fps) |

The observing capture completed all 2700 frames, 3 lost; no canary hits, 0 `IO_PAGE_FAULT`. Register 120 also holds 1920×1080 from reset while the input arrives at 4K across every signal re-lock. **On this card (FPGA 122) registers 120 and 130 are storage with no observable effect**; the card's own scaling (4096×2160 to 3840×2160) is not controlled from the host. The driver keeps its CPU scaler.

### 4.9: 50 ms poll and the LOG_STATUS rate limit (driver `2ef336a`, srcversion `DD6C962F9203FA1AE6011BA`)

OBS open; the console switched 4K to 1080p and back, then went into rest mode and woke.

| Check | Result |
|---|---|
| OBS "select timed out" | 25, in two bursts of about 12 (about 1 s each, 13:22:55 and 13:23:49–50); none at the other changes. Before: 31 in this kind of session. |
| Status dumps | 25 `LOG_STATUS` calls from OBS; the driver printed its part about twice (2 s rate limit). The core's two-line START/END banner still appears per call. Before: 293 full dumps. |
| Why the bursts remain | During them the card still reported the signal present with the old timings (Power Present 1), so no no-signal frames were sent; it just delivered no frames for about a second while the console renegotiated. The faster poll can't help there. A watchdog sending blue frames would replace OBS's held last frame with a blue flash, so it was not added. |
| Frames, faults | 7372 frames, 2 lost; canary hits only at the changes, all inside the buffers; 0 `IO_PAGE_FAULT` |

### 4.8: 120 Hz from the PS5 (driver `2ef336a`)

PS5 with 120 Hz Output on Automatic, running Hogwarts Legacy with its 120 Hz option and a high-frame-rate mode. The PS5 switched its output to 1920×1080 at 119.88 Hz (this EDID offers no 1440p120 or 4K120). OBS canvas set to 120 fps.

| Check | Result |
|---|---|
| Input | `--query-dv-timings` 1920×1080p, 248.58 MHz, 119.88 fps; measured 8.342 ms; register 110 reads 120 |
| OBS source still at 3840×2160 (opened at 4K60) | 120 fps delivered, the driver upscaling each 1080p frame on the CPU: IRQ thread 530 ms CPU per 5 s (0.88 ms per frame) |
| OBS source reopened (properties → OK) | OBS log: "Resolution: 1920x1080, Framerate: 120.00 fps". 602 frames in 5 s (120 fps), none missed; IRQ thread 80 ms CPU per 5 s (0.13 ms per frame) |
| Lost frames | 3 this boot, at mode switches |

OBS's DV Timing list stops at 60 Hz because the kernel's standard timings (`v4l2_dv_timings_presets`) go no higher at 1080p and have no 1440p; "Leave Unchanged" uses the live timings, which is what OBS needs. After the console changes mode, reopen the source so it captures at the new native size.

### 4.10: buzz during console mode changes (driver `1f3e39a`, srcversion `3ADD58FAC28F00B5A49A027`)

PS5 switched from 4K to 1080p and back, with OBS capturing and the console-direct loopback on. The card's PipeWire node was recorded with `pw-record`, and the driver's debugfs `status` was logged every ~30 ms. `tests/staleaudio.py` classified the audio.

**Before (driver `12f8c36`):** each switch buzzed for 3.4–3.9 s.
- **The buzz:** each sample matched the one 5120 frames earlier to within 5 LSB, slipping to 4864 about once a second: a loop of the last ~107 ms of audio.
- **Timing:** it started about 1 s before the driver reported "no usable signal".
- **Cause:** the debugfs counters show the card raised no audio interrupts (and no video) for the whole buzz, so the driver delivered nothing. The ALSA ring is 16384 frames, which doesn't match the loop. The replay comes from PipeWire re-delivering its last buffers while the ALSA pointer stands still.
- **Rejected first fix:** a driver-side mute of repeating content never fired, because the driver never sees the loop.

**After (silence fill):**

| Check | Result |
|---|---|
| Switch to 1080p | 3.46 s of digital silence; no replay loop; debugfs: 1 gap, 165 silence packets |
| Switch back to 4K | 4.01 s of digital silence; no replay loop; 2 gaps, 353 silence packets in total |
| Normal running | No gap filled before the first switch (1025 card packets) |
| By ear | No buzz at either switch |
| Kernel log | No `IO_PAGE_FAULT`. The canary logs in-buffer over-long quarters at the size changes, ending at 0x47d7ff, as on every stage 4 boot (5–29 per boot). |

With no HDMI source connected (operator check, 2026-10-19), the card's audio node delivers silence: nothing is audible, and there is no buzz.

### 4.4: zero-copy probe (driver `5c8ee4f`)

The probe build ran on branch `probe/zero-copy`; that code is debug-only and not in the stage branches.
- **Setup:** PS5 at 3840×2160p60, OBS closed. One `v4l2-ctl` stream at the input size, started by `tests/zcprobe.sh`. IOMMU domain `DMA-FQ`.
- **Method:** over seven frames the probe pointed registers 16–19 alternately at a 33 MB test buffer the driver allocated, and back at the normal DMA buffers, with the last frame going to the normal buffers. It programmed each pair of quarters right after the previous frame's half-done interrupt for those quarters. Before each frame it filled both possible destinations of each quarter with a canary, and after the half it counted the canary words left in each.

| Frame | Pointed at | Quarters 0–2 landed | Quarter 3 landed |
|---|---|---|---|
| 0, 2, 4 | test buffer | test buffer (0 canary words left of 1036800) | test buffer, except its last 512 words |
| 1, 3, 5 | normal buffers | normal buffers | normal buffers, except the last 512 words |
| 6 | normal buffers (restored) | normal buffers | normal buffers, except the last 512 words |

- **Quarter 3:** its programmed length, 4149248 bytes, is the 4147200 bytes of frame data rounded up to 2 KiB. The card wrote only the frame data, so the 2 KiB tail kept its canary in whichever buffer it was pointed at. In each frame the other buffer was untouched.
- **Addresses:** the test quarters started at T + 0, 0x3f4800, 0x7e9000 and 0xbdd800, so 2 KiB alignment is enough.
- **Windows:** all four translation entries read `00000000:e0000000` (one 512 MiB window at 0xE0000000), and the test buffer's IOVA was 0xEE000000, inside it. The table was never written.
- **Containment:**
  - The 16.6 MB guard after the test quarters was untouched.
  - After the restore, the test buffer stayed untouched, then and 1 s later.
  - No `IO_PAGE_FAULT`, and no stray-DMA canary hit.

So the card takes a new address for every frame and applies it to the next one. Zero-copy into app buffers can work on this card.

## H.2: IRQ placement on the 9950X3D (driver `1f3e39a`)

The PS5 sent 3840×2160p60 while OBS captured 3840×2160, so the IRQ thread copied about 1 GB/s. A pointer-chase benchmark ran one thread on each core of the V-cache CCD (CPUs 0–7), chasing a random cycle through a shared working set. Each configuration ran 15 s, three rounds alternating, with the card's IRQ pinned either to CPU 4 (V-cache CCD, where it booted) or to CPUs 8–15 and 24–31 (the other CCD). Without capture, the working set fits the 96 MiB V-cache up to about 80 MiB (684 M loads/s at 72 MiB, 323 M at 88 MiB).

| Working set | IRQ on CPU 4: M loads/s per round | IRQ on the other CCD: M loads/s per round |
|---|---|---|
| 64 MiB | 697, 125, 531 (per-second spread up to ±312) | 784, 786, 785 (±5) |
| 80 MiB | 649, 567, 579 | 718, 602, 685 |

- **Effect:** at 64 MiB, the copy through the V-cache CCD cut throughput by a third on average, and once to DRAM speed. With the IRQ on the other CCD, throughput per thread matched the run without capture (98 M loads/s).
- **The copy's own cost:** the IRQ thread used 0.80–0.89 s per 15 s on CPU 4 and 0.83–0.95 s on the other CCD.
- **Default placement:** with the default mask (all CPUs), the kernel put the IRQ on CPU 4 at boot, and on CPU 25 when the mask was rewritten. Which CCD it lands on is chance.
- **Caveat:** this is a worst case, a load that lives in the V-cache. Games gain less, by how much depends on the game.

**`tools/irq-affinity install`:**
- It pinned IRQ 202 to 8–15,24–31 and wrote a udev rule that reapplies the pin whenever the driver binds a device.
- It added an irqbalance drop-in with `--banmod=HwsUHDX1Capture`. irqbalance's control socket then listed 121 balanced IRQs, including the neighbours 200, 201 and 205, but not 202.
- With fake sysfs topologies the choice is: one L3 domain, no change; two equal domains (non-X3D or dual V-cache), no change; X3D, the smaller domain's CPUs.

After a reboot, the udev rule had pinned IRQ 202 to 8–15,24–31 at driver bind: the IRQ and its handler thread ran on CPU 9, and irqbalance started with `--banmod=HwsUHDX1Capture`. OBS captured normally.

### 4.4: zero-copy, round 1 (driver `821f2d1`, srcversion `3538EA2396BF2C18A6E6C22`)

PS5 at 3840×2160p59.94, OBS closed, IOMMU `DMA-FQ`. `tests/zctest.sh` ran 600 frames per case through `framecheck.py`. It switched the `zero_copy` parameter at runtime and read the IRQ thread's CPU time from `/proc` (10 ms resolution).

| Case | Delivery | IRQ thread CPU, 600 frames | Zero-copy frames |
|---|---|---|---|
| Copy path (`zero_copy=0`, MMAP) | 59.94 fps, 600 unique, no gaps, deltas 16.67–16.70 ms | 540 ms | — |
| Zero-copy (MMAP) | 59.94 fps, 600 unique, no gaps, deltas 16.50–16.87 ms | 0 ms | 597 (the first 3 waited for 2 stable frames) |
| Zero-copy plus a second MMAP stream | both 59.94 fps, 600 unique, no gaps | 570 ms (the second stream's copy) | 597 |
| USERPTR (copy path, vmalloc queue) | 59.94 fps, 600 unique, no gaps | 510 ms | — |
| 100 start/stop cycles, 1–7 frames each | none failed | — | 140 |

- **Correctness:**
  - No failed zero-copy frames, no late register writes, and no buffer outside the window.
  - Kernel log: no `IO_PAGE_FAULT`, no stray-DMA canary hits, no warnings.
- **Picture:** a frame captured by zero-copy and one by the copy path render identically: the full 4K home screen, no seams at the quarter boundaries. Mean luma per quarter matches, and no stale-half mark word is left in the picture.
- **Register 9:**
  - read 0x3F500 during 4K capture: the largest quarter, 4149248 bytes;
  - read 0xFE00 before the first stream: the 1080p default lengths set at load.

### 4.4: zero-copy, round 2 with OBS (driver `821f2d1`, `zero_copy=1` set before OBS started)

| Check | Result |
|---|---|
| OBS at 3840×2160 | Picture and console audio normal. 303 zero-copy frames per 5 s (60 fps); IRQ thread 0 ms CPU per 5 s, against about 270 ms on the copy path. OBS's consumer had no buffer missing. |
| PS5 to 1080p and back to 4K | Zero-copy stepped aside for the switch and came back by itself at 4K, again with 0 ms CPU. One zero-copy frame completed with an error (the frame in flight at a switch); no late writes. The canary logged two 2 KiB blocks in DMA buffer 0, at 0xfe000 and 0x3f5000. Those are register 9 × 16 for the 1080p and 4K quarters, the new cap. Before register 9 followed the quarter size, the same switches wrote runs up to 0x47d7ff. |
| Suspend and resume with OBS open | Card re-initialised and the picture came back. Zero-copy resumed with no failures or late writes. OBS's consumer counted 93 frames with no buffer queued, once, right after the wake-up, before OBS had re-queued its buffers. |
| Kernel log | No `IO_PAGE_FAULT`, no warnings |
