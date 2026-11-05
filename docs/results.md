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

## Stage 6: first session (driver `47006b7`, srcversion `ECF3B4B0DE67246B58F850D`)

DKMS version `1.0.0.230324.6.0` on 7.2.10, with `zero_copy=1` set in `/etc/modprobe.d/hws.conf` (6.1 makes it default off). Operator session on 2026-10-19: OBS capturing the Switch 2 at 3840×2160, the console switched to 1080p and back, then three suspend and resume cycles.

- **Operator:** picture and audio normal throughout, including after each resume. No audio problem at the resolution changes.
- **Kernel log, this boot:** no `IO_PAGE_FAULT`, no BUG or WARNING line, no "HDCP active" line across the three suspends (6.7), and no "zero_copy is on but unavailable" line (the 6.1 gate passes on this card). Each resume logged "resumed, card re-initialised".
- **debugfs `status`, after the session:**

| Counter | Value |
|---|---|
| Frames / lost | 6534 / 3 |
| Zero-copy frames / failed / late writes | 5887 / 0 / 0 |
| Interrupt sources other than bits 0 and 8 (6.14) | 0 |
| Frames failed for a late copy (6.15) | 0 |
| Audio gaps filled with silence | 3 |

- **`modinfo`:** `zero_copy` and `debug_dma` both describe themselves as default off.
- **`lspci -vv`:** `RlxdOrd+` on the card, now as the PCI core leaves it (6.12 removed the driver's write).
- **Seen in passing:** each rate-limited `LOG_STATUS` call still prints the V4L2 core's empty "START STATUS" and "END STATUS" lines. Cosmetic.
- **Not yet run:** the 6.2 simulated reset, the 6.3 SIGSTOP and start/stop tests, the 6.4 unbind test, the 6.6 and 6.5 test builds, `framecheck.py` runs, and the debug-kernel pass (6.16).

## Stage 6: automated checks (driver `47006b7`)

Run on 2026-10-19 with OBS closed, the Switch 2 sending 3840×2160p60 and `zero_copy=1`.

| Check | Result |
|---|---|
| `framecheck.py`, 4 buffers, 300 frames, 3840×2160 (zero-copy) | 60.00 fps, 300 of 300 unique, no gaps, deltas 16.66–16.68 ms |
| Same at 1920×1080 (CPU scaler) | 60.00 fps, 300 of 300 unique, no gaps |
| Two 4K streams at once, 300 frames each | both 60.00 fps, 300 of 300 unique, no gaps |
| Two 4K streams at once, 600 frames each, compared by sequence number | the frames of both streams are identical for every one of the 600 sequence numbers, and all 600 are unique |
| `tests/zctest.sh`, run twice | every case 60.00 fps with no gaps; IRQ thread 0 ms for zero-copy against 430–440 ms for the copy path; 100 start/stop cycles all passed; no `IO_PAGE_FAULT`, canary hit or WARNING |
| `v4l2-compliance -s` | 58 of 59; the failure is the by-design `reqbufs != EBUSY` for a second handle |
| 1,000 ALSA capture start/stop cycles of 512–2048 frames on `hw:0,0` | none failed; no BUG, WARNING or oops |
| 50 rounds: `arecord` stopped with SIGSTOP mid-capture, then killed | no oops; the card records normally afterwards |
| 9.5-minute 4K capture (34,200 frames) | 59.999 fps, no sequence gaps, deltas 16.45–16.88 ms, no frame lost; 34,197 zero-copy frames, 0 failed, 0 late writes; other interrupt sources 0; late copies 0 |

- **Unique-frame counts in `zctest.sh`:** the second run gave 401 of 600 unique frames in every case, including the copy path with zero-copy off. The source was repeating frames at that point. The per-sequence comparison shows the two consumers receive identical frames, so this isn't a driver effect.
- **Buffers outside the window:** debugfs counted 174 during the `zctest.sh` runs (0 before them). Those buffers used the copy path, which is the designed fallback, but zero-copy round 1 saw none. The debug-kernel pass below shows it comes from the 100 start/stop cycles.

## Stage 6: unbind test (6.4)

Run on 2026-10-19 with OBS closed. Commands: `echo 0000:0b:00.0 | sudo tee /sys/bus/pci/drivers/HwsUHDX1Capture/unbind`, and the same with `bind`.

- **Driver `47006b7`, nothing open:** unbind and bind returned at once; `/dev/video0` and the ALSA card went away and came back; the kernel log showed only the probe line.
- **Driver `47006b7`, a 4K zero-copy stream (`v4l2-ctl --stream-mmap=4`) and `arecord` running:** unbind returned at once, and `arecord` exited with "File descriptor in bad state". `v4l2-ctl` stayed blocked in DQBUF, because nothing woke it, until it was killed with SIGKILL. It then exited without a D state or any kernel message, so closing after remove is safe. Fixed in `5815209`: remove() calls `vb2_queue_error()` on every open queue.
- **Driver `5815209`** (DKMS `1.0.0.230324.6.1`, srcversion `6A47E2DE4A0F57F50595CB5`, loaded with `modprobe` while the card was unbound, no reboot): with both streams running, `v4l2-ctl` now gets `VIDIOC_DQBUF: Input/output error` and exits by itself.
- **20 cycles** of bind, start both streams, wait 1–2 s, unbind, on `5815209`: all 20 unbinds returned, every app exited by itself within 1 s, and the kernel log had no BUG, WARNING, oops, refcount, hung-task or `IO_PAGE_FAULT` line. After the last bind: 4K capture at 60.00 fps, 300 of 300 unique, no gaps, and `arecord` records. The IRQ placement helper (H.2) re-pinned IRQ 202 to CPUs 8–15 and 24–31 after each bind.
- **Not covered:** KASAN and kmemleak (not in this kernel; 6.16).

## Stage 6: debug-kernel pass (6.16)

The repositories carry no debug kernel, so one was built on 2026-10-19: `7.2.10-hwsdebug`, from the Nobara source package (the same CachyOS tree and Nobara patches as `7.2.10-200`), with the running config cut down by `localmodconfig`, plus `CONFIG_KASAN` (generic, inline), `CONFIG_DEBUG_KMEMLEAK`, `CONFIG_PROVE_LOCKING`, `CONFIG_DEBUG_ATOMIC_SLEEP` and `CONFIG_DMA_API_DEBUG`. It was booted once with `grub2-reboot`. Driver `5815209` (DKMS `1.0.0.230324.6.1`), `zero_copy=1`, OBS closed.

| Check | Result |
|---|---|
| Boot and probe | the card probed normally; no report naming the driver |
| 20 unbind/bind cycles with a 4K zero-copy stream and `arecord` open | all apps exited by themselves; 300 frames after the last bind |
| 2,000 ALSA start/stop cycles | none failed |
| `tests/zctest.sh` | every case passed; 0 failed, 0 late writes; 174 buffers outside the window during the start/stop cycles (see below); no `IO_PAGE_FAULT` or WARNING |
| `v4l2-compliance -s` | 58 of 59, the same by-design failure |
| KASAN, atomic-sleep and lockdep reports naming the driver | 0 |
| DMA API reports naming the driver | 0 |
| kmemleak, after the unbind cycles and again after the other tests | 0 unreferenced objects |

- **Reports from elsewhere:** a suspicious-RCU warning in `kernel/sched/fair.c` 1.5 s into boot, before the driver loads, and a DMA API warning from `udmabuf`, used by `kwin_wayland`. Neither involves this driver.
- **Buffers outside the window:** 0 through the four streaming cases, then 174 during the 100 zero-copy start/stop cycles, the same count as on the normal kernel. So it reproduces on a fresh boot and comes from rapid open, stream, close cycles. Those frames used the copy path, the designed fallback. Likely cause, inferred and not measured: with the IOMMU in `DMA-FQ` mode, freed addresses are reused only after the flush queue empties, so quick reallocations get lower addresses, below the card's 512 MiB window. 6.17 checks this.
- **Not covered here:** the suspend tests, since the debug kernel was booted only once.
- **Removed 2026-10-19,** after 6.8: the kernel, its modules, its DKMS builds and the build tree. `hws_video_quiesce()` and the 6.8 engine restart came after this pass, so lockdep hasn't seen the status thread taking `engine_lock` before `cap_lock`; by reading, every path that takes both takes them in that order.

## Stage 6: simulated card reset (6.2)

On the debug kernel `7.2.10-hwsdebug` (with `CONFIG_DEBUG_ATOMIC_SLEEP` and lockdep), driver `5815209` loaded with `debug_dma=1` and `zero_copy=1`. Writing to debugfs `simulate_reset` sets register 0 to 0x10, which clears the run bit.

- **Idle:** reset, then `arecord` for 3 s. The driver logged "card had lost its run state; re-initialised", and 144 audio packets arrived in 3 s (full rate is about 141). No atomic-sleep, lockdep or KASAN report.
- **During a 4K zero-copy stream:** reset at frame 185, then `arecord` 1 s later. Video stopped at the reset. The two zero-copy buffers in flight completed with an error. The re-init followed the audio start, and frames resumed at sequence 187 (one dropped), 1.1 s after the reset, at 16.67 ms spacing. No report from the checkers; zero-copy 0 failed, 0 late writes.
- **Gap found:** only an audio start notices a lost run state. Without one, video would stay stopped. Follow-up: let the status thread check the run bit on every poll while capture runs.
- **Video only, driver `af5925d` (6.19, DKMS `1.0.0.230324.6.2`):** reset during a 4K zero-copy stream with no audio started. The status thread re-initialised the card on its own; the stream's longest gap was 67 ms, one in-flight buffer completed with an error, and no sequence number was skipped. No checker report.

## Stage 6: suspend tests (6.3, 6.7)

Driver `af5925d` (DKMS `1.0.0.230324.6.2`) on 7.2.10, `zero_copy=1`, OBS closed. Each suspend is deep sleep (S3) through `sudo rtcwake -m mem -s 12` to `-s 20`, so no one had to wake the PC.

| Check | Result |
|---|---|
| One suspend with nothing open | woke by RTC; "resumed, card re-initialised"; clean log |
| `arecord -d 30` running across a suspend (6.3) | alsa-lib printed "Suspended. Trying resume. Failed. Restarting stream. Done." and the file holds the full 30.0 s; exit 0 |
| `arecord` stopped with SIGSTOP across a suspend, killed after resume (6.3) | no oops; the card records normally afterwards |
| Five suspend cycles with a 4K zero-copy stream and `arecord` running (6.7) | after each resume, 120–121 frames in 2 s and both apps still running; zero-copy 0 failed, 0 late writes; no "HDCP active" line; no BUG, WARNING or `IO_PAGE_FAULT` |

- **Audio content:** the Switch was sending digital silence during these runs. The card delivered real packets at about 47 per second with no silence fill, so the stream itself restarts and runs, but this run doesn't show sound returning after a resume. The operator's OBS session (first session above) does.
- **Unrelated:** each resume logs a firmware "TSC ADJUST differs" line from the BIOS.

## Stage 6: fault-injection builds (6.1, 6.5, 6.6)

A test-only copy of `af5925d`, never committed, with a module parameter `hws_fault` and log lines where the sound card and `hws_pcie_dev` are freed. Loaded with `insmod` in place of the installed driver on 7.2.10, then replaced by the installed driver again (srcversion `A3BA6FC3CEE7AB5988D4D63`, zero-copy working).

| Fault | Result |
|---|---|
| 1: FPGA version forced to 0 (6.1) | "zero_copy is on but unavailable: needs FPGA 122 or later; using the copy path" in the log, the same reason in debugfs `status`; 120 frames captured through the copy path; input capped at 3840 wide |
| 2: `snd_pcm_new()` fails (6.5) | probe fails; no video node or sound card left; the sound card and `hws_pcie_dev` are both freed; `rmmod` works |
| 3: `snd_card_register()` fails (6.5) | the same |
| 4: `hws_audio_register()` fails as a whole (6.5) | probe fails; the status thread is stopped; `hws_pcie_dev` freed; `rmmod` works |
| 5: register 0 bit 3 stuck at stop (6.6) | unbind took 242 ms with one warning, "card still busy 200 ms after stop"; everything freed; `rmmod` works |

No BUG, WARNING or oops in any run. kmemleak isn't in this kernel; the free markers stand in for it.

## 6.9: DMA-stop probe (driver `584abbb`, branch `probe/dma-stop`)

The probe build ran on branch `probe/dma-stop`; that code is debug-only and not in the stage branches. Loaded with `insmod`, `debug_dma=1`, `zero_copy=0` (so the card wrote only the driver's DMA buffers), IOMMU `DMA-FQ`, the Switch 2 at 3840×2160p60, one `v4l2-ctl` stream. Three runs, each on a fresh stream.

- **Method:** at a top-half interrupt the probe filled quarters 0 and 1 (idle until the next frame) with a pattern. 2,041 µs after the next frame-end interrupt, with the card inside quarter 0, it cleared the video enable bit and read register 2 back. It then sampled how far the card had written into quarter 0, and register 0, at 0 µs to 50 ms after the disable, and scanned both quarters.
- **Result, identical in all three runs:** quarter 0 was written up to byte 2,095,104 at the first sample, and not one more byte in the next 50 ms. The written part is a single prefix with no gaps and ends on a 2 KiB boundary. Quarter 1 wasn't touched. Register 0 read 0x3 at every sample: bit 3 stayed 0.
- **Meaning:** once the cleared enable bit is read back, the card writes nothing more. It finishes the 2 KiB burst in progress before that point. Register 0 bit 3 doesn't show DMA in progress, so waiting on it would prove nothing. `hws_video_quiesce()` relies on the readback and adds a short pause as margin.
- No BUG, WARNING or `IO_PAGE_FAULT`.
- **`hws_video_quiesce()`, driver `cc357eb` (DKMS `1.0.0.230324.6.3`, srcversion `B29F5E369590713DA901773`):** 4K capture 60.00 fps, 300 of 300 unique, no gaps; a zero-copy stream across one S3 cycle (`rtcwake`) delivered 120 frames in the 2 s after resume; unbind with that stream and `arecord` open took 20 ms, both apps exited by themselves, and capture worked after the bind (zero-copy 0 failed). No BUG, WARNING or `IO_PAGE_FAULT`.

## 6.8: input-size change with zero-copy active

Driver `2399b61` (DKMS `1.0.0.230324.6.5`, srcversion `0C2CDA11DC242C44B3B5C8C`) on 7.2.10, `zero_copy=1 debug_dma=1`. The operator switched the Switch 2's output between 1920×1080p60 and 3840×2160p60 from its settings menu, with OBS showing the picture. `tests/switchtest.sh` ran a second `v4l2-ctl` stream through `framecheck.py` and logged each size change the driver saw. The debugfs counter "engine restarts for a size change" counts the restarts the fix makes.

| Run | Zero-copy consumer | Size changes | Engine restarts | Frame check | Kernel log |
|---|---|---|---|---|---|
| B: capture 3840×2160, OBS closed for the first part | the `v4l2-ctl` stream | 20 checked (10 each way), then 20 more with OBS capturing 4K | 10, then 20 in total | 0 sequence repeats once error buffers are left out (see below) | 0 `IO_PAGE_FAULT`, 0 stray writes, no WARNING or BUG |
| A: capture 1920×1080 | OBS at 1920×1080 | 40 (20 each way) | 20, one for each change up to 4K | 9,140 frames, 0 sequence repeats, 0 error buffers, 12 sequence gaps (frames lost at switches) | 0 `IO_PAGE_FAULT`, **1 stray write**, no WARNING or BUG |

- **The fix ran where it should.** Restarts happen only when the input changes away from the size zero-copy is capturing. In run A that is every change from 1080p up to 4K: the change where the old code could have overrun a 1080p buffer. None of the 20 caused a fault or a write outside the driver's buffers.
- **The stray write is on the copy path, not zero-copy.** It landed in the driver's own DMA buffer 0 at offset 0xfe000–0x1057ff (30 KiB), 180 ms after the 1080p picture returned from a 4K→1080p change (the 18th). At that moment the input was 4K and OBS 1080p, so zero-copy wasn't in use and the old path ran: new lengths and a canary refill with the engine still writing a 4K quarter. The card wrote 30 KiB past the new 1080p length and then stopped. The buffer is sized for 4K, so nothing outside it was touched. This matches the stage 4 canary hit at a cable switch.
- **Transient size.** Change 38 read 1920×2160 for one poll before 1920×1080. The driver accepted it and moved on.
- **Recovery:** after each change the frame counter moved again within 18–93 ms (no-signal frames count).
- **Tooling fixes found by run B:** `framecheck.py` counted buffers dequeued with the error flag as frames (zero timestamp and sequence), and `switchtest.sh` counted each "no usable signal" as a size change. Both fixed in `144057f`.
- **Every size change restarts the engine, driver `bf3ede3` (DKMS `1.0.0.230324.6.6`, srcversion `3AB0CB5F8F9E57212FEA2AA`), `zero_copy=0 debug_dma=1`.** Run C: OBS at 1920×1080 and the `v4l2-ctl` stream, both on the copy path; 40 size changes (20 up to 4K, 18 down, and one through a transient 1920×2104). 9,822 frames, 0 sequence repeats, 0 error buffers, 10 sequence gaps (frames lost at switches); the frame counter moved again within 19–83 ms. 41 restarts during the run, one more than the changes the script saw, since a transient size can last less than its 50 ms poll. 0 `IO_PAGE_FAULT`, **0 stray writes**, no WARNING or BUG.

## 6.17: hardware validation pass

### Resolution matrix

Driver `bf3ede3` (DKMS `1.0.0.230324.6.6`), `debug_dma=1`, OBS closed. The PC's GPU (AMD, HDMI-A-2) drove the card's input. `tests/modematrix.sh` set each mode with `kscreen-doctor`, waited for the driver to report it, and captured 300 frames at the native size through `framecheck.py`, walking up and back down, with `zero_copy` 0 and then 1. The desktop on that output was static, so unique frames don't mean anything here.

| zero_copy | GPU mode | Driver input | fps | delta median / stdev ms | unique | gaps | repeats | errors | zero-copy frames |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@120.00 | 1920x1080 | 119.86 | 8.33 / 0.09 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@239.96 | 1920x1080 | 235.56 | 4.17 / 1.13 | 7/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@120.00 | 1920x1080 | 120.00 | 8.33 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.02 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.06 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.01 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 1920x1080@120.00 | 1920x1080 | 120.00 | 8.33 / 0.00 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 1920x1080@239.96 | 1920x1080 | 239.96 | 4.17 / 0.00 | 3/300 | 0 | 0 | 0 | 297 |
| 1 | 1920x1080@120.00 | 1920x1080 | 120.00 | 8.33 / 0.00 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.01 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 297 |
| 1 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.01 | 2/300 | 0 | 0 | 0 | 297 |

- **1080p240 on the copy path,** first pass: 235.56 fps over 300 frames with no gaps, repeats or errors. Two repeats of that mode alone with 1,200 frames gave 239.96 fps on both paths, so it was the start of the stream right after the mode change, not a sustained shortfall.
- Zero-copy took 297 of 300 frames at every size; the first frames of a stream use the copy path by design.

### Audio from a source that isn't 48 kHz

Driver `bf3ede3`. The card's EDID declares one audio format, 2-channel LPCM at 48 kHz, and the GPU's HDMI port (`hw:2,9`) then offers only 32 and 48 kHz. **A 44.1 kHz source can't be produced from this PC,** so that check isn't done. A source that ignores the EDID would be needed. 32 kHz stood in for it: a 1 kHz tone played straight to `hw:2,9`, captured from `hw:0,0` at 48 kHz.

| Source | Delivered | Content |
|---|---|---|
| 48 kHz, 1 kHz tone | 192,000 frames in 3.97 s | 1,000.0 Hz, full level |
| 32 kHz, 1 kHz tone | 192,000 frames in 3.99 s | 97.7% silence from the driver's silence fill; a few packets of 32 kHz samples played as 48 kHz (about 1,500 Hz) |

So with a source rate the card doesn't support, the stream keeps its 48 kHz pace and is mostly silent, with no rate drift and no garbage at full scale. It isn't usable audio. That's expected, since the EDID rules the rate out.

### Stale audio at each start (6.20)

The 32 kHz capture began with 1,024 frames that exactly matched the end of the 48 kHz capture before it. With 48 kHz only, alternating a 1 kHz and a 500 Hz tone, one capture of each, three pairs: every capture started with one or two packets (21–43 ms) of the previous capture's tone, even right after a module reload. Clearing the driver's audio ring before the enable changed nothing, so the card sends them. Driver `7784170` (DKMS `1.0.0.230324.6.7`, srcversion `76CCE9310DB87869487B36C`) drops the first two packets after each audio start: in five pairs, all 10 captures started with their own tone from frame 0.

### IOMMU modes

**`iommu.strict=1`** (one boot, IOMMU type `DMA`), driver `7784170` (DKMS `1.0.0.230324.6.7`), the PC's GPU as the source. `tests/zctest.sh` at 1920×1080p240 and again at 3840×2160p60: every case delivered at the full rate with 0 sequence gaps or repeats, zero-copy 0 failed and 0 late writes, and **0 buffers outside the window, including the 100 start/stop cycles** that counted 174 under the default `DMA-FQ`. 0 `IO_PAGE_FAULT`, stray writes or warnings. This supports the inferred cause: with lazy (flush-queue) unmapping, freed addresses come back late, so quick reallocations land below the 0xE0000000 window and use the copy path. Under strict unmapping they are reused at once. The copy-path fallback is the designed behaviour, so nothing needs changing.

**`iommu=pt`** (one boot, IOMMU type `identity`), driver `7784170`. At probe the driver logged "4.5 MiB DMA buffers unavailable (no IOMMU translation or CMA); 4096-wide input will be captured at 3840", and debugfs gave zero-copy's reason as "IOMMU not translating". `tests/modematrix.sh`:

| zero_copy | GPU mode | Driver input | fps | delta median / stdev ms | unique | gaps | repeats | errors | zero-copy frames |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.12 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@120.00 | 1920x1080 | 119.68 | 8.33 / 0.29 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@239.96 | 1920x1080 | 237.20 | 4.17 / 0.84 | 6/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@120.00 | 1920x1080 | 120.01 | 8.33 / 0.02 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.03 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.10 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.10 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.18 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.16 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.07 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@120.00 | 1920x1080 | 120.00 | 8.33 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@239.96 | 1920x1080 | 239.96 | 4.17 / 0.02 | 4/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@120.00 | 1920x1080 | 120.00 | 8.33 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.04 | 2/300 | 0 | 0 | 0 | 0 |

- Every mode delivered through the copy path, with `zero_copy` 0 and 1 alike (0 zero-copy frames). The first stream after a change to 120 or 240 Hz on the copy path again started slightly slow (119.68 and 237.20 fps over 300 frames, no gaps), as in the matrix above.
- **4096×2160p60 from the GPU:** the driver's input and the DV timings read 3840×2160, and 300 frames captured at 60.00 fps with no gaps or repeats. The card scales 4096-wide input to 3840 before the FPGA (stage 3), so the driver's own 3840 clamp for 4 MiB buffers can't be exercised with this card.

**`amd_iommu=off`** (one boot; the card has no IOMMU group), driver `7784170`. The same fallback message as under `iommu=pt`; the DMA buffers are 4 MiB at physical addresses below 4 GB (buffer 0 at 0x55800000, address table window 0x40000000), and zero-copy reports "IOMMU not translating". `tests/modematrix.sh`:

| zero_copy | GPU mode | Driver input | fps | delta median / stdev ms | unique | gaps | repeats | errors | zero-copy frames |
|---|---|---|---|---|---|---|---|---|---|
| 0 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.13 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.12 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.12 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@120.00 | 1920x1080 | 119.87 | 8.33 / 0.08 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@239.96 | 1920x1080 | 239.96 | 4.17 / 0.01 | 4/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@120.00 | 1920x1080 | 120.00 | 8.33 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.07 | 2/300 | 0 | 0 | 0 | 0 |
| 0 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.02 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@120.00 | 1920x1080 | 119.87 | 8.33 / 0.08 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@239.96 | 1920x1080 | 239.97 | 4.17 / 0.00 | 5/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@120.00 | 1920x1080 | 120.00 | 8.33 / 0.00 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 3840x2160@60.00 | 3840x2160 | 60.00 | 16.67 / 0.03 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 2560x1440@59.95 | 2560x1440 | 59.95 | 16.68 / 0.01 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1920x1080@60.00 | 1920x1080 | 60.00 | 16.67 / 0.17 | 2/300 | 0 | 0 | 0 | 0 |
| 1 | 1280x720@60.00 | 1280x720 | 60.00 | 16.67 / 0.15 | 2/300 | 0 | 0 | 0 | 0 |

- A 1 kHz tone from the GPU's HDMI port came through at 1,000.0 Hz and full level.
- The three test boot entries were removed afterwards; the default entry stayed `7.2.10-200` throughout.

### 8-hour OBS session at 4K60

Driver `7784170` (DKMS `1.0.0.230324.6.7`), `zero_copy=1` from `/etc/modprobe.d/hws.conf`, normal boot (IOMMU `DMA-FQ`). OBS captured 3840×2160 YUYV over zero-copy from the PC's HDMI-A-2 output at 3840×2160p60, with a quiet 1 kHz tone (about −38 dBFS) played into the card through `hw:2,9`. Sleep and screen power-off were blocked with `systemd-inhibit` and `kde-inhibit` for the run. `tests/soak.sh 8 30`, 2026-10-19 23:11 to 2026-10-20 07:12:

| time | fps | lost | zero-copy failed | audio gaps | OBS RSS | MemAvailable | IO_PAGE_FAULT | stray writes | PipeWire errors |
|---|---|---|---|---|---|---|---|---|---|
| 23:12 | 59.99 | 0 | 0 | 1 | 433 MiB | 84806 MiB | 0 | 0 | 0 |
| 23:42 | 59.99 | 0 | 0 | 1 | 449 MiB | 85932 MiB | 0 | 0 | 0 |
| 00:12 | 59.99 | 0 | 0 | 1 | 449 MiB | 85878 MiB | 0 | 0 | 0 |
| 00:42 | 59.99 | 0 | 0 | 1 | 458 MiB | 85494 MiB | 0 | 0 | 0 |
| 01:12 | 59.99 | 0 | 0 | 1 | 458 MiB | 85565 MiB | 0 | 0 | 0 |
| 01:42 | 59.99 | 0 | 0 | 1 | 459 MiB | 84499 MiB | 0 | 0 | 0 |
| 02:12 | 59.99 | 0 | 0 | 1 | 459 MiB | 84287 MiB | 0 | 0 | 0 |
| 02:42 | 59.99 | 0 | 0 | 1 | 459 MiB | 85667 MiB | 0 | 0 | 0 |
| 03:12 | 59.99 | 0 | 0 | 1 | 459 MiB | 86190 MiB | 0 | 0 | 0 |
| 03:42 | 59.99 | 0 | 0 | 1 | 459 MiB | 84736 MiB | 0 | 0 | 0 |
| 04:12 | 59.99 | 0 | 0 | 1 | 459 MiB | 86248 MiB | 0 | 0 | 0 |
| 04:42 | 59.99 | 0 | 0 | 1 | 459 MiB | 86374 MiB | 0 | 0 | 0 |
| 05:12 | 59.99 | 0 | 0 | 1 | 459 MiB | 85260 MiB | 0 | 0 | 0 |
| 05:42 | 59.99 | 0 | 0 | 1 | 459 MiB | 86300 MiB | 0 | 0 | 0 |
| 06:12 | 59.99 | 0 | 0 | 1 | 459 MiB | 85269 MiB | 0 | 0 | 0 |
| 06:42 | 59.99 | 0 | 0 | 1 | 459 MiB | 86236 MiB | 0 | 0 | 0 |
| 07:12 | 59.99 | 0 | 0 | 1 | 459 MiB | 85253 MiB | 0 | 0 | 0 |

- **Totals at the end:** 1,796,601 frames, 0 lost; zero-copy 1,790,121 frames, 0 failed, 0 late writes, 0 buffers outside the window; OBS's consumer had no buffer missing once.
- **Rate:** 59.99 in every row; `soak.sh` truncates to two decimals, and the card counted 1,796,601 frames in about 8 h 00 min, which is 60 fps.
- **Audio:** no gap during the run (the 1 counted before the tone started; two more came after the tone stopped at the end).
- **Memory:** OBS grew from 433 to 459 MiB in the first 2.5 h and then stayed at 459 MiB for 5.5 h. Available system memory moved between 84.3 and 86.4 GiB with no trend.
- **Kernel and PipeWire:** 0 `IO_PAGE_FAULT`, 0 stray writes, 0 PipeWire errors or xruns, no WARNING or BUG.

### GPU losses during mode changes while capturing (6.21, open)

The GPU (Radeon RX 9070, `amdgpu 0000:03:00.0`) dropped off the PCIe bus twice, each time while `kscreen-doctor` changed HDMI-A-2's mode and the card was capturing from it. Both times the kernel logged `device lost from bus!`, then SMU and display-engine errors, and the desktop stopped responding (the second time the screen went white); the operator rebooted.

| When | Driver | Capturing | Mode changes before the loss | Lost 1.3–3 s after |
|---|---|---|---|---|
| 2026-10-19 22:58:47 | `7784170` (DKMS 6.7), `zero_copy=1` | OBS at 1280×720, zero-copy | 1 | 720p → 2160p60 |
| 2026-10-20 07:47:56 | `6432897`, `zero_copy=1 debug_dma=0` | `v4l2-ctl` at 1920×1080 | about 17, alternating 1080p60 and 2160p60 every 6 s (the 6.10 check) | 2160p60 → 1080p60 |
| 2026-10-20 08:07:02 | DKMS 6.7, reloaded with defaults 3 s before | **nothing** | 1 | 1080p240 → 2160p60 |

What didn't lose the GPU: about 66 mode changes in the 6.17 matrix runs (driver 6.6 and 6.7, normal, `iommu=pt` and IOMMU-off boots), where each mode was set **before** a capture started; 120 resolution changes from the Switch 2 during capture (6.8); the 8-hour session (no mode changes). The card sits behind the chipset (00:02.1) and the GPU on a CPU root port (00:01.1), in different IOMMU groups; neither loss came with an AER error or an IOMMU fault from the card.

The third loss came with nothing capturing, so capture isn't required; it followed a driver reload by about 3 s (the card's HDMI receiver is re-initialised at probe). The ~66 safe changes had no reload just before. Explanations that still fit:
1. The engine restart that 6.8 added for every input-size change while capturing (`bf3ede3`) makes the card's HDMI receiver react in a way that hangs the GPU's display engine mid-modeset.
2. A GPU mode change on the output feeding this card can hang the GPU, more often when the card's receiver is busy (capturing) or settling (just re-initialised): receiver or amdgpu behaviour, independent of the driver version.

**Control run, capture driver unloaded (2026-10-20 08:32–08:35).** Card cabled to HDMI-A-2 and powered, `HwsUHDX1Capture` removed with `rmmod` (no `/dev/video0`), OBS closed. 20 changes alternating 1920×1080p60 and 3840×2160p60, one every 10 s: every mode took, and the kernel logged no `device lost`, `REG_WAIT` or ring timeout. The driver then loaded normally and captured 120 of 120 frames. This points at something the driver does (the card's HDMI receiver with the driver loaded is the remaining difference), but it isn't proof: the second loss came only after about 17 changes at 6 s intervals, and 20 clean changes is a small sample.

**Driver loaded, nothing capturing (2026-10-20 08:38–08:41).** DKMS 6.9 (srcversion `0CD4DA1B1E0DF0597BFDFE4`) loaded, `/dev/video0` not open, the same 20 changes 10 s apart: no GPU error; the driver's status file followed each change (1920×1080, 3840×2160). So the status thread's polling of an idle card doesn't trigger it either, at least at this pace. The third loss (nothing capturing, but 3 s after a driver load) and the second (capturing, 6 s apart) remain.

**Code check (no hardware).** The register writes a capture makes when the input size changes (the 6.8 restart) are the video enable (register 2), the quarter lengths (50–53), the DMA size cap (9) and, with zero-copy, the buffer addresses (16–19): DMA and engine registers only. The restart's `CheckCardStatus()` would run the full start sequence only if register 0's run bit were clear; probe 6.9 showed it stays set after a stop, and the status thread's check of the same bit (6.19) never fired in any boot, the three loss boots included. Nothing in the driver writes EDID, hot-plug or HDMI link controls; no known register does that. The one place the driver may reset the card's input path is the undocumented register 0 start sequence at probe and resume (0x00, 0x10, 0x80000000, 0x80FFFFFF, 0x13), which the vendor's Windows driver also writes.

**Closed as a known issue (2026-10-20).** Nothing found ties the losses to a driver defect: the driver doesn't touch the HDMI link during capture, and a GPU that drops off the bus over a sink's behaviour points at amdgpu or GPU firmware. The setup is unusual (the capture PC's own GPU as the source); 120+ Switch 2 resolution changes during capture were clean. Recorded for the user guide (7.2); no more mode changes on this output in testing. Further runs (capturing with DKMS 6.3 against 6.9) would narrow it but each risks a lockup; it risks another lockup, so it waits for the operator. Until then no mode changes are made on the GPU output feeding the card at all; `tests/modematrix.sh` refuses to run without `HWS_ALLOW_GPU_MODESET=1`.

### Not done

- **A second, non-X3D machine:** none available.

## 6.11: a frame repeated to the zero-copy consumer

Driver `fix/release-audit` with a debug knob (commit below), `zero_copy=1 debug_dma=1`, 3840×2160p60 from the PC's GPU. Writing K to debugfs `stale_bottom_halves` makes the K-th bottom half from then on take the stale-half path. On a fresh stream, K=3 lands on the frame where zero-copy picks its first buffer: the zero-copy consumer then holds a copy buffer when the next frame arrives in its zero-copy buffer. 60-frame captures through `framecheck.py`:

| Build | K | Runs | Sequence repeats | Error buffers | "held copy buffers failed" |
|---|---|---|---|---|---|
| 6.11 fix removed (test build, not committed) | 3 | 5 | 1 in each | 0 | — |
| with the fix | 3 | 5 | 0 | 1 in each | +1 in each |
| with the fix | 1, 2, 4–10 | 1 each | 0 | 0 or 1 | 0 (the path isn't reached) |

The knob reproduces LINUX-014 and the fix removes the repeat: the held buffer completes with an error instead. A stale bottom half every 0.3 s during a running zero-copy stream (40 of them over 900 frames) gave 40 sequence gaps, 40 error buffers and no repeats. The 6.8 switch runs had 0 repeats as well. No `IO_PAGE_FAULT`, WARNING or BUG.

## 6.15: late copies under load

Driver `6432897` (built from the branch, `insmod`), `zero_copy=0` so every frame takes the copy path, the PC's GPU as the source, OBS closed. Captures through `framecheck.py`; "late" is debugfs "copy path: frames failed because a copy ended late", "card lost" the driver's dropped-frame count. `stress-ng` isn't installed, so a small C program loaded the CPUs instead.

| Input | Load | Delivered | Gaps | Repeats | Error buffers | Late | Card lost |
|---|---|---|---|---|---|---|---|
| 3840×2160p60, 600 frames | none | 60.00 fps | 0 | 0 | 0 | 0 | 0 |
| 3840×2160p60 | 32 spinning threads | 17.68 fps | 1,434 | 0 | 0 | 0 | 0 |
| 3840×2160p60 | 32 threads copying 64 MiB buffers | 17.80 fps | 1,420 | 0 | 0 | 0 | 0 |
| 1920×1080p240, 2,400 frames | none | 239.96 fps | 0 | 0 | 0 | 0 | 0 |
| 1920×1080p240 | 32 spinning threads | 48.29 fps | 9,523 | 0 | 0 | 0 | 0 |
| 1920×1080p240 | 32 memory-copy threads | 39.74 fps | 12,086 | 0 | 0 | 0 | 0 |
| 3840×2160p60, 600 frames | SCHED_FIFO 60 bursts on CPU 31: 10 ms busy, 20 ms idle | 50.41 fps | 114 | 0 | 115 | 115 | 0 |
| 1920×1080p240, 2,400 frames | SCHED_FIFO 60 bursts on CPU 31: 3 ms busy, 7 ms idle | 178.72 fps | 822 | 0 | 683 | 683 | 139 |

- **Ordinary load doesn't make copies late.** The IRQ thread runs at SCHED_FIFO 50, so normal-priority load never delays it; the late count stayed 0. The rate fell because the capturing program (`v4l2-ctl` and the Python checker) was starved and had no buffer queued.
- **A delayed IRQ thread does.** Real-time bursts on the IRQ thread's CPU (31) made copies end past the half-frame window. Every late copy came back as an error buffer, and the gaps match: 683 late + 139 lost = 822 at 240 Hz; at 4K, 115 error buffers and 114 gaps (the last frame's failure leaves no gap after it). No repeats, no `IO_PAGE_FAULT`, WARNING or BUG.
- `sched_rt_runtime_us` equals `sched_rt_period_us` (1,000,000) on this kernel, so real-time throttling is off; the burst program always slept longer than it spun.
- With no load, the late count stayed 0 here, in the 8-hour session, and in every earlier capture: no false positives seen.

## 6.10: no stray-write lines with debug_dma off

Driver `7784170` (DKMS `1.0.0.230324.6.7`), `debug_dma=0` (the default) and `zero_copy=1`, Switch 2 as the source (no GPU mode changes; see 6.21). OBS captured at 3840×2160 while `tests/switchtest.sh` ran a second stream; the operator switched the Switch between 1920×1080 and 3840×2160 20 times (10 each way).

- 0 "stray DMA write" lines, 0 `IO_PAGE_FAULT`, no WARNING or BUG.
- 21 engine restarts for the 20 changes (one transient size); the frame counter moved again within 20–59 ms of each change.
- 5,839 frames, 0 sequence repeats, 0 error buffers, 6 sequence gaps (frames lost at switches).
- The second stream asked for 1920×1080 but got 3840×2160, the size OBS held, so `framecheck.py`'s unique-frame count from this run is wrong (it cut 4K frames into 1080p pieces). The sequence checks come from the log and stand. `switchtest.sh` now reads the format back.
- An earlier attempt with GPU mode changes was stopped by the GPU loss in 6.21.

**IRQ-thread wakeup latency at 1080p240.** Driver DKMS `1.0.0.230324.6.7`, `zero_copy=0` (the copy path, where the canary sits), the PC's GPU at 1920×1080p239.96, set while nothing was capturing (6.21). For each run the driver was reloaded with `debug_dma` 0 or 1, a 3,000-frame `v4l2-ctl` capture started, and ftrace recorded `sched_waking` and `sched_switch` for the IRQ thread for 10 s; the latency is from the thread's wakeup to it running (ftrace timestamps have 1 µs resolution).

| `debug_dma` | Wakeups | Median | p99 | p99.9 | Max | Frames lost |
|---|---|---|---|---|---|---|
| 0 | 4,808 | 1 µs | 5 µs | 16 µs | 18 µs | 0 |
| 1 | 4,810 | 1 µs | 8 µs | 17 µs | 23 µs | 0 |
| 0 | 4,810 | 1 µs | 4 µs | 16 µs | 28 µs | 0 |
| 1 | 4,809 | 1 µs | 4 µs | 16 µs | 32 µs | 0 |

No difference beyond run-to-run noise: the canary scan, now outside `cap_lock`, doesn't delay the IRQ thread. No late copies or `IO_PAGE_FAULT` during the eight runs. Putting things back afterwards lost the GPU (6.21, third loss): the driver was reloaded with its defaults at 08:06:58, HDMI-A-2 was switched from 1080p240 to 2160p60 with nothing capturing, and at 08:07:02 the GPU dropped off the bus.

## 6.18: dead code removed

Four commits on `fix/release-audit`: `aebb731` (the per-device vb2 queue and its ops), `be1e6ac` (unused fields, tables, ioctl numbers and locals; the register read-backs in `InitVideoSys()` stay as plain reads), `9ce0184` (thirteen `#if 0` and `#if 1` blocks, resolved by script) and `2d4e62a` (three unused includes and seventeen repeated ones). `hws_video.c` and `hws.h` lose about 560 lines. W=1 warnings on 7.2.10: 30 before, 23 after; it builds without errors on 7.2.6, 7.2.9 and 7.2.10.

Checks on that build (`insmod` of a fresh build, then DKMS `1.0.0.230324.6.9`, srcversion `0CD4DA1B1E0DF0597BFDFE4` on all three kernels), the PC's GPU at 3840×2160p60 as the source, no mode changes:

| Check | Result |
|---|---|
| `v4l2-compliance -s` | 58 of 59, the by-design `reqbufs != EBUSY` |
| DV timings, inputs, `G_PARM`, controls | 3840×2160p60 594 MHz; `HDMI` ok; 60/1; brightness, contrast, saturation |
| `S_PARM` 30 in the streaming handle | 30.00 fps, deltas 33.33 ms |
| `tests/zctest.sh` | every case 60.00 fps, 0 gaps, 0 repeats, 0 error buffers; zero-copy 0 failed, 0 late writes; 100 start/stop cycles added nothing to the window count |
| Audio, alternating 1 kHz and 500 Hz captures (6.20) | each starts with its own tone; 96,000 frames in 2.06 s |
| Unbind with `v4l2-ctl` and `arecord` open | 16 ms, both exited by themselves; 120 of 120 frames after the bind |
| Kernel log | no `IO_PAGE_FAULT`, WARNING, BUG or GPU loss |

The window count stood at 232 before `zctest.sh` began: `v4l2-compliance -s` runs many quick buffer cycles, the `DMA-FQ` effect recorded in 6.17.

### Build output in the branch, and a stale DKMS module

`7784170` staged `src/` with `git add -A` and so committed the kbuild output of a local build (`.o`, `.ko`, `.cmd`, `.mod.c`); `aeafbaa` and `6432897` updated it. Upstream tracks none of these. `tools/dkms-install.sh` packed all of `src/` from git, so the objects reached DKMS, and for DKMS `1.0.0.230324.6.8` kbuild reused the committed `hws_video.o` on 7.2.10 instead of compiling: that module held the code from `6432897` (it still had the removed queue code; srcversion `FBA939D1AAC60E8C53BE29D`, unlike the other kernels). Earlier installs weren't affected in practice, since the committed objects matched their commit's source (6.7 had the same srcversion on every kernel). `b5c3a3d` removes the build output from the index and makes the installer take only `Makefile`, `dkms.conf` and the three source files. 6.8 was removed from DKMS and 6.9 installed; all its modules have the same srcversion and none has the removed code. When the series is split into PRs, those three commits must leave out their `src/` build output.
