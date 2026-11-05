# Changelog

User-visible changes. `docs/SPEC.md` gives the plan behind each item and `docs/results.md` the tests.

## 1.1.0 (2026-10-20)

Changes since the upstream driver (GloriousEggroll's `main`, `bd17e06`). Tested with a VC12-4K (`8888:8581`, FPGA 122) on Nobara 44, kernels 7.2.9 to 7.2.11; see [the release notes](docs/release-notes/v1.1.0.md).

### Install and uninstall

- `install.sh` installs through DKMS only and builds for every installed kernel that has headers. It names the packages to install when something is missing, and with Secure Boot on it prints the key enrolment steps.
- `uninstall.sh` removes every version, including what the old `scripts/hws-*.sh` installers left behind.
- `scripts/` is gone. `dkms-install.sh` runs `install.sh`. The path may contain spaces.
- `modinfo HwsUHDX1Capture` shows the installed version.
- The module builds on kernels 5.14 to 7.2, including RHEL 9 and 10 and their rebuilds.

### Video

- A blocking read no longer freezes every other program using the card (a deadlock in the ioctl lock).
- Each program gets each frame once: no repeated, mixed or torn frames. Timestamps come from the frame-end interrupt, and a dropped frame shows as a gap in the sequence numbers.
- Several programs can capture at once, each at its own frame rate (`S_PARM` divides the input rate).
- The input's own size is listed first and is the default. Other sizes are scaled on the CPU.
- DV timings (`QUERY_DV_TIMINGS` and the rest), input status, and source-change events, so OBS and other programs see the real input mode.
- The measured frame rate tells 59.94 from 60 Hz. High refresh rates work: 1920×1080 at 120 and 240 Hz, 2560×1440 at 144 Hz.
- Brightness, contrast and saturation controls. Hue had no effect on this card and is gone.
- With HDCP on the source, the driver delivers black frames and logs it, as the vendor's Windows driver does.
- Capture works after suspend and resume.
- Changing the input resolution while capturing no longer risks the card writing past a buffer.
- The driver recovers a card that lost its run state, with or without audio running.
- Unplugging (unbinding) or unloading the driver while programs have it open no longer crashes the kernel. The programs get an error and exit.
- New option `zero_copy` (off by default): the card writes frames straight into the program's buffers. See the README.
- The 4096×2160 to 2560×1440 scaler read past the end of the frame and could crash the kernel. It is replaced by the general scaler. This card never sends 4096-wide input to the driver, so the bug was not reachable here.

### Audio

- The sound card reports itself as a BATCH device with periods that match the card's packets. PipeWire no longer mistakes the 21 ms packets for clock drift, which made a low-latency loopback sound robotic and crackly.
- While the source changes mode and sends no audio, the driver delivers silence instead of PipeWire replaying its last buffer as a buzz.
- A recording no longer starts with up to 43 ms of the previous recording's sound.
- Closing or suspending during a recording no longer risks a crash.

### Diagnostics

- `/sys/kernel/debug/hws-<pci address>/status` shows the input, frame and audio counters, zero-copy state and each program's delivery.
- New option `debug_dma` (off by default) for driver development.

### Known issues

- Changing the resolution of the PC's own GPU output while the card captures it has made an AMD Radeon RX 9070 drop off the PCIe bus. See the README.
