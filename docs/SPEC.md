# VC12-4K driver fixes and features

This spec covers fixes and features for the HwsUHDX1Capture driver (AVMatrix VC12-4K, PCI `8888:8581`). It is split into a baseline and three stages. Each stage lists its changes and how to check them.

Line numbers refer to `src/hws_video.c` at commit `f40a4d5` (branch `fix/audio-batch-timing`).

## Why

A review of the driver against a running card (FPGA version 122, PCIe 2.0 x4) found these problems:

- **Interrupts off during copies.** `video_data_process` holds `spin_lock_irqsave(&videoslock)` across every frame copy and the CPU scaler (lines 3490–3607). With a 3840×2160 source and a 1920×1080 consumer, that is about 1.9 ms per buffer on a Ryzen 9 9950X3D.
- **Duplicate frames.** The worker fills every queued buffer with the same frame (lines 3552–3594). With 4 queued buffers (OBS's default), apps receive about 240 fps from a 60 fps source, and interrupts are off for about 7.7 ms per frame. Upstream PR #23 reports this and links it to HDMI audio dropouts in OBS.
- **Possible kernel crash.** `S_FMT` has no busy check, and the copy path never checks the destination size (lines 421, 1667, 3585). A second app changing the format while another streams makes the worker write past the end of the first app's buffers.
- **DMA overrun on 4096×2160.** The DMA buffers are 4 MiB, but a 4096×2160 quarter-frame is 4,423,680 bytes. The size check is commented out (line 4144), and the 3840 clamp applies only to version-0 hardware (line 5719).
- **Teardown race.** `stop_streaming` can return while the worker still holds one of the app's buffers.
- **Wrong timestamps and sequence numbers.** Both are taken at copy time and per buffer, not per frame.
- **Load average.** The status thread sleeps uninterruptibly (`ssleep`, line 5815), which adds 1.0 to the load average.
- **Queue buffer allocation.** The queue buffers are 4 MiB `kmalloc` allocations (the largest contiguous size, so they can fail after uptime). The page-reservation code around them is unused and has a copy-paste bug (line 4403).
- **Apps can't see the input.** The driver reports 15–240 fps for every size, ignores `S_PARM` (issue #15), and has no DV timings or signal status. Its picture controls store values but never reach the hardware (register 91 holds them).

## Ground rules

- **Branches** are stacked on `fix/audio-batch-timing`: `fix/video-delivery` → `feat/input-status` → `rework/single-copy`. To upstream, rebase each topic onto `main`. The stages don't touch the audio code.
- **Kernel compatibility.** Keep the existing `LINUX_VERSION_CODE` guards. Use only APIs that exist on 5.x and later: `kvmalloc`, `schedule_timeout_idle`, `vb2_is_busy`, `request_threaded_irq`, the `v4l2_*dv_timings*` helpers, `v4l2_src_change_event_subscribe`, `v4l2_ctrl_*`. Compile against every installed kernel's headers.
- **Safety.**
  - Keep the IOMMU translating. It currently contains stray DMA writes (stage 3.5).
  - Never test format changes during streaming on an unfixed driver.
  - Load a new build by rebooting after `dkms install`. `modprobe -r` can hang while PipeWire or WirePlumber hold the ALSA device.
- **Rollback.** Copy the known-good ref's `src/` into `/usr/src/HwsUHDX1CaptureForLinux-1.0.0.230324/`, run `dkms build` and `dkms install --force`, then reboot.

## Stage 0: baseline

Record these on the unmodified driver, with no other app using the card, in `docs/results.md`:

- **Frame delivery:** `tests/framecheck.py` on a `v4l2-ctl --stream-mmap=4 --stream-count=300` capture. Report fps, unique frames, timestamp deltas and sequence gaps.
- **Compliance:** `v4l2-compliance -d /dev/video0` pass and fail counts. Don't use `-s`.
- **Status thread:** its state and `/proc/loadavg`.
- **Optional:** a 10-minute OBS session (4K source, 1080p capture). Record PipeWire ERR deltas for the headphone output and the card, OBS missed frames, and the `IO_PAGE_FAULT` count.

`tests/run-baseline.sh` runs the automated parts.

## Stage 1: `fix/video-delivery`

| # | Change |
|---|---|
| 1.1 | **Slot states; lock only for bookkeeping.** Queue slots move between FREE, READY and READING. The worker holds `videoslock` only to choose a slot and pop buffers. It copies with the lock dropped, then frees the slot. The tasklet writer never writes a READING slot. Overwriting a READY slot counts as a drop. Each slot stores its own sizes, dimensions and interlace flag, which are shared today. |
| 1.2 | **One buffer per consumer per frame.** The worker takes the newest READY slot and drops older ones. It fills at most one buffer for each streaming consumer. If no new frame arrived, it delivers nothing. With no signal it delivers one blue frame per consumer per status check, as now. |
| 1.3 | **Format busy and size checks.** `S_FMT` with a different format returns `-EBUSY` while any consumer's queue is busy. Every copy checks `vb2_plane_size()` first; a buffer that is too small completes with `VB2_BUF_STATE_ERROR`. |
| 1.4 | **Teardown.** Each file handle counts its in-flight buffers. `stop_streaming` waits until that count reaches zero. |
| 1.5 | **Timestamps and sequence.** Taken at the frame-end interrupt, stored in the slot with a per-channel frame counter, and copied to every buffer for that frame. Dropped frames show as sequence gaps. The queue also gets `V4L2_BUF_FLAG_TSTAMP_SRC_EOF`. |
| 1.6 | **DMA sizing.** Allocate 4.5 MiB per DMA buffer: the largest quarter plus the audio area. Check that each buffer stays inside one 512 MiB address-table window. If the allocation fails (no IOMMU translation and no CMA), fall back to 4 MiB and clamp the input to 3840×2160. Restore the quarter-size check. A size that still doesn't fit is reported as no signal and never programmed into the DMA engine. |
| 1.7 | **Status thread.** Sleeps with `schedule_timeout_idle()` inside a `kthread_should_stop()` loop. |
| 1.8 | **Queue buffers.** Allocated with `kvmalloc` and freed with `kvfree`. The page-reservation loops and the `m_pVideoData_area*` fields are removed. |
| 1.9 | **Ioctl deadlock.** Found by the stage 0 baseline. `hws_unlocked_ioctl` holds the device-wide `ioctl_lock` across `video_ioctl2` so it can swap `vdev->queue` to the caller's queue. A blocking `DQBUF` therefore holds that lock while it waits, and any other ioctl on the device blocks until a frame arrives, forever if streaming hasn't started. `v4l2-compliance` hangs this way. Fix: remove the queue swapping. Buffer ioctls, `poll` and `mmap` call vb2 on the caller's own queue under that queue's `qmutex`; `q->lock` is set on all kernels, so vb2 drops it while waiting. `vdev->lock` is cleared, so the V4L2 core takes no device lock. Format and parameter ioctls take `video_lock` themselves. |

**Done when:**
- With 4 buffers, delivery is 59.94 fps ±0.1 and every frame is unique.
- Two simultaneous streams each get 59.94 fps.
- A conflicting `S_FMT` during a stream returns `EBUSY`.
- Timestamp deltas are about 16.68 ms (jitter under 1 ms), and sequence numbers step by 1 except at drops.
- The status thread is not in `D` state.
- `v4l2-compliance` (no `-s`) completes without hanging. The remaining failures are the stage 2 items, plus the multi-reader `reqbufs != EBUSY` result, which is by design.
- 200 open/stream/close cycles beside a running stream leave no warning or oops in dmesg.
- In a 10-minute OBS session, audio errors and missed frames are no worse than the baseline.
- dmesg confirms 4.5 MiB DMA buffers.

## Stage 2: `feat/input-status`

| # | Change |
|---|---|
| 2.1 | **DV timings and input status.** Implement `QUERY/G/S/ENUM_DV_TIMINGS` and `DV_TIMINGS_CAP`. Timings come from registers 5 and 90 plus the measured rate. Match against `v4l2_dv_timings_presets` by size, interlace and closest rate; otherwise synthesize zero-blanking timings with pixelclock = w × h × fps. `ENUMINPUT` reports `V4L2_IN_CAP_DV_TIMINGS` and the `NO_SIGNAL`/`NO_SYNC` status bits. Remove `G/S_STD`. Add `SUBSCRIBE_EVENT` and send `V4L2_EVENT_SOURCE_CHANGE` when the size, interlace flag or signal changes. Poll status every 250 ms. Implement `log_status`. |
| 2.2 | **Measured frame rate.** Keep an EWMA of frame-end intervals, snapped to a standard rate when within 0.5%. |
| 2.3 | **Native size.** The detected input size is listed first in `ENUM_FRAMESIZES`. `TRY/S/G_FMT` work from width and height rather than table indexes. While nothing streams, the default format follows the input. The CPU scaler remains for apps that request a table size. |
| 2.4 | **Frame intervals and `S_PARM`.** List input/1 to input/4 as intervals. `S_PARM` picks the closest divisor per file handle and skips frames for that consumer. `G_PARM` reports the effective rate with `V4L2_CAP_TIMEPERFRAME`. |
| 2.5 | **Picture controls.** Brightness, contrast, saturation and hue move to a `v4l2_ctrl_handler`, range 0–255, default 128. Changes write register `91 + 2·ch` as `sat<<24 \| hue<<16 \| con<<8 \| bri` and are re-applied after a hardware re-init. |
| 2.6 | **Small fixes.** `bus_info` becomes `PCI:<pci_name>`. Fix the 720×576 table entry, which currently says 720×480. Drop `VB2_READ` and the `read()` stub. |

**Done when:**
- `v4l2-compliance` failures drop from the baseline.
- `--query-dv-timings` matches the source, and returns `ENOLINK` with no signal.
- Switching the source's resolution raises a source-change event.
- `--set-parm 30` on a 59.94 fps source streams at 29.97.
- Picture controls visibly change the image, and register 91 reads back the values.
- An existing OBS source keeps working.

## Stage 3: `rework/single-copy`

This design will be refined after stages 1 and 2 have run for a while.

- A threaded IRQ replaces the video tasklet and workqueue.
- Each DMA half is copied straight into one buffer per consumer. A staging frame is used only for interlaced input or scaling.
- The 16-slot queue goes away (72 MiB).
- `remove()` and probe error paths are fixed, with MSI fallback, the correct `IRQF_SHARED` usage, and `alloc_ordered_workqueue`.
- The stray DMA writes to IOVA `0xfec00000` at capture start and stop are diagnosed through debugfs (DMA addresses and register readbacks) and then fixed.
- A debugfs status file reports the input, the frame rate, per-consumer counters and IRQ counts.

**Done when:** stage 1 and 2 checks still pass, each frame is copied once per consumer, and no `IO_PAGE_FAULT` occurs at capture start or stop.

## Build and test loop

1. Compile against each `/lib/modules/*/build`.
2. Install: copy `src/` into the DKMS source directory, run `dkms build` and `dkms install --force`, then reboot.
3. Run the stage's checks and record the results in `docs/results.md`.
