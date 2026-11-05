# VC12-4K driver fixes and features

This spec covers fixes and features for the HwsUHDX1Capture driver (AVMatrix VC12-4K, PCI `8888:8581`). It is split into a baseline and seven stages, plus host settings outside the driver. Each stage lists its changes and how to check them. Stages 6 and 7 address the findings of the release audit in [AUDIT.md](AUDIT.md). The [upstream PR series](#upstream-pr-series) plans how the work goes upstream. [REGISTERS.md](REGISTERS.md) lists what is known about the card's registers.

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

- **Branches and PRs.** Each stage has its own branch, stacked on the one before:
  - `fix/audio-batch-timing` (audio fix)
  - `fix/video-delivery` (stages 0 and 1)
  - `feat/input-status` (stage 2)
  - `rework/single-copy` (stage 3)
  - `host/dkms` (H.4), `feat/hw-status` (stage 4), `host/irq-affinity` (H.2) and `feat/zero-copy` (4.4)
  - `fix/release-audit` (stage 6) and `release/packaging` (stage 7), each on top of the one before

  The stage branches are the fork's development history and stay as pushed. Upstream gets smaller PRs rebuilt from them: see [Upstream PR series](#upstream-pr-series). Commits that change docs never change `src/`, so the rebuilt PRs can leave the fork's docs out.
- **Kernel compatibility.** Keep the existing `LINUX_VERSION_CODE` guards. Use only APIs that exist on 5.x and later: `kvmalloc`, `schedule_timeout_idle`, `vb2_is_busy`, `request_threaded_irq`, the `v4l2_*dv_timings*` helpers, `v4l2_src_change_event_subscribe`, `v4l2_ctrl_*`. Compile against every installed kernel's headers.
- **Safety.**
  - Keep the IOMMU translating. It currently contains stray DMA writes (stage 3.5), but only while the stray address is unmapped. The card sits behind the chipset in IOMMU group 22, together with the SATA, USB, Wi-Fi and both Ethernet controllers. All devices in a group share one DMA address space. A stray write that hits a writable mapping of one of them goes through and corrupts that device's data without a fault (see H.5).
  - Never test format changes during streaming on an unfixed driver.
  - Load a new build by rebooting after `dkms install`. `modprobe -r` can hang while PipeWire or WirePlumber hold the ALSA device.
- **Rollback.** Each build is its own DKMS version (H.4). Install an earlier one with `sudo dkms install -m HwsUHDX1CaptureForLinux -v <version> -k <kernel> --force` for each kernel, then reboot. `tools/dkms-install.sh --status` shows what each kernel has. `1.0.0.230324.2.0` is stage 2 (`b8cb4e6`).

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
| 1.9 | **Ioctl deadlock.** Found by the stage 0 baseline. `hws_unlocked_ioctl` holds the device-wide `ioctl_lock` across `video_ioctl2` so it can swap `vdev->queue` to the caller's queue. A blocking `DQBUF` therefore holds that lock while it waits, and any other ioctl on the device blocks until a frame arrives, forever if streaming hasn't started. `v4l2-compliance` hangs this way. Fix: remove the queue swapping. Buffer ioctls, `poll` and `mmap` call vb2 on the caller's own queue under that queue's `qmutex`; `q->lock` is set on all kernels, so vb2 drops it while waiting. `vdev->lock` is cleared, so the V4L2 core takes no device lock. Every other ioctl runs under the device's `ioctl_lock`. |

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
| 2.7 | **Suspend and resume.** Found during stage 2 testing: after S3 the card's registers are wiped and capture stays dead until a reboot, because the driver has no PM callbacks. Suspend records and stops the running engines; resume replays the probe-time reset and `InitVideoSys()`, then restarts them. |

**Done when:**
- `v4l2-compliance` failures drop from the baseline.
- `--query-dv-timings` matches the source, and returns `ENOLINK` with no signal.
- Switching the source's resolution raises a source-change event.
- `--set-parm 30` on a 59.94 fps source streams at 29.97.
- Picture controls visibly change the image, and register 91 reads back the values.
- An existing OBS source keeps working.
- After `systemctl suspend` and wake, capture resumes without a reboot.

## Stage 3: `rework/single-copy`

**Status (2026-10-19): done** on `rework/single-copy`; results in [results.md](results.md#stage-3-rework-single-copy). Probe 4.4 was not run first after all: the threaded IRQ, the per-consumer buffer bookkeeping and the queue removal are needed either way, and a positive 4.4 result would only replace the copy into the held buffer. The stray DMA (3.5) turned out to be over-long quarter runs at capture start and resolution changes, ending at register 9 × 16; fixed by programming the lengths before the engine starts and setting register 9 (see 4.9).

- A threaded IRQ replaces the video tasklet and workqueue.
- Each DMA half is copied straight into one buffer per consumer. A staging frame is used only for interlaced input or scaling.
- The 16-slot queue goes away (72 MiB).
- `remove()` and probe error paths are fixed, with MSI fallback, the correct `IRQF_SHARED` usage, and `alloc_ordered_workqueue`.
- The stray DMA writes to IOVA `0xfec00000` at capture start and stop are diagnosed through debugfs (DMA addresses and register readbacks) and then fixed. This is a data-safety item, not log noise. Only the MSI window (0xfee00000–0xfeefffff) is reserved in the card's IOMMU group, so 0xfec00000 can be mapped for any device in it (see Ground rules). No faults since stage 1 doesn't prove a fix: a write that now lands on a mapped address leaves no fault.
- A debugfs status file reports the input, the frame rate, per-consumer counters and IRQ counts.

**Done when:** stage 1 and 2 checks still pass, each frame is copied once per consumer, and no `IO_PAGE_FAULT` occurs at capture start or stop.

## Stage 4: hardware features and upstreaming

The in-tree `hws` driver (`drivers/media/pci/hws`, built as `hws.ko` in this 7.2 kernel) supports AVMatrix's multi-channel cards but not this one. It shares this card's register layout, and several registers it documents hold live values here ([REGISTERS.md](REGISTERS.md)).
- Items 4.1, 4.2 and 4.5 only read registers or change what the driver reports, so they can land before stage 3.
- Items 4.3 and 4.4 write registers this driver never writes, so they start as probes under the rules below.
- Item 4.8 needs no new registers.
- Item 4.9 copies a register write and a pause the Windows driver makes.
- Item 4.10 changes only what the driver delivers to ALSA.

| # | Change |
|---|---|
| 4.1 | **HDCP status and blanking.** Register 8 has a bit per channel that is set while the source uses HDCP (0 with the Switch 2). The Windows driver doesn't deliver video while it is set; apps get a frame reading "HDCP Protection" instead. This driver never reads register 8, so it passes on whatever the card sends. Match the vendor:<br>- While the bit is set, deliver a blank frame instead of the video.<br>- Report `V4L2_IN_ST_NO_ACCESS` in `ENUMINPUT`.<br>- Show it in `log_status`, and log one line when it changes.<br><br>Test on the fixed driver only, with the PS5's HDCP setting on and off: with it on, apps get only blank frames. Nothing here goes past detecting HDCP and blanking.<br><br>**Done (2026-10-19), but not seen to trigger:** with the PS5's HDCP on, register 8 stayed 0 and the card delivered the normal picture, so neither this driver nor the Windows driver would blank. The blanking stays for whatever input does set the bit. For capture, turn HDCP off on the console as Sony intends. |
| 4.2 | **Hardware frame rate.** Register 110 holds the input rate in whole fps (60 here). With no stream running, nothing is measured. The driver then reports either the last measured rate, which is stale if the source changed mode since, or a table value before the first stream.<br>- Use register 110 when there is no current measurement, or when the measurement differs from it by more than 1 fps.<br>- The measured interval still decides between 59.94 and 60.<br><br>The Windows driver reads register 110 as the input rate on FPGA 122 and later. First check that it follows the source while no stream runs. The PC survey showed why this matters: after each mode change, `QUERY_DV_TIMINGS` reported the previous mode's rate. |
| 4.3 | **Hardware scaler (probe).** Registers 120 and 130 read 1920×1080 and 60 here, although this driver never writes them and frames arrive at the input size. Mainline writes the requested size to register 120. On FPGA 122 and later, the Windows driver writes the app's requested size to 120 and rate to 130 on every status poll. But it still sizes the DMA from the input, scales on the CPU, and has no frame skipping of its own. So 120 likely doesn't change the delivered data, and 130 is the more promising half of the probe.<br><br>Probe, during a 4K stream:<br>1. Write 1920×1080 to register 120. Check whether the delivered data changes size (view a saved raw frame at a 1920-pixel stride).<br>2. Write 30 to register 130. Check whether the frame interrupts halve.<br><br>If either works, the driver uses it instead of the CPU scaler or frame skipping. Since the photos, the likely mechanism is the GSV2011's own 4K-to-2K downscaler, set up by the on-board microcontroller: watch whether register 90 (input size) changes after writing register 120.<br><br>**Done (2026-10-19): no-go.** Both registers read back what is written, but neither changes the input size, the input rate or the frame interrupt rate ([results.md](results.md#43-registers-120-and-130-probe)). The driver keeps its CPU scaler. A 1080p capture of a 4K source would then move a quarter of the data and spend no CPU on scaling. Only sizes up to the input size are written, since a smaller output can't overrun the buffers. |
| 4.4 | **Zero-copy. Done (2026-10-19) on `feat/zero-copy`:** with a translating IOMMU, OBS at 4K60 gets its frames without a CPU copy (IRQ thread CPU 0 ms against 270 ms per 5 s), the copy path stays for everything else, and register 9 now follows the quarter size ([results.md](results.md#44-zero-copy-round-1-driver-821f2d1-srcversion-3538ea2396bf2c18a6e6c22)). **Probe positive (2026-10-19):** the card picks up new addresses in registers 16–19 for every frame. With the IOMMU translating, all four windows map 0xE0000000–0xFFFFFFFF, and a test buffer allocated by the driver landed inside them, so app buffers can be targeted without touching the table ([results.md](results.md#44-zero-copy-probe-driver-5c8ee4f)). Next: the feature, per the requirements below.<br><br> Since stage 3 each frame is copied once per consumer, from the DMA buffers into the app's buffer (0.13 ms per 1080p frame and about 1 ms per 4K frame on a 9950X3D). Removing that copy matters more on weaker CPUs and smaller caches than on the development machine.<br><br>Mainline captures straight into app buffers. Before each frame, it reprograms the channel's address-table entry (BAR0 + 0x208 + 8·ch) and buffer address (register 16 + ch), and also writes register 26 + ch. On this card a frame arrives as four quarters through buffer registers 16–19. Register 26 falls in the audio-address range (24 + i), so it is not the video target here. The Windows driver has no zero-copy path either.<br><br>**Probe:** between frames, point registers 16–19 and their table entries at the four quarters of a test buffer. Check that the next frame lands there, and that the card picks up new addresses every frame.<br><br>**If the probe works, the feature must work on any system, not just the development machine:**<br>- **Fallback.** The copy path stays. Zero-copy is chosen per queue at REQBUFS, only when every buffer meets the card's limits: each quarter contiguous in DMA address space, and inside one address-table window, which doesn't cross a 512 MiB boundary. Otherwise the queue uses the copy path, with one log line saying why.<br>- **With an IOMMU** (translated or passthrough), `dma-contig` buffers are contiguous in IOVA space from scattered pages.<br>- **Without one** (`iommu=off`, or an Intel or AMD board with it disabled in firmware), a 16.6 MiB 4K frame needs physically contiguous memory, which usually means CMA. Most distributions configure little or none, so expect the copy path there.<br>- **Consumers and sizes.** The card writes into one consumer's buffer. The driver copies only for extra consumers, decimated consumers and scaled sizes. MMAP and DMABUF keep working; USERPTR uses the copy path.<br>- **Tests.** Run with the IOMMU translated, which is the only mode allowed for the probe. After the probe works, also run with `iommu=pt`, and check that the copy-path fallback is chosen when allocations can't meet the limits (forced with a debug parameter). |
| 4.5 | **Quantization.** With the Switch 2's RGB range on Automatic, Y spans 1–254, so the card delivered full range ([results.md](results.md)). The card's EDID marks both RGB and YCbCr quantization as selectable, so the console chooses the range; expect the card to pass it through.<br><br>**Done (2026-10-19):** it doesn't pass it through. The Switch 2 on Automatic, Limited and Full, and the PS5 on Limited (black at Y ≈ 1), all arrive full range: the card converts its input. The format now reports full-range Rec. 709. Find out whether that is fixed or follows the source: repeat the measurement with the console's RGB range set to Limited and to Full, on the Switch 2 and the PS5.<br>- If the card always delivers full range, report `V4L2_QUANTIZATION_FULL_RANGE` with Rec. 709 encoding.<br>- If it follows the source, keep the default and document the OBS setting per console. Detecting the range then needs the AVI InfoFrame (stage 5). |
| 4.6 | **Mainline support.** Add `8888:8581` to the in-tree `hws` driver so the card works without DKMS. The Windows driver serves 8888:8591, 8888:85A1, 1F33:8581 and 1F33:8591 with the same code, and the 8591 adds a line-in audio input. That needs:<br>- a single channel;<br>- 4K input in quarters;<br>- the audio ring;<br>- this card's version-register layout;<br>- the stage 1–3 fixes mainline lacks.<br><br>Build it out of tree from `drivers/media/pci/hws` first, and run the stage 1 and 2 checks against it. Sending patches to linux-media is public: only when asked. |
| 4.7 | **Upstream the fork's work** to GloriousEggroll's repo as the [upstream PR series](#upstream-pr-series), crediting PR #23. Only when asked. |
| 4.8 | **High refresh rates.** Retail listings give the input as up to 1440p144 and 1080p240, and the Windows driver offers capture modes up to 1440p144, 1080p240 and 720p240. The card's EDID advertises 1080p120, 1080p240, 1440p144 and 4096×2160p60, but not 1440p120 ([REGISTERS.md](REGISTERS.md#edid)), so a PS5 may only offer 120 Hz at 1080p. A PC's HDMI output can drive all of these modes for testing without a console.<br><br>**Survey done (2026-10-19, [results.md](results.md#stage-48-input-mode-survey-with-a-pc-source)):** 1080p240, 1440p144 and 1080p120 capture at full rate with no driver change. 4096×2160 arrives scaled to 3840×2160 and letterboxed by the card. What's left is the console check: which modes the PS5 and Switch 2 actually offer through this EDID.<br><br>**Done (2026-10-19):** the PS5 runs 120 Hz games at 1920×1080 at 119.88 Hz through this card, captured by OBS at 120 fps with 0.13 ms of CPU per frame ([results.md](results.md#48-120-hz-from-the-ps5-driver-2ef336a)). When the display's EDID advertises 120 Hz, the PS5 offers 1080p and 1440p at 120 Hz, and the Switch 2 offers 120 Hz in some games.<br>1. Survey which modes each console offers through the card (PS5: Settings → Screen and Video → Video Output Information).<br>2. Capture each one and run `framecheck.py` for the delivered rate, unique frames and DV timings.<br>3. Fix what breaks, such as the timings cap, the size table or rate snapping.<br><br>These modes fit the link: 1440p144 in YUYV is about 1.06 GB/s, close to the 1.0 GB/s that 4K60 already moves. When playing through the OBS preview, a 120 Hz capture halves the frame time, if OBS runs at 120 fps and the monitor refreshes at 120 Hz or more. |
| 4.9 | **Register 9 and the start pause.** On FPGA 122 and later, the Windows driver writes its 5 MiB buffer size to register 9, in 16-byte units. It also pauses 500 µs between the 0x80000000 and 0x80FFFFFF start writes to register 0. This driver does neither.<br>- **Register 9: done in stage 3, with a different value.** An over-long quarter run ends at register 9 × 16 plus a 2 KiB block, so the Windows value (the buffer size) would put that block past the end of the buffer. The driver sets buffer size − audio ring − 2 KiB.<br>- Add the pause.<br>- Consider polling input status every 50 ms like the Windows driver (this driver uses 250 ms), so source changes are seen sooner. Each poll costs only a few register reads.<br><br>Then check that capture still works at every size, including 4096×2160. |
| 4.10 | **Buzz while the source renegotiates.** On every console resolution change the card stops sending audio packets for 3–4 s (no audio interrupts), so the ALSA pointer stands still. PipeWire then re-delivers its last buffers of the card's audio over and over, a loop of 4864–5120 frames that is heard as a buzz. The same happens with no source, or with a source that sends no audio.<br>- **Fix:** once no packet has arrived for 40 ms (two packet periods), a delayed work delivers whole packets of silence at 48 kHz, counted from the last packet's time, until the card's packets resume.<br>- **Rejected first attempt:** muting packets whose content repeats. It never fired, because the driver never sees the loop.<br>- **Check:** `tests/staleaudio.py` lists replay loops and silence in a `pw-record` capture of the card's node. debugfs `status` counts the gaps filled. |

### Probe rules

- **Where probes run.** Probes that only change modes (4.3) run from a script (`tests/regprobe.py`) that maps BAR0 through sysfs (`resource0`). The released driver stays loaded, OBS is closed, and one `v4l2-ctl` stream observes. Probes that redirect DMA (4.4) run inside the driver, behind a debug-only module parameter that is off by default, because they need buffers the driver owns.
- **What a probe writes.** Only the registers named for it. It reads each one first and restores it on exit, including after Ctrl-C.
- **IOMMU.** A probe refuses to run unless the card's IOMMU domain is translated (`DMA` or `DMA-FQ`).
- **DMA addresses.** They come only from buffers the driver allocated; none are computed or guessed. Each test buffer is followed by a guard area at least its own size, checked after the probe, so a mis-sized write lands in memory the driver owns.
- **One probe per boot.** Reboot afterwards; loading the driver re-initialises the card.
- **Records.** Record the probe, dmesg, the `IO_PAGE_FAULT` count and the framecheck output in `docs/results.md`. Record the outcome, positive or not, in `docs/REGISTERS.md`.
- **Driver code** for a feature is written only after its probe works.

**Done when:**
- With the PS5's HDCP on, apps get only blank frames, and `--get-input` reports no access.
- After the source changes mode with no stream running, `--query-dv-timings` reports the new rate.
- Probes 4.3 and 4.4 are recorded, and features are built only on a positive result.
- The format reports the quantization the range test found.
- Every mode a console offers through the card captures at its full rate, or the limit is documented.

## Stage 5: interface discovery by reverse engineering

This stage needs vendor binaries that aren't public. AVMatrix publishes no SDK or firmware. The only Windows driver listing found is on a third-party site (`vc12-4k.inf`, version 6.0.0.4, December 2024). The goal is a register map complete enough to decide which other features the hardware has. Nothing in this stage modifies the card.

**Status (2026-10-19): the Windows driver is done.** AVMatrix's current Windows driver (6.0.0.4, 2026-03-17) has been analysed, and [REGISTERS.md](REGISTERS.md#windows-driver) has the results.
- It produced stage 4.1's blanking, 4.9, and confirmations for 4.2, 4.3, 4.4 and 4.8.
- It touches no registers beyond those listed, and has no EDID, I²C, InfoFrame, HDR, audio-format or temperature access.
- So the vendor driver offers no route to the EDID, InfoFrame, audio-format and status candidates below. Only the receiver chip's datasheet or a firmware image could.
- What remains is PCB photos and, if one turns up, a firmware image.

**Status (2026-10-19): chips identified (5.1).** Photos give an Artix-7 XC7A35T FPGA, a GSCoolink GSV2011 HDMI 2.0 transceiver, a Macronix MX25L3233F 4 MiB SPI flash, an unmarked 48-pin microcontroller, and no DRAM ([REGISTERS.md](REGISTERS.md#hardware)). The EDID, InfoFrame, HDR, audio-format and colour-range candidates belong to the GSV2011, configured by the microcontroller. The Windows driver gives the host no path to them, so they are **no-go** for this driver. Reading them would need the GSV2011 register map, which isn't public, and a host path into the microcontroller, which doesn't exist. The flash holds the FPGA bitstream, which the IDCODE alone already identifies, so a dump (5.5) adds little. The card's EDID, which decides which modes and formats a console offers, can be read with no risk: connect a PC's HDMI output to the card's input and read the EDID the PC sees (`/sys/class/drm/*-HDMI-A-*/edid`, decoded with `edid-decode`). Status, firmware build and FPGA temperature have no known register either. That leaves exact timings and output modes only through further register probing.

**Status (2026-10-19): stage 5 closed.** Items 5.4–5.6 were dropped by the operator decision as not worth the effort:
- **5.4, debugfs register dump:** the vendor driver reads no register beyond those already measured.
- **5.5, firmware analysis:** the flash holds the FPGA bitstream, and reading it would mean opening the card with a programmer.
- **5.6, per-candidate go/no-go:** settled by 5.1. The candidates live in the GSV2011, which the host can't reach.

**Why the Windows driver first.**
- The card is driven only through BAR0 registers and DMA, so the vendor's Windows driver is the fullest record of what the registers do.
- This driver descends from it ([REGISTERS.md](REGISTERS.md#vendor-ancestry)), so its functions should map onto ours closely.
- It is an ordinary x64 PE file, which Ghidra handles well.

| Input | Where from | What it can show |
|---|---|---|
| Windows driver package (`.inf`, `.sys`, any DLLs or utilities) | AVMatrix support. Third-party copies only to compare hashes. | Every register access and its purpose:<br>- property handlers for picture, HDCP, EDID, scaling, deinterlacing and colour<br>- I²C access to the HDMI receiver<br>- audio sample-rate handling<br>- version and update paths |
| Vendor tools (capture utility, firmware updater) | AVMatrix support | The flash layout and update protocol. Updaters often contain the firmware image. |
| PCB photos, both sides, heatsink off | The card | Part numbers for the FPGA, HDMI receiver, loop-out splitter and flash. Their datasheets bound what is possible. |
| Firmware image | An updater, or a read-only flash dump as a last resort | The EDID, any receiver or soft-CPU firmware, and the exact FPGA part from the bitstream |

**Method.**
1. Unpack packages on Linux without installing or running them, into an empty directory, and treat the contents as untrusted.
2. Analyse them statically with Ghidra (`rea`). Find the BAR0 accessors and list every offset read or written, with its caller. Name the callers from strings and property tables, and match them to this driver's functions.
3. Write the results into `docs/REGISTERS.md`: register, access, meaning, evidence (vendor function, mainline or measurement) and confidence.
4. Confirm on the card, read-only. A debug-only debugfs file dumps the registers the vendor driver reads, and only those, because reading an unknown register can have side effects.
5. Turn each feature found into a numbered item with a probe and checks, as in stage 4.

**Candidates.** Each needs evidence before work starts.
- **EDID.** Read it (`G_EDID`). If it lives in RAM rather than flash, let users load their own (`S_EDID`), for example to offer or hide 120 Hz, HDR or multichannel audio. An EDID stored in flash is out of scope.
- **InfoFrames.**
  - AVI quantization and colorimetry, for correct format reporting (4.5).
  - HDR detection from the Dynamic Range and Mastering InfoFrame, to report BT.2020/PQ or warn.
  - Content type (`V4L2_CID_DV_RX_IT_CONTENT_TYPE`).
- **Exact timings.** Pixel clock and blanking for `QUERY_DV_TIMINGS`, instead of presets or synthesized timings. Also 59.94 vs 60 without measuring.
- **Audio format.** Sample rate and channel count. The driver assumes 48 kHz stereo, so a 44.1 kHz source would play at the wrong speed if the card doesn't resample. Multichannel LPCM, if the card carries it.
- **Output modes.** Mainline describes register 0's start value (0x13) as selecting "YUYV/BT.709/etc". Other values may select the colour matrix, the range or another pixel format. Register 88 shows no YV12 support on this card.
- **Processing blocks.** A scaler, deinterlacer or frame-rate converter, if probe 4.3 is inconclusive.
- **Status.** Firmware build, HDCP version (status only), FPGA temperature.

**What it can't do.**
- Add hardware the card lacks. If the FPGA has no scaler, no register creates one.
- Exceed the link. 4K60 in YUYV already uses about 1 GB/s of the roughly 1.6 GB/s that PCIe 2.0 x4 carries in practice, so 4K120 (about 2 GB/s) can't fit.
- Anything with HDCP beyond reading its status.

**Firmware images.** If one turns up:
- Strings, version stamps and the EDID (which starts with `00 FF FF FF FF FF FF 00`) come out quickly.
- A 4K60 HDMI 2.0 input usually needs a dedicated receiver chip. Many have a small microcontroller whose firmware and EDID live in flash. That code is small and readable.
- The FPGA bitstream names the exact part. Turning a bitstream back into logic is research-level work, even for parts with open bitstream documentation, and isn't worth it for register discovery.

**Boundaries.**
- Interoperability only, on a card the user owns. Check the vendor's license terms first.
- Nothing vendor-owned goes in the repo: no binaries, firmware, disassembly or copied code. `REGISTERS.md` describes behaviour in our own words and cites evidence by function or address.
- HDCP: read status only. Nothing that disables, strips or works around HDCP is researched, documented or built.
- No writes to the card's flash: no firmware updates and no modified bitstreams. A flash dump, if ever needed, is read-only, from an unpowered card, with a 3.3 V programmer.
- Vendor binaries are analysed, never run.

**Done when:** `REGISTERS.md` covers every register the vendor driver uses, with meaning and confidence, and each candidate has a go or no-go with its evidence. Go items become numbered items with probes.

## Stage 6: fixes from the release audit

**Status (2026-10-20): stage 6 done.** Every item from 6.1 to 6.21 is closed on `fix/release-audit` and checked on the card, except 6.21, closed as a known issue. Results are in [results.md](results.md); the driver runs as DKMS `1.0.0.230324.6.9`. [AUDIT.md](AUDIT.md) has the evidence for each finding, with line numbers at `66d3773`. Each item below names the findings it closes. Items 6.1 to 6.4 and 6.14 must land before the first upstream PR (4.7) and before a public release (7.8).

**Where the fixes go (decided 2026-10-19).** Stage 6 work goes on a new branch, `fix/release-audit`, on top of `feat/zero-copy`. Stage 7 goes on `release/packaging`, on top of that. No pushed branch is rewritten. Upstream never gets these branches as they are: the [upstream PR series](#upstream-pr-series) rebuilds them from upstream `main` in small PRs, and folds each stage 6 fix into the PR whose code it fixes.

### Group A: crash fixes and safe defaults

| # | Change |
|---|---|
| 6.1 | **Gate zero-copy and ship it off.** LINUX-001, introduced on `feat/zero-copy`. The operator agreed to off by default (2026-10-19).<br>- `hws_zc_wanted()` returns false unless `m_DeviceHW_Version` and `m_MaxHWVideoBufferSize` are both set. These are the conditions under which `hws_set_dma_max()` writes register 9.<br>- `zero_copy` defaults to off. Its description says it needs FPGA 122 or later and a translating IOMMU.<br>- When `zero_copy` is on but zero-copy can't run, the driver logs one line with the reason, and debugfs `status` shows it.<br>- On the development machine, a host setting outside the repo keeps it on: `options HwsUHDX1Capture zero_copy=1` in `/etc/modprobe.d/`. |
| 6.2 | **No card re-init from the ALSA trigger.** LINUX-002, introduced by `46f7ec9` on `feat/hw-status`.<br>- `StartAudioCapture()` no longer calls `CheckCardStatus()`. Once 6.3 lands, the trigger is its only caller. It sets a flag instead when register 0 bit 0 is clear.<br>- At its next poll, within 50 ms, the status thread sees the flag and calls `InitVideoSys(pdx, 1)` under `cap_lock`, after it has moved zero-copy back to the DMA buffers.<br>- `StartVideoCapture()` runs in process context and keeps calling `CheckCardStatus()`. |
| 6.3 | **The audio work never writes a missing buffer.** LINUX-004 and LINUX-011. Path 2 came with `483648e` on `feat/input-status`, and the rest is upstream.<br>- `audio_data_process()` delivers packets only while `running` is set and `ring_size_byframes` is nonzero. Otherwise it frees the queue slots.<br>- `hw_free` calls `cancel_work_sync()` on the audio work as well as on the silence work, and `close` clears `drv->substream`.<br>- The trigger handles SUSPEND as it handles STOP. The PCM drops `SNDRV_PCM_INFO_RESUME`, so `snd_pcm_resume()` returns -ENOSYS and apps prepare and restart the stream.<br>- `hws_resume()` no longer restarts audio capture. The app's next trigger START does that. |
| 6.4 | **Unregister first, and free on the last close.** LINUX-003, upstream.<br>- `hws_remove()` runs in this order: set `m_PciDeviceLost`, call `snd_card_disconnect()` and `video_unregister_device()`, stop the status thread, stop the engines, free the IRQ, kill the tasklet, cancel the audio works, destroy `auwq`, free the DMA pool, unmap BAR0 and disable the device.<br>- `struct hws_pcie_dev` is freed only after the last V4L2 handle and the last ALSA handle close. A `kref` holds it. Probe takes one reference for itself, one for the `v4l2_device` and one for each sound card. Remove drops the first. The `v4l2_device` release callback and each card's `private_free` drop theirs.<br>- `snd_card_free_when_closed()` replaces `snd_card_free()`, so remove doesn't wait for PipeWire to close the PCM.<br>- Every register access that can run after remove checks `m_PciDeviceLost` first. Most already do. `hws_zc_program()` doesn't. |

**Done when:**
- In a test build that forces `m_DeviceHW_Version` to 0, debugfs reports zero-copy as unavailable, with the reason. On this card, `modinfo` shows `zero_copy` off by default, and `tests/zctest.sh` passes with `zero_copy=1`.
- On the debug kernel (6.16), audio starts cleanly after the 6.10 test knob clears register 0 bit 0: no splat, and capture runs again within 100 ms.
- An `arecord` running across `systemctl suspend` restarts its stream and keeps recording. An `arecord` stopped with SIGSTOP across a suspend and killed after the resume causes no oops. 10,000 audio start and stop cycles on the debug kernel cause no KASAN report.
- With `v4l2-ctl --stream-mmap` and `arecord` running, 20 sysfs unbind and bind cycles on the debug kernel cause no KASAN or kmemleak report. Both apps exit with an error, and capture works after the last bind.

### Group B: device lifecycle

| # | Change |
|---|---|
| 6.5 | **Probe error paths.** LINUX-015.<br>- `hws_audio_register()` stores each card right after `snd_card_new()`, so its error path frees it.<br>- `hws_probe()` stops the status thread before it unregisters the video node. |
| 6.6 | **A bounded busy wait.** LINUX-010. `Check_Busy()` gives up after 200 ms, logs one warning and returns -ETIMEDOUT. `StopDevice()` then carries on. |
| 6.7 | **The status thread across suspend.** LINUX-016.<br>- The status thread is freezable (`set_freezable()`, and it freezes between polls), so it doesn't poll a suspended card.<br>- A read of 0xFFFFFFFF from register 8 counts as no HDCP, as a read of 0xFFFFFFFF from register 5 already counts as no signal. |

**Done when:**
- Three test builds each fail probe in a different place: `snd_pcm_new()`, `snd_card_register()`, and `hws_audio_register()` as a whole. Each probe fails cleanly, kmemleak reports nothing, and the next module load works.
- In a test build that forces register 0 bit 3 set, remove finishes within 250 ms and logs one warning.
- After five suspend cycles with OBS open, the journal has no "HDCP active" line.

### Group C: zero-copy hardening

These items come after 6.1, and 6.8 needs 6.9.

| # | Change |
|---|---|
| 6.9 | **Wait for the DMA to stop.** R-02.<br>1. Probe, under the probe rules: with a debug module parameter and driver-owned buffers with guard areas, clear the video enable bit in the middle of a quarter run. Then watch the destination and its guard for 50 ms. Record whether writes land after the bit clears, and whether register 0 bit 3 follows DMA activity. Record the result in `results.md` and `REGISTERS.md`.<br>2. Then add `hws_video_quiesce()`. It clears the enable bit, reads register 2 back to flush the write, and then waits as long as the probe shows is needed: for bit 3 to clear, or for one frame period, at most 50 ms.<br>3. Every path that returns or frees a buffer the card may write calls it: the timeout path in `hws_stop_streaming_multi()`, `hws_suspend()`, `hws_remove()` and 6.8. |
| 6.8 | **An input-size change with zero-copy active.** R-01.<br>- When capture is running, `ChangeVideoSize()` stops the engine with `hws_video_quiesce()`, releases any zero-copy buffers, programs the new lengths and register 9, and restarts the engine. The frame in flight is lost, as it is today. (First written for zero-copy only; the switch runs showed the card also finishing an old-size quarter into a DMA buffer after the canary refill, so it covers the copy path too. See `results.md`.)<br>- For this path the status thread takes `engine_lock` before `cap_lock`. `hws_start_streaming_multi()` takes them in the same order, so the change adds no lock inversion.<br>- An optional probe, for the record: point a driver-owned 1080p test buffer with a guard at the card, switch the source to 4K, and check the guard. The result tells whether the old code could overrun. |
| 6.10 | **Debug features off by default.** LINUX-013.<br>- A new module parameter, `debug_dma`, defaults to off. It turns on the canary, the register snapshots, the stray-write log, and the test knobs that 6.2 and 6.11 need. With it off, the status thread doesn't scan.<br>- With `debug_dma` on, the scan runs without `cap_lock`. It holds the lock only to read `canary_start`.<br>- The scan logs the expected 2 KiB block at register 9 × 16 as an over-long run, not as a stray write. |
| 6.11 | **No repeated frame for the zero-copy consumer.** LINUX-014. At a top half that uses a zero-copy buffer, the driver completes any copy buffer held by the same consumer with `VB2_BUF_STATE_ERROR` before it copies. |

**Done when:**
- The 6.9 probe result is in `results.md` and `REGISTERS.md`.
- With `zero_copy=1`, run 20 source switches between 1080p and 4K in each direction, with the app capturing at the starting size. The kernel logs no `IO_PAGE_FAULT`, `debug_dma=1` reports no stray write, and `framecheck.py` shows no repeated sequence number.
- With `debug_dma=0`, 20 console mode changes log no "stray DMA write" line.
- The IRQ thread's wakeup latency at 1080p240 is recorded with `debug_dma` off and on, using `perf sched latency` or ftrace.

### Group D: hardware compatibility and API use

| # | Change |
|---|---|
| 6.12 | **Leave Relaxed Ordering to the PCI core.** LINUX-005. Remove `enable_pcie_relaxed_ordering()` and its call. |
| 6.13 | **No DMA sync calls on coherent memory.** LINUX-012. Remove the `dma_sync_single_for_cpu()` calls on the coherent DMA buffers. Add `dma_rmb()` where the code reads DMA data after a register read: at the start of `hws_video_half()`, and before the packet copy in `MemCopyAudioToSteam()`. |
| 6.14 | **Untested PCI IDs and interrupt bits.** R-03.<br>- The interrupt handler writes back every bit it reads from register 1, so no enabled source stays set. A debugfs counter counts bits other than 0 and 8. Bit 12 is the 8591's line-in audio, per `REGISTERS.md`.<br>- The three untested IDs keep matching, because upstream already claims them. On those IDs, probe logs one line saying the model is untested, and the 6.1 gate keeps zero-copy off. `README.md` lists them as untested (7.2). |
| 6.15 | **Catch a half the card overwrote during the copy.** R-06. When the copy of a half ends later than one half-frame interval after its interrupt, the frame completes with an error, and a debugfs counter counts it. The zero-copy code already keeps that interval. |

**Done when:**
- On this machine, `lspci -vv` shows `RlxdOrd` as the PCI core leaves it. A 10-minute 4K60 capture on each path gives the same `framecheck.py` result as the stage 3 and 4.4 runs.
- The debug kernel (6.16) logs no DMA API warning from the driver.
- On this card, the unknown-interrupt counter stays at 0 through a 10-minute capture.
- Under `stress-ng --cpu 0`, at 4K60 and at 1080p240, the late-copy counter rises, and the sequence gaps in `framecheck.py` match it.

### Group E: verification and cleanup

| # | Change |
|---|---|
| 6.16 | **A debug-kernel pass.** This needs a kernel with `CONFIG_PROVE_LOCKING`, `CONFIG_DEBUG_ATOMIC_SLEEP`, `CONFIG_DMA_API_DEBUG`, `CONFIG_KASAN` and `CONFIG_DEBUG_KMEMLEAK`. Installing one is a host change, so it needs the operator's OK. Run the checks in groups A to D that name the debug kernel, `tests/zctest.sh`, and `v4l2-compliance -s` with no other app open. |
| 6.17 | **A hardware validation pass.** R-04 and R-05. Record each result in `results.md`:<br>- the resolution matrix: 720p, 1080p, 1440p, 4K, 1080p120 and 1080p240, switched in both directions, with `zero_copy` off and on;<br>- `iommu=pt` and `iommu=off`: zero-copy reports unavailable and the copy path delivers. With the IOMMU off, the 4 MiB fallback captures 4096-wide input at 3840;<br>- a 44.1 kHz source, such as a PC's HDMI output set to 44.1 kHz: the rate and pitch the card delivers;<br>- a second machine that isn't an X3D part, if one is available. Otherwise record this as not done;<br>- an 8-hour OBS session at 4K60: memory use, `IO_PAGE_FAULT` count, PipeWire errors, and the delivered rate at the start and the end. |
| 6.19 | **Recover a lost run state while capturing.** Found in the 6.2 test: only an audio start noticed that register 0 bit 0 had cleared, so video stayed stopped without one. The status thread now checks the bit on every poll while either engine runs, and re-initialises the card in the same poll. Folds into PR-17 with 6.2. |
| 6.20 | **Drop the stale audio packets at an audio start.** Found in 6.17: every capture began with one or two packets (1,024 frames each, 21–43 ms) of the audio the card captured before the last disable, even across a module reload. Clearing the driver's ring didn't help, so the card itself sends them. `StartAudioCapture()` now drops the first two packets; the silence fill covers any gap. Test: alternate 1 kHz and 500 Hz tones from the PC's HDMI output with a capture of each; every capture starts with its own tone. |
| 6.21 | **GPU lost during HDMI mode changes while capturing. Closed as a known issue (2026-10-20), see `results.md`.** Found in 6.17 and 6.10: three times the PC's GPU dropped off the PCIe bus while its HDMI output to the card changed mode, locking up the desktop: twice during a capture, once with nothing capturing 3 s after a driver reload. About 66 changes with no capture and no reload just before, and 120 Switch 2 changes during capture, never did. Find whether 6.8's engine restart (`bf3ede3`) is involved: with the operator's OK, make GPU mode changes during a capture with DKMS 6.3 (before `bf3ede3`). If 6.3 never loses the GPU, rework the restart; if it does, record it as a receiver or amdgpu limit in the README. Evidence in `results.md` (6.17). |
| 6.18 | **Remove dead code.** LINUX-020. This item comes last in the stage, so it can't conflict with the fixes. Remove the per-device queue and its ops, the unused fields, definitions and includes, and the `#if 0` blocks. The fixed-size scalers stay. |

**Done when:**
- The debug kernel logs no report that names the driver during 6.16.
- Each 6.17 result is in `results.md`. A failure there becomes a new numbered item.
- After 6.18, the stage 2 and 4.4 checks give the same results as before, and the W=1 build logs fewer warnings.

## Stage 7: public release

**Status (2026-10-20): 7.1 and 7.4 to 7.7 done on `release/packaging`, which is pushed; 7.2 waits on an independent reader; 7.8 tags 1.1.0.** Stage 7 covers what a user outside the development machine needs. It changes scripts, docs, the build files and CI. Under `src/` it was meant to change only `src/Makefile`; 7.5 also moved the scalers and the pure calculations into their own files so tests can build them, and its tests found a scaler bug that was fixed there (see `results.md`). Item 7.8 is the release itself and comes last.

| # | Change |
|---|---|
| 7.1 | **One install and one uninstall path.** LINUX-006, LINUX-007 and LINUX-009.<br>- `install.sh` installs through DKMS only. It copies a clean tree into `/usr/src/HwsUHDX1CaptureForLinux-<version>/`, then runs `dkms add`, `dkms build` and `dkms install`. If `mokutil --sb-state` reports Secure Boot on, it prints the steps to enrol the DKMS signing key.<br>- The release version sorts above the development versions (`1.0.0.230324.<stage>.<n>`), so DKMS builds it for new kernels.<br>- `uninstall.sh` removes every `HwsUHDX1CaptureForLinux` version and the old `HwsUHDX1Capture.ko` package from DKMS. It deletes `/usr/local/share/HWS`, the `/usr/bin/hws-*.sh` links, the old `/usr/src` link, and any `HwsUHDX1Capture.ko*` left under `/lib/modules/`. Then it runs `depmod` for each kernel. It asks for a reboot only if `/sys/module/HwsUHDX1Capture` exists.<br>- `scripts/` is removed. `install.sh` and `uninstall.sh` at the top level hold the logic, and the top-level `dkms-install.sh` calls `install.sh`.<br>- `tools/dkms-install.sh` gets a `--remove <version>` option. At a kernel whose build link points nowhere, it warns and skips that kernel.<br>- Every script quotes its expansions, re-runs itself with `exec sudo "$0" "$@"`, writes no file in the working directory, and passes shellcheck. |
| 7.2 | **User documentation.** LINUX-008. `README.md` becomes the user guide. It covers:<br>- a known issue (6.21): with the PC's own GPU feeding the card, changing that output's resolution while capturing, or just after the driver loads, has made an RX 9070 drop off the PCIe bus and lock up the desktop; consoles and other PCs as sources aren't affected as far as tested;<br>- the supported hardware: 8888:8581 tested with FPGA 122, the other IDs untested, and x86-64 as the only tested architecture;<br>- the kernels and distributions, as listed by 7.4;<br>- the packages each distribution needs, as checked in the 7.1 VMs;<br>- Secure Boot and MOK enrolment;<br>- install, a first check (`v4l2-ctl --list-devices`, `arecord -l` and the kernel log), upgrade and uninstall;<br>- OBS setup: the device "HUHDVideo 1", Color Range Full, and the audio source;<br>- `zero_copy`: what it does, when it is safe, and how to turn it on;<br>- troubleshooting: no device, "Key was rejected by service", blue frames with no signal, black frames with HDCP, and silence while the source changes mode;<br>- the limitations: YUYV only, one input, 48 kHz stereo audio, no HDR or 10-bit, input up to 3840×2160 at 60 Hz and 1920×1080 at 240 Hz (the card scales a 4096×2160 source to 3840×2160), and CPU scaling for any output size other than the input's.<br><br>`quick_start.txt` and `doc/` are vendor material. The README either marks them as vendor material, or they are removed. The developer docs in `docs/` get one line in the README. |
| 7.3 | **Licence terms. Dropped (operator decision, 2026-10-19):** the licence stays as it is. LINUX-018 stays recorded in the audit, with no change planned. |
| 7.4 | **A CI build matrix.** LINUX-019 and R-07.<br>- A GitHub Actions workflow on the fork builds the module in containers against the kernel headers of Fedora (current), Ubuntu 24.04 and 22.04, Debian 12, Arch (current), and CentOS Stream 9 and 10. CentOS Stream covers RHEL's backported APIs.<br>- Each job passes the header directory to `make` explicitly, because `uname -r` in a container returns the host's kernel.<br>- The same workflow runs shellcheck on every script, plus the Python tests that need no card and the 7.5 tests. It reports checkpatch results without failing on them.<br>- Pushing the workflow to the fork is the operator's call.<br>- Where the code fails on an older kernel, it either gets a version guard or the minimum kernel rises. The README says which. |
| 7.5 | **Tests without the card.** LINUX-019. Move the pure calculations into functions that a userspace harness can build, and test them under ASan and UBSan:<br>- the quarter sizes from `SetVideoFormteSize()`;<br>- the bound in `hws_zc_buffer_ok()`;<br>- the rate snapping in `hws_input_timeperframe()` and the windows in `hws_frame_end()`;<br>- the preset match in `hws_query_timings()`;<br>- the zero-copy release counters;<br>- that every scaler writes exactly `out_w × out_h × 2` bytes. |
| 7.6 | **Probe tools check the device.** LINUX-017. `tests/regprobe.py` requires `--pci`. It refuses to run unless that device's `vendor` and `device` files read 0x8888 and 0x8581, and the driver bound to it is `HwsUHDX1Capture`. `tests/zctest.sh` finds the device under `/sys/bus/pci/drivers/HwsUHDX1Capture/`, as `tools/irq-affinity` does. |
| 7.7 | **Build hygiene.** LINUX-021. Add a `.gitignore` for build output, `hws_install.log` and `__pycache__`. `src/Makefile` uses `ccflags-y` and drops `EXTRA_LDFLAGS`. `src/load.sh`, which nothing references, is removed. |
| 7.8 | **The release.**<br>- A `CHANGELOG.md` lists the user-visible changes of each stage.<br>- A `SECURITY.md` gives a contact that the maintainer chooses.<br>- A tag `v<version>` matches `PACKAGE_VERSION` in `src/dkms.conf`.<br>- The release notes list the tested hardware, the tested kernels and the limitations.<br>- The release needs 6.1 to 6.4, 6.14, 7.1, 7.2 and 7.4. |

**Done when:**
- **7.1:** In VMs running Fedora 44, Ubuntu 24.04 and Arch Linux: install, reboot, and `modinfo HwsUHDX1Capture` shows the release version. Then uninstall: `modinfo` fails, `dkms status` lists nothing for the driver, and none of the paths in 7.1 exist. Both scripts work when run from `/tmp/a b/`. On this machine with the card: install over the development versions, reboot, and OBS captures.
- **7.2:** Someone who hasn't worked on the driver follows the README alone on a fresh VM to a working install. Every command in the README has been run.
- **7.4:** The workflow passes, and the README's kernel list comes from it.
- **7.5:** `make -C tests/unit check` passes in CI. Each test fails when the bound it checks breaks, for example with `HWS_ZC_SLACK` below 4096, the smallest slack that fits every input size.
- **7.6:** A run with another device's address exits before it opens `resource0`.
- **7.7:** After a build and a test run, `git status` is clean, and the W=1 build log is unchanged.
- **7.8:** The tagged tree passes CI and installs in the 7.1 VMs.

## Upstream PR series

**Status (2026-10-20): seven PRs, branches pushed to the fork, not yet opened.** The operator chose seven PRs over the 31 planned before. Upstream merges rarely (every few months), and 25 of the 31 formed one chain that could only merge one at a time, so seven PRs that stack in order replace them.

### How the PRs are built

- **Cut along the tested history.** Each PR is one or two of the fork's stage branches, unchanged, so the tree at the end of every PR is a tree that ran on the card. If upstream stops after any PR, it has a tested driver. The earlier plan folded stage 6 fixes into the PRs whose code they fix; that would have produced intermediate trees that never ran, so the fixes stay in PR 6.
- **One commit left out:** `7c64b98` turned zero-copy on by default, and only PR 6 turned it off again. Without it, a tree that stops at PR 5 keeps zero-copy off. PRs 6 and 7 were restacked on the corrected PR 5; the one conflict (the parameter description) resolves to PR 6's text, and the final tree is unchanged.
- **No build output.** Stages 6 and 7 committed kernel build output and a `.pyc` by mistake. The PR branches `upstream/6-audit-fixes` and `upstream/7-packaging` are copies with those files filtered out of every commit; their final tree equals the `v1.1.0` release. The fork's own branches keep their history.
- **Docs go with the code.** The commits reference `docs/SPEC.md`, `docs/results.md` and `docs/AUDIT.md`, so those files ride along and the references resolve. Upstream may ask to drop them; they hold no code.
- **Built at every tip:** each PR branch builds on kernel 7.2.11 and on Ubuntu 24.04's 6.8 (`tools/ci-build.sh`), checked 2026-10-20. PR 7 adds CI for nine distributions.
- **Base:** upstream `main` at `bd17e06`, unchanged since 2026-06-29.

### Merge order

The PRs stack: each branch contains every commit of the ones before it. Upstream must merge them in order, 1 to 7. Until a PR's predecessors merge, GitHub shows their commits in its diff too; each description lists the commits that are its own. Once a predecessor merges with a merge commit (upstream's habit), its commits drop out of the next PR's diff by themselves. If upstream squashes or rebases a PR instead, the next branch needs a rebase onto the new `main`, which the fork does on request.

| PR | Branch | Topic | Commits | Changed (vs. previous) | Merge after |
|---|---|---|---|---|---|
| 1 | `upstream/1-audio-batch` | Audio reported as BATCH, periods in whole packets | 1 | 1 file, +25 −4 | upstream `main` (stands alone) |
| 2 | `upstream/2-frame-delivery` | Frame delivery: no deadlock, one frame per program, timestamps, teardown, DMA sizing (stage 1) | 13 | 7 files, +887 −568 | PR 1 |
| 3 | `upstream/3-input-status` | Input information: DV timings, measured rate, native size, frame intervals, controls, suspend/resume (stage 2) | 23 | 8 files, +960 −777 | PR 2 |
| 4 | `upstream/4-capture-rework` | Threaded-IRQ capture into app buffers, HDCP, register 9, silence fill, high refresh rates (stages 3 and 4) | 36 | 10 files, +1496 −1036 | PR 3 |
| 5 | `upstream/5-zero-copy` | Zero-copy, off by default (stage 4.4) | 7 | 8 files, +717 −34 | PR 4 |
| 6 | `upstream/6-audit-fixes` | Fixes from the release audit, dead code removal (stage 6) | 65 | 11 files, +1506 −772 | PR 5 |
| 7 | `upstream/7-packaging` | Installer, CI, unit tests, README, changelog (stage 7) | 22 | 34 files, +2894 −2636 | PR 6 |

PR 1 is small and stands alone, a first merge that shows the series works. PR 5 is optional: upstream can decline zero-copy, but PRs 6 and 7 contain fixes to it, so declining 5 means rebasing 6 and 7 without it. Open upstream PRs to mention: #23 (kubax, duplicate frames), which PR 2 supersedes and credits, and #25 (`.deb` packaging), which touches the install area PR 7 rewrites.

### Writing the PR description

The operator writes each description, short and in their own words. Reviewers distrust long generated text. Explain every change plainly instead. A description has five parts:

1. **Problem.** What a user sees on current `main`, and how to reproduce it, in one or two sentences and a command.
2. **Changes.** One bullet per commit: what it changes, why, and in which functions. A large commit gets a short paragraph for each part.
3. **Testing.** The hardware, kernel and source used, the commands that were run, and the results. Give numbers where they matter, such as fps, unique frames or fault counts.
4. **Not covered.** What wasn't tested, and the known caveats.
5. **Related.** The PR this one depends on, and any PR it supersedes or credits, such as upstream PR #23.

Before a PR opens, walk through its diff hunk by hunk until you can explain each hunk without notes. If you can't explain a hunk, resolve it first: simplify it, explain it in a code comment, or drop it. Each PR's bead lists the review questions to expect.

### Opening them

The fork can't open PRs on upstream; the operator opens each from the fork's branch on GitHub (the compare links are in the PR briefs, kept outside the repository). Open PR 1 for review. Open PRs 2 to 7 at the same time as drafts, or one at a time after the previous one merges; either way, each description states what it depends on.

## Host integration

These items live outside the driver, in the user's setup or as helper scripts in `tools/`.

| # | Item |
|---|---|
| H.1 | **OBS source settings and console output.** The console side: turn HDR and VRR off for capture. The card's EDID advertises both, but the card delivers 8-bit SDR and the driver assumes a fixed frame rate ([REGISTERS.md](REGISTERS.md#edid)). The OBS side: all three of these are already set on this machine:<br>- Use Buffering off, for the lowest latency when playing through the preview.<br>- Color Range Full, to match the measured output (4.5).<br>- DV Timing "Leave Unchanged", so the driver's live timings apply. |
| H.2 | **IRQ placement (opt-in helper, any CPU). Done (2026-10-19):** with 4K60 capture, a V-cache-bound benchmark lost a third of its throughput on average while the IRQ sat on the V-cache CCD, and none on the other CCD ([results.md](results.md#h2-irq-placement-on-the-9950x3d-driver-1f3e39a)). `tools/irq-affinity install` is in place on the development machine.<br><br> On the development machine the card's interrupt (IRQ 202, `0000:0b:00.0`) runs on CPU 4, on the 9950X3D's V-cache CCD (CPUs 0–7 and 16–23, 96 MiB L3), where games run best. Since stage 3 the threaded handler does the per-frame copy on the IRQ's CPUs, about 1 GB/s at 4K60, through the game CCD's cache.<br><br>Most users won't have this layout, so this is not a driver change: the driver keeps the kernel's default affinity, and the threaded handler follows whatever affinity is set. Instead, add an opt-in helper, `tools/irq-affinity`:<br>- It reads the L3 domains from `/sys/devices/system/cpu/cpu*/cache/index3/`.<br>- It acts only when they differ in size (AMD X3D parts with one V-cache CCD). It moves the card's IRQ to the CPUs of the other domain, and stops irqbalance moving it back.<br>- On one L3 domain (most desktop CPUs, all single-CCD parts) or symmetric domains (dual V-cache, non-X3D multi-CCD), it changes nothing and says why. A `--cpus` option takes an explicit list for other cases.<br>- The setting must survive reboots and the IRQ number changing.<br><br>**Measure before keeping it:** with a CPU-heavy PC game running while the card captures 4K60, compare the game's frame times and the IRQ thread's CPU time with the helper on and off. If there's no measurable difference, document that and drop it. |
| H.3 | **OBS follows the input. Skipped (operator decision, 2026-10-19).** When the console changes resolution, the driver raises a source-change event. OBS keeps the old format until the source reopens, and the CPU scaler fills the gap.<br><br>A user service (`tools/obs-follow-input`) waits for the event and makes OBS reopen the source through obs-websocket, which is built into OBS 28 and later. OBS restarts a V4L2 source when one of its settings changes, so the helper writes the new input size into the source's settings. Check that OBS then picks up the new size before relying on it. |
| H.4 | **DKMS builds for every kernel. Done (2026-10-19).** `tools/dkms-install.sh <ref> <version>` unpacks a git ref into `/usr/src/HwsUHDX1CaptureForLinux-<version>/`, records the commit in `SOURCE_COMMIT`, and builds and installs it for every kernel with headers. Versions sort numerically, so AUTOINSTALL builds the newest for new kernels: `1.0.0.230324.<stage>.<n>`. Installed: `1.0.0.230324.3.0` (stage 3, `d42ecb5`) on 7.2.10, 7.2.9 and 7.2.6; built only: `1.0.0.230324.2.0` (stage 2) as the rollback. The old shared `1.0.0.230324` version is gone from DKMS. |
| H.5 | **Slot and IOMMU group. Dropped (operator decision, 2026-10-19):** there's no other usable slot, because the graphics card blocks the case's other slots. The card stays in the chipset slot in IOMMU group 22. 3.5 fixed the stray writes, and the IOMMU stays translating. |

## Build and test loop

1. Compile against each `/lib/modules/*/build`.
2. Install: `tools/dkms-install.sh <ref> 1.0.0.230324.<stage>.<n>` with a new version number, then reboot.
3. Run the stage's checks and record the results in `docs/results.md`.
