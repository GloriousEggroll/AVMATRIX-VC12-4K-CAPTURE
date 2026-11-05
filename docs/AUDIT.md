# Release audit, 2026-10-19

This file records the findings of a release-readiness audit of the driver at commit `66d3773` (branch `feat/zero-copy`). [SPEC.md](SPEC.md) stages 6 and 7 hold the changes that address them, and each finding below names its spec item. Line numbers refer to `src/hws_video.c` at `66d3773` unless another file is named.

## Scope

- **Read in full:** every function in `src/hws_video.c` except the CPU scalers, plus `src/hws.h`, `src/hws_reg.h`, `src/Makefile`, `src/dkms.conf`, all of `scripts/`, the install wrappers at the top level, `tools/`, and `tests/build-all-kernels.sh`, `run-baseline.sh`, `zctest.sh`, `regprobe.py` and `quarter_sizes.py`.
- **Read in part:** [results.md](results.md) (stage 0 and the zero-copy sections), [REGISTERS.md](REGISTERS.md) (registers 0 and 1, the Windows driver notes) and the CPU scalers (`VideoScaler()`, `All_VideoScaler()`, `UHD_TO_FHD_Scaler()`, `UHD_To_HD_Scaler()` and `FHD_To_HD_Scaler()`).
- **Not read:** the other 25 fixed-size scalers, `doc/`, `play/`, `tests/staleaudio.py` and `tests/yrange.py`.
- **Not run:** anything that opens the card. Hardware results quoted here come from [results.md](results.md).

Each finding has three ratings:

- **Severity:** Critical, High, Medium or Low, by demonstrated impact.
- **Confidence:** High means the source or a command confirms it. Medium means the code path is confirmed but the trigger or the hardware behaviour is not.
- **Origin:** "stack" means a commit between upstream `main` (`bd17e06`) and `66d3773` introduced it. "Upstream" means `main` already had it.

## Findings

| ID | Severity | Confidence | Origin | Finding | Spec |
|---|---|---|---|---|---|
| LINUX-001 | High | High | stack | Zero-copy runs on cards where its safety bound is never programmed | 6.1 |
| LINUX-002 | Medium | High | stack | `usleep_range()` is reachable from the atomic ALSA trigger | 6.2 |
| LINUX-003 | Medium | High | upstream | Unbind with open handles: use-after-free and DMA into freed buffers | 6.4 |
| LINUX-004 | Medium | Medium | both | The audio work writes a PCM buffer that is gone | 6.3 |
| LINUX-005 | Medium | Medium | upstream | Relaxed Ordering is forced on against the PCI core's quirk | 6.12 |
| LINUX-006 | Medium | High | both | Uninstall leaves the module installed | 7.1 |
| LINUX-007 | Medium | High | upstream | Installers run unquoted paths as root | 7.1 |
| LINUX-008 | Medium | High | upstream | The README is stale and wrong for users | 7.2 |
| LINUX-009 | Low | High | upstream | The installer reports success while the old module stays loaded | 7.1 |
| LINUX-010 | Low | High | upstream | `Check_Busy()` can loop forever | 6.6 |
| LINUX-011 | Low | High | both | The PCM advertises RESUME but rejects SUSPEND and RESUME | 6.3 |
| LINUX-012 | Low | High | upstream | DMA sync calls on coherent memory | 6.13 |
| LINUX-013 | Low | Medium | stack | The debug canary scan runs in every build | 6.10 |
| LINUX-014 | Low | Medium | stack | The zero-copy consumer can get one frame twice | 6.11 |
| LINUX-015 | Low | High | both | Leak and ordering bugs in probe error paths | 6.5 |
| LINUX-016 | Low | Medium | both | The status thread keeps polling across suspend | 6.7 |
| LINUX-017 | Low | High | stack | `regprobe.py` writes BAR0 at a hard-coded PCI address | 7.6 |
| LINUX-018 | Low | High | upstream | The licence terms are ambiguous | none, by operator decision |
| LINUX-019 | Low | High | both | No CI, and only 7.2 kernels are built | 7.4, 7.5 |
| LINUX-020 | Low | High | upstream | Dead code paths | 6.18 |
| LINUX-021 | Low | High | both | No `.gitignore`, and ignored Makefile flags | 7.7 |
| LINUX-022 | Medium | High | upstream | The interrupt handler acknowledges the wrong bits on a second pass | 6.14 |

