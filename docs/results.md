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
