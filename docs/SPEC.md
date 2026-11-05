# VC12-4K driver fixes and features

This spec covers fixes and features for the HwsUHDX1Capture driver (AVMatrix VC12-4K, PCI `8888:8581`). It is split into a baseline and five stages, plus host settings outside the driver. Each stage lists its changes and how to check them. Stages 4 and 5 are proposals: some stage 4 items are experiments, and stage 5 depends on getting vendor binaries. [REGISTERS.md](REGISTERS.md) lists what is known about the card's registers.

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
  - one new branch for each later stage

  Each branch becomes one PR to upstream `main`, opened in order. After a PR merges, rebase the next branch onto upstream `main` so its PR shows only its own stage. The stages don't touch the audio code. Commits that change docs never change `src/`, so a PR can keep or drop the docs.
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
| 4.4 | **Zero-copy (probe).** **Probe positive (2026-10-19):** the card picks up new addresses in registers 16–19 for every frame. With the IOMMU translating, all four windows map 0xE0000000–0xFFFFFFFF, and a test buffer allocated by the driver landed inside them, so app buffers can be targeted without touching the table ([results.md](results.md#44-zero-copy-probe-driver-5c8ee4f)). Next: the feature, per the requirements below.<br><br> Since stage 3 each frame is copied once per consumer, from the DMA buffers into the app's buffer (0.13 ms per 1080p frame and about 1 ms per 4K frame on a 9950X3D). Removing that copy matters more on weaker CPUs and smaller caches than on the development machine.<br><br>Mainline captures straight into app buffers. Before each frame, it reprograms the channel's address-table entry (BAR0 + 0x208 + 8·ch) and buffer address (register 16 + ch), and also writes register 26 + ch. On this card a frame arrives as four quarters through buffer registers 16–19. Register 26 falls in the audio-address range (24 + i), so it is not the video target here. The Windows driver has no zero-copy path either.<br><br>**Probe:** between frames, point registers 16–19 and their table entries at the four quarters of a test buffer. Check that the next frame lands there, and that the card picks up new addresses every frame.<br><br>**If the probe works, the feature must work on any system, not just the development machine:**<br>- **Fallback.** The copy path stays. Zero-copy is chosen per queue at REQBUFS, only when every buffer meets the card's limits: each quarter contiguous in DMA address space, and inside one address-table window, which doesn't cross a 512 MiB boundary. Otherwise the queue uses the copy path, with one log line saying why.<br>- **With an IOMMU** (translated or passthrough), `dma-contig` buffers are contiguous in IOVA space from scattered pages.<br>- **Without one** (`iommu=off`, or an Intel or AMD board with it disabled in firmware), a 16.6 MiB 4K frame needs physically contiguous memory, which usually means CMA. Most distributions configure little or none, so expect the copy path there.<br>- **Consumers and sizes.** The card writes into one consumer's buffer. The driver copies only for extra consumers, decimated consumers and scaled sizes. MMAP and DMABUF keep working; USERPTR uses the copy path.<br>- **Tests.** Run with the IOMMU translated, which is the only mode allowed for the probe. After the probe works, also run with `iommu=pt`, and check that the copy-path fallback is chosen when allocations can't meet the limits (forced with a debug parameter). |
| 4.5 | **Quantization.** With the Switch 2's RGB range on Automatic, Y spans 1–254, so the card delivered full range ([results.md](results.md)). The card's EDID marks both RGB and YCbCr quantization as selectable, so the console chooses the range; expect the card to pass it through.<br><br>**Done (2026-10-19):** it doesn't pass it through. The Switch 2 on Automatic, Limited and Full, and the PS5 on Limited (black at Y ≈ 1), all arrive full range: the card converts its input. The format now reports full-range Rec. 709. Find out whether that is fixed or follows the source: repeat the measurement with the console's RGB range set to Limited and to Full, on the Switch 2 and the PS5.<br>- If the card always delivers full range, report `V4L2_QUANTIZATION_FULL_RANGE` with Rec. 709 encoding.<br>- If it follows the source, keep the default and document the OBS setting per console. Detecting the range then needs the AVI InfoFrame (stage 5). |
| 4.6 | **Mainline support.** Add `8888:8581` to the in-tree `hws` driver so the card works without DKMS. The Windows driver serves 8888:8591, 8888:85A1, 1F33:8581 and 1F33:8591 with the same code, and the 8591 adds a line-in audio input. That needs:<br>- a single channel;<br>- 4K input in quarters;<br>- the audio ring;<br>- this card's version-register layout;<br>- the stage 1–3 fixes mainline lacks.<br><br>Build it out of tree from `drivers/media/pci/hws` first, and run the stage 1 and 2 checks against it. Sending patches to linux-media is public: only when asked. |
| 4.7 | **Upstream the fork's branches** to GloriousEggroll's repo: one PR per topic, rebased on upstream `main`, crediting PR #23. Only when asked. |
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

## Host integration

These items live outside the driver, in the user's setup or as helper scripts in `tools/`.

| # | Item |
|---|---|
| H.1 | **OBS source settings and console output.** The console side: turn HDR and VRR off for capture. The card's EDID advertises both, but the card delivers 8-bit SDR and the driver assumes a fixed frame rate ([REGISTERS.md](REGISTERS.md#edid)). The OBS side: all three of these are already set on this machine:<br>- Use Buffering off, for the lowest latency when playing through the preview.<br>- Color Range Full, to match the measured output (4.5).<br>- DV Timing "Leave Unchanged", so the driver's live timings apply. |
| H.2 | **IRQ placement (opt-in helper, any CPU).** On the development machine the card's interrupt (IRQ 202, `0000:0b:00.0`) runs on CPU 4, on the 9950X3D's V-cache CCD (CPUs 0–7 and 16–23, 96 MiB L3), where games run best. Since stage 3 the threaded handler does the per-frame copy on the IRQ's CPUs, about 1 GB/s at 4K60, through the game CCD's cache.<br><br>Most users won't have this layout, so this is not a driver change: the driver keeps the kernel's default affinity, and the threaded handler follows whatever affinity is set. Instead, add an opt-in helper, `tools/irq-affinity`:<br>- It reads the L3 domains from `/sys/devices/system/cpu/cpu*/cache/index3/`.<br>- It acts only when they differ in size (AMD X3D parts with one V-cache CCD). It moves the card's IRQ to the CPUs of the other domain, and stops irqbalance moving it back.<br>- On one L3 domain (most desktop CPUs, all single-CCD parts) or symmetric domains (dual V-cache, non-X3D multi-CCD), it changes nothing and says why. A `--cpus` option takes an explicit list for other cases.<br>- The setting must survive reboots and the IRQ number changing.<br><br>**Measure before keeping it:** with a CPU-heavy PC game running while the card captures 4K60, compare the game's frame times and the IRQ thread's CPU time with the helper on and off. If there's no measurable difference, document that and drop it. |
| H.3 | **OBS follows the input. Skipped (operator decision, 2026-10-19).** When the console changes resolution, the driver raises a source-change event. OBS keeps the old format until the source reopens, and the CPU scaler fills the gap.<br><br>A user service (`tools/obs-follow-input`) waits for the event and makes OBS reopen the source through obs-websocket, which is built into OBS 28 and later. OBS restarts a V4L2 source when one of its settings changes, so the helper writes the new input size into the source's settings. Check that OBS then picks up the new size before relying on it. |
| H.4 | **DKMS builds for every kernel. Done (2026-10-19).** `tools/dkms-install.sh <ref> <version>` unpacks a git ref into `/usr/src/HwsUHDX1CaptureForLinux-<version>/`, records the commit in `SOURCE_COMMIT`, and builds and installs it for every kernel with headers. Versions sort numerically, so AUTOINSTALL builds the newest for new kernels: `1.0.0.230324.<stage>.<n>`. Installed: `1.0.0.230324.3.0` (stage 3, `d42ecb5`) on 7.2.10, 7.2.9 and 7.2.6; built only: `1.0.0.230324.2.0` (stage 2) as the rollback. The old shared `1.0.0.230324` version is gone from DKMS. |
| H.5 | **Slot and IOMMU group. Dropped (operator decision, 2026-10-19):** there's no other usable slot, because the graphics card blocks the case's other slots. The card stays in the chipset slot in IOMMU group 22. 3.5 fixed the stray writes, and the IOMMU stays translating. |

## Build and test loop

1. Compile against each `/lib/modules/*/build`.
2. Install: `tools/dkms-install.sh <ref> 1.0.0.230324.<stage>.<n>` with a new version number, then reboot.
3. Run the stage's checks and record the results in `docs/results.md`.