### LINUX-001: zero-copy runs on cards where its safety bound is never programmed

- **Location:** the `zero_copy` default (line 3541), `hws_zc_wanted()` (3560–3563), the gate in `hws_zc_pick()` (3685–3687), the early return in `hws_set_dma_max()` (5465), and `HWS_ZC_SLACK` (`src/hws.h`, line 300).
- **Evidence:** `hws_set_dma_max()` writes register 9 only when `m_DeviceHW_Version` is set, and `SetHardWareInfo()` sets it only for FPGA versions above 121. `hws_zc_wanted()` checks only the `zero_copy` parameter and IOMMU translation. The parameter defaults to true since `7c64b98`. The comment on `HWS_ZC_SLACK` says the 8 KiB slack is enough because register 9 caps an over-long run at the largest quarter plus 2 KiB. `src/hws_reg.h` gives register 9's reset value as 0x44000, which is 4.25 MiB.
- **Trigger:** a card with FPGA 121 or older, an IOMMU in `DMA` or `DMA-FQ` mode, an MMAP consumer at the input size, and a source resolution change while zero-copy is active.
- **Impact:** an over-long run can reach 4.25 MiB past a quarter's start, which is past the end of the app's buffer. For a 1080p buffer the overrun is up to 3,411,968 bytes (computed from `SetVideoFormteSize()`). On the development machine the card shares IOMMU group 22, and so one IOVA space, with the SATA, USB, Wi-Fi and Ethernet controllers. A write that lands on one of their mappings corrupts their data without a fault.

### LINUX-002: `usleep_range()` is reachable from the atomic ALSA trigger

- **Location:** `hws_pcie_audio_trigger()` (4273–4289), `StartAudioCapture()` (4867, 4873), `CheckCardStatus()` (4830–4841) and `InitVideoSys()` (5957).
- **Evidence:** the PCM is not `nonatomic`, so ALSA calls the trigger with the stream spinlock held and interrupts off. Trigger START calls `StartAudioCapture()`, which calls `CheckCardStatus()`, which calls `InitVideoSys()` when register 0 bit 0 is clear. Commit `46f7ec9` added `usleep_range(500, 600)` to `InitVideoSys()`.
- **Trigger:** an app starts the ALSA stream while the card has lost its run state, the case that `CheckCardStatus()` exists for.
- **Impact:** "BUG: scheduling while atomic" instead of a re-initialised card.

### LINUX-003: unbind with open handles

- **Location:** `hws_remove()` (4792–4828), `vdev->release` (4075) and `hws_unregister_video()` (4775–4784).
- **Evidence:** `vdev->release` is `video_device_release_empty`, and `hws_remove()` ends with `kfree(dev)`. The freed `struct hws_pcie_dev` embeds the `video_device`, the `v4l2_device`, the control handler and every lock. Before it unregisters the video node or frees the ALSA card, `hws_remove()` stops the status thread and the engines, frees the IRQ, destroys `auwq` and sets it to NULL. Commit `5bd61fb` reordered this function but kept that order.
- **Trigger:** a driver unbind or PCI remove through sysfs, AER or DPC recovery, or hot removal, while OBS, PipeWire or WirePlumber holds the video or audio device.
- **Impact:** an open handle's `hws_release()` uses freed memory. Until the node is unregistered, STREAMON re-enables DMA with no interrupt handler, into DMA buffers that `DmaMemFreePool()` frees next. A trigger START in that window queues work on the NULL `auwq`.

### LINUX-004: the audio work writes a PCM buffer that is gone

- **Location:** `audio_data_process()` (1810–1890), `_deliver_samples()` (1714–1750), `hws_pcie_audio_hw_free()` (4239–4246) and `hws_resume()` (6402–6405).
- **Evidence:** `hw_free` cancels the silence work but not the audio work. `audio_data_process()` never checks `drv->running`, and it writes to `substream->runtime->dma_area`. `drv->substream` is set at open and never cleared. `hws_resume()` restarts audio capture in hardware for a stream that was running. ALSA meanwhile keeps that stream SUSPENDED, because the trigger rejects SUSPEND and RESUME (LINUX-011).
- **Trigger:**
  - **Path 1 (upstream):** a packet queued just before trigger STOP runs after `hw_free` has set `dma_area` to NULL. This is a narrow race at every stop.
  - **Path 2 (`483648e`):** suspend during audio capture, then an app closes the suspended stream without `prepare`. ALSA's drop calls trigger STOP only for a running stream, so capture keeps running after the runtime is freed.
- **Impact:** a NULL dereference in the audio work while it holds `audiolock` with interrupts off. The next audio tasklet then spins on that lock forever.

### LINUX-005: Relaxed Ordering is forced on

- **Location:** `enable_pcie_relaxed_ordering()` (5903–5907), called at line 6261.
- **Evidence:** the function sets `PCI_EXP_DEVCTL_RELAX_EN` unconditionally. The PCI core clears that bit for devices below root ports flagged `PCI_DEV_FLAGS_NO_RELAXED_ORDERING`, which the 7.2 headers define.
- **Trigger:** a platform with such a root port, for example the Intel Xeon E5 v3 and v4 ports the kernel quirks. Whether the FPGA sets the relaxed-ordering attribute is unknown.
- **Impact:** possible DMA ordering errors, seen as corrupt frames or audio.

### LINUX-006: uninstall leaves the module installed

- **Location:** `scripts/hws-uninstall.sh` (lines 8, 46 and 62–64), `scripts/hws-dkms-install.sh` (79) and `tools/dkms-install.sh` (47–51).
- **Evidence:**
  - `MODULE_NAME` is `HwsUHDX1Capture.ko`, and the uninstaller searches for `${MODULE_NAME}.ko`, which never matches.
  - Kernel 7.2 installs external modules as `HwsUHDX1Capture.ko.zst` under `updates/` (`INSTALL_MOD_DIR ?= updates` in `scripts/Makefile.modinst`, and `CONFIG_MODULE_COMPRESS_ZSTD=y`). No script looks for that name.
  - The installer creates `/usr/src/HwsUHDX1Capture.ko-1.0.0.230324`, and the uninstaller removes `/usr/src/HwsUHDX1Capture.ko_1.0.0.230324`.
  - `tools/dkms-install.sh` registers a second package name, `HwsUHDX1CaptureForLinux`, and has no remove mode. Under `set -e` it stops in the middle of its kernel loop at a kernel whose build link points nowhere.
- **Impact:** after `install.sh` and then `uninstall.sh`, the module stays in `/lib/modules` and loads at boot, while the script prints "Uninstall Successfully!". The DKMS path leaves a dangling `/usr/src` link. The two installers together register two DKMS packages that build the same module.

### LINUX-007: installers run unquoted paths as root

- **Location:** `scripts/hws-install.sh` (lines 18, 62 and 98), `scripts/hws-repair.sh` (15, 51 and 88), and the `LOGFILE` and `sudo su -c "$0 -s $*"` lines in each installer.
- **Evidence:** `MODULE_BUILD_DIR` is the working directory plus `/hws_build`, and it reaches `rm -rvf` unquoted, as root. Each installer truncates and appends `./hws_install.log` as root. shellcheck reports SC2086 (unquoted expansion) throughout these scripts.
- **Trigger:** running `install.sh` or `hws-repair.sh` from a path with a space, or running any installer in a directory that another user can write.
- **Impact:** root runs `rm -rvf` on each fragment of the split path. A symlink at `./hws_install.log` redirects a root write to its target.

### LINUX-008: the README is stale

- **Location:** `README.md`, `quick_start.txt` and the installers' closing messages.
- **Evidence:** `README.md` line 62 and `quick_start.txt` line 41 name the OBS device "HVideo1 (HWS)", but the driver calls it "HUHDVideo 1" (`HWS_VIDEO_NAME` in `src/hws_reg.h`). The installers print the vendor contacts sales@avmatrix.com and alex.liu@longtimetech.com. The README says the driver was tested on Fedora 37 with kernel 6.2. It lists no supported kernels, distributions or architectures, no packages per distribution, no Secure Boot steps and no limitations. It doesn't mention `zero_copy`, debugfs or `tools/`.
- **Impact:** an install on a Secure Boot system fails with no guidance, and users look for a device name that doesn't exist.

### LINUX-009: the installer reports success while the old module stays loaded

- **Location:** `scripts/hws-dkms-install.sh` (line 200).
- **Evidence:** the script greps `lsmod` for `HwsCapture`, which never matches `HwsUHDX1Capture`. It then runs `modprobe`, which does nothing while the old module is loaded, and prints success without the reboot prompt.

### LINUX-010: `Check_Busy()` can loop forever

- **Location:** `Check_Busy()` (4444–4471), which `StopDsp()` calls from `StopDevice()` on remove and on probe failure.
- **Evidence:** the loop runs while register 0 bit 3 is set. `TimeOut` is incremented and never read, and a W=1 build warns that it is set but unused. Each pass prints a line.
- **Impact:** if the bit sticks, remove hangs in D state and prints about 100 lines per second.

### LINUX-011: the PCM advertises RESUME but rejects SUSPEND and RESUME

- **Location:** `audio_pcm_hardware.info` (4179) and the trigger's default case (4300–4302).
- **Evidence:** the PCM advertises `SNDRV_PCM_INFO_RESUME`, and the trigger returns -EINVAL for SUSPEND and RESUME. `drv->running` stays true across a suspend, so the silence work keeps running. The flag comes from upstream, and `running` from `1f3e39a`.
- **Impact:** `snd_pcm_resume()` fails with -EINVAL instead of -ENOSYS. This is part of LINUX-004 path 2.

### LINUX-012: DMA sync calls on coherent memory

- **Location:** `hws_video_half()` (3858–3859) and `MemCopyAudioToSteam()` (5069).
- **Evidence:** both call `dma_sync_single_for_cpu()` on memory from `dma_alloc_coherent()`. The DMA API defines sync calls for streaming mappings only.
- **Impact:** the calls do nothing on x86. A `CONFIG_DMA_API_DEBUG` kernel reports them, and their effect on non-coherent arm64 systems is undefined.

### LINUX-013: the debug canary scan runs in every build

- **Location:** `hws_fill_canary()` and `hws_check_canary()` (4897–4957), and the status thread (5748–5752).
- **Evidence:** every fifth poll, which is every 250 ms, the status thread scans the gap between each DMA buffer's quarter and its audio ring. It holds `cap_lock` during the scan, and the IRQ thread needs that lock. Computed from `SetVideoFormteSize()`, a scan reads 14,684,160 bytes at 1080p and 2,242,560 bytes at 4K. The code comment says about 2 MB. The 2 KiB block that an over-long run writes at register 9 × 16 lands inside this gap. [results.md](results.md) (zero-copy round 2) records it logged as a stray write at console mode changes.
- **Impact:** every console resolution change logs "stray DMA write" for expected behaviour. The scan may also hold the IRQ thread past the zero-copy margin at high frame rates. That delay is not measured.

### LINUX-014: the zero-copy consumer can get one frame twice

- **Location:** `hws_hold_buffers()` (3470) and `hws_video_half()` (3889–3890 and 3952–3953).
- **Evidence:** `hws_hold_buffers()` skips a consumer that already holds a buffer. If a bottom half is lost in the frame where zero-copy starts, the zero-copy consumer holds a copy buffer and also owns `zc_cur`. The next frame then completes both buffers with the same sequence number.
- **Impact:** one repeated frame.

### LINUX-015: leak and ordering bugs in probe error paths

- **Location:** `hws_audio_register()` (4349–4416) and the error labels in `hws_probe()` (6331–6334).
- **Evidence:** `dev->audio[i].card` is set only after `snd_card_register()` succeeds. When `snd_pcm_new()` or the registration fails, the card from `snd_card_new()` leaks. At `err_video`, `hws_unregister_video()` frees the control handler before `StopKSThread()` stops the thread, and the thread calls `v4l2_ctrl_s_ctrl(ctrl_power)` for any node with an open handle.

### LINUX-016: the status thread keeps polling across suspend

- **Location:** `MainKsThreadHandle()` (5714–5767), `hws_hdcp_active()` (3428–3431) and `hws_suspend()` (6364–6385).
- **Evidence:** the status thread isn't freezable, and `hws_suspend()` doesn't park it. `m_PciDeviceLost` is set only in `StopDevice()`, so a read of 0xFFFFFFFF from register 8 counts as HDCP active. The Windows driver treats 0xFFFFFFFF as a removed card ([REGISTERS.md](REGISTERS.md#windows-driver)).
- **Impact:** the thread reads registers of a card in D3. A false "HDCP active" log line is possible. That line is inferred from the code, not observed.

### LINUX-017: `regprobe.py` writes BAR0 at a hard-coded PCI address

- **Location:** the `--pci` default in `tests/regprobe.py`, and `tests/zctest.sh` (line 8).
- **Evidence:** both default to `0000:0b:00.0`. `regprobe.py` maps that device's `resource0` and writes to it without checking the vendor and device IDs.
- **Impact:** on another machine, root writes into an unrelated device's BAR0.

### LINUX-018: the licence terms are ambiguous

- **Evidence:** `LICENSE` holds the GPL version 3 text. `MODULE_LICENSE("GPL")` declares GPL version 2 or later to the kernel. No source file has an SPDX tag or a copyright line.
- **Impact:** the redistribution terms are unclear, and mainline (4.6) needs GPL-2.0-compatible terms.
- **Decision (2026-10-19):** the operator keeps the licence as it is. No change is planned.

### LINUX-019: no CI, and only 7.2 kernels are built

- **Evidence:** the repository has no CI configuration, tags or changelog. Only `tests/quarter_sizes.py` runs without the card. The development machine has headers only for 7.2.6, 7.2.9 and 7.2.10, while the `LINUX_VERSION_CODE` guards cover 3.5 to 7.0.
- **Impact:** the "5.x and later" rule in [SPEC.md](SPEC.md#ground-rules) is unverified.

### LINUX-020: dead code paths

- **Evidence:**
  - The per-device vb2 queue `vq` and its ops (`hwspcie_video_qops`, lines 1330–1505, with `mdelay(100)` in its start) are initialised but unreachable. `vdev->queue` is NULL, and every buffer ioctl goes to a per-open queue.
  - These fields and definitions are unused: `current_out_size_index` (written, never read), `current_out_framerate`, `Interlaced`, `video_lock`, `queue_lock`, `std`, `startstreamIndex`, `seqnr`, `uio_info` (only `mem[0]` is used), the `DVRS_HW_*` ioctl numbers, `m_pAudioData*`, `m_VideoInHWmode`, `cs_data_fs` and `framegrabber_support_refreshrate`.
  - The `#if 0` blocks and some includes (`sound/opl3.h`, `linux/fb.h` and `linux/aio.h`) do nothing.

### LINUX-021: no `.gitignore`, and ignored Makefile flags

- **Evidence:** the repository has no `.gitignore`. `hws_install.log`, which an installer writes as root, and `tests/__pycache__` sit untracked in the tree. The legacy installer copies `src/` with any build output into `/usr/local/share/HWS`. `src/Makefile` sets `EXTRA_CFLAGS` and `EXTRA_LDFLAGS`, which the 7.2.10 kbuild Makefiles don't reference. The build works anyway, because `hws.h` is included by relative path. Nothing references `src/load.sh`.

### LINUX-022: the interrupt handler acknowledges the wrong bits on a second pass

Found while fixing R-03 (spec 6.14), after the audit.

- **Location:** `irqhandler()`, the acknowledge write to register 1 (line 5392 at `66d3773`).
- **Evidence:** `Int_Value` gains 0x01 for a video half and 0x100 for an audio packet on every pass of the handler's loop, and is never reset between passes. On a second pass it holds 0x02 or 0x202, so the write acknowledges bits that aren't set and leaves the real ones set.
- **Trigger:** a new interrupt source sets a bit while the handler is between its acknowledge and its re-read of register 1.
- **Impact:** the loop runs on to its 100-pass limit, acknowledging further wrong bits. Whether the card then raises the interrupt again is unknown.

## Risks that need validation

These are open questions, not confirmed defects.

- **R-01: an input-size change while zero-copy is active (spec 6.8).**
  - `ChangeVideoSize()` (5579–5601) calls `hws_zc_stop()`, which moves only the next runs back to the DMA buffers. The function then raises the quarter lengths and register 9 at once, while the card may still be in a run into the zero-copy buffer.
  - Zero-copy round 2 tested 4K to 1080p and back with OBS at 4K, so the buffer was sized for the larger mode. The other direction is not recorded.
  - Open question: does the card apply a new register 9 or length value to a run already in progress? If it does, a 1080p zero-copy buffer overruns when the source switches to 4K. The computed overruns past the buffer end:

| Quarter | Bytes past the end |
|---|---|
| 0 | 0 |
| 1 | 1,032,192 |
| 2 | 2,068,480 |
| 3 | 3,104,768 |

- **R-02: stopping DMA (spec 6.9).** `StopVideoCapture()` clears an enable bit, and its callers return or free buffers right after it. Those callers are the timeout path in `hws_stop_streaming_multi()`, `hws_zc_stop(pdx, true)`, `hws_suspend()` and `hws_remove()`. Nothing reads a register back, and nothing waits for register 0 bit 3, which mainline names "DMA busy". Unknown: does the card finish a burst after the bit clears?
- **R-03: untested PCI IDs (spec 6.14).** `hws_pci_table` also claims 8888:85A1, 1F33:8581 and 8888:8591, and `SetHardWareInfo()` treats them all as one channel. The driver writes 0x3FFFF to the interrupt enable register but acknowledges only bits 0 and 8 of register 1. [REGISTERS.md](REGISTERS.md) documents bit 12 as line-in audio on the 8591. So that card's line-in interrupt is enabled and never acknowledged.
- **R-04: the zero-copy timing margin (spec 6.17).** A register write counts as in time when it lands within half the shortest recent interval between half-frame interrupts, which is about 1 ms at 240 Hz. This model was measured on one machine. A late write is detected, and its frame completes with an error.
- **R-05: the audio format (spec 6.17).** The driver offers only 48 kHz, 16-bit stereo, as the Windows driver does. Its behaviour with a 44.1 kHz or multichannel source is unrecorded.
- **R-06: torn copies (spec 6.15).** `hws_video_half()` checks the stale-half mark before it copies a half, not after. If the IRQ thread runs late, the card can start overwriting the half during the copy, and the frame still completes as good.
- **R-07: the kernel and distribution range (spec 7.4).** Builds before 7.2, and distribution kernels with backported APIs such as RHEL's, are untested.

## Verification record

Environment: Nobara on Fedora 44, kernel 7.2.10-200, gcc 16.2.1 and DKMS 3.4.3. Headers were installed for 7.2.6, 7.2.9 and 7.2.10. sparse, cppcheck, clang-tidy, smatch and coccinelle were not installed. The card (8888:8581 at `0000:0b:00.0`) was present with the driver loaded, but the audit did not open it.

| Check | Result |
|---|---|
| Kbuild of `git archive 66d3773 src` against each installed kernel, in a scratch directory | All three passed. Each logged 22 missing-prototype warnings, 1 misleading-indentation warning and 1 pahole version note. |
| W=1 build on 7.2.10 | Passed. Added 6 set-but-unused variables, 2 unused constants and 1 format-overflow warning on `card->shortname`, which can't overflow with the names the driver uses. |
| `checkpatch.pl --no-tree -f` on the three source files | 2,448 errors and 427 warnings, nearly all whitespace and brace style. The SPDX tag is missing. |
| shellcheck on every script | `tools/` passed. The legacy installers reported SC2086 and others, and `src/load.sh` has no shebang. |
| `python3 -I -B tests/quarter_sizes.py` | OK. |
| A parse of every Python test | All parse. |
| A scratch script that mirrors `SetVideoFormteSize()` and `hws_zc_buffer_ok()` | The zero-copy bound holds for every even width from 320 to 4096 and every height from 240 to 2160. The smallest slack that works for all of them is 4096 bytes. The script also produced the R-01 figures. |
| Kernel headers and the DKMS script, read | The facts cited in LINUX-005, LINUX-006 and LINUX-021. DKMS passes `KERNELRELEASE` to `make`. |

Not run: `v4l2-compliance`, `tests/run-baseline.sh`, `tests/zctest.sh`, `tests/regprobe.py`, any streaming, suspend or unbind test, and kernel sanitizers. Each of these needs the card, root, or a debug kernel.
