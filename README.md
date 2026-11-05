# AVMatrix VC12-4K capture card driver for Linux

This driver runs the AVMatrix VC12-4K, a PCIe card that captures one HDMI input at up to 3840×2160 at 60 Hz or 1920×1080 at 240 Hz, with its stereo audio. It gives you two devices:

- a video device named **HUHDVideo 1** (for example `/dev/video0`), which OBS, `v4l2-ctl` and other V4L2 programs can open;
- a sound card named **HUHDAudio 1**, which ALSA, PipeWire and PulseAudio programs can record from.

The driver is not part of the Linux kernel. `install.sh` builds it with DKMS, which rebuilds it each time you install a new kernel.

## Supported hardware and systems

- **Card:** PCI ID `8888:8581` (VC12-4K), tested with FPGA version 122. The driver also binds to other AVMatrix IDs; those are untested and the kernel log says so.
- **Architecture:** x86-64 only.
- **Tested with the card:** Nobara 44 (Fedora-based), kernels 7.2.9 to 7.2.11, AMD Ryzen 9 9950X3D.
- **Install tested in VMs:** Fedora 44, Ubuntu 24.04 and Arch Linux, with Secure Boot on for Fedora and Ubuntu.
- **Builds against:** Fedora 44 and Arch (7.2.11), Ubuntu 24.04 (6.8), Debian 12 (6.1), Ubuntu 22.04 (5.15), CentOS Stream and AlmaLinux 9 (5.14) and 10 (6.12). The oldest kernel that builds is 5.14. The CI workflow in `.github/workflows/build.yml` checks these on every push.

## Install

### 1. Install the build tools and kernel headers

The headers must match the kernel you are running (`uname -r`).

| Distribution | Command |
|---|---|
| Fedora | `sudo dnf install dkms gcc make kernel-devel-$(uname -r)` |
| Ubuntu, Debian | `sudo apt update && sudo apt install dkms gcc make linux-headers-$(uname -r)` |
| Arch | `sudo pacman -Syu dkms gcc make linux-headers`, then reboot if the kernel was updated |

Desktop installs have the kernel's video and sound modules. Minimal kernels (cloud and some server installs) leave them out, and the driver then fails to load. Add them like this:

| Distribution | Command |
|---|---|
| Fedora | `sudo dnf install kernel-modules-$(uname -r)` |
| Ubuntu | `sudo apt install linux-modules-extra-$(uname -r)` |

### 2. Run the installer

```sh
git clone --branch v1.1.0 https://github.com/bhoglo/AVMATRIX-VC12-4K-CAPTURE.git
cd AVMATRIX-VC12-4K-CAPTURE
./install.sh
```

`install.sh` asks for your password through `sudo`. It copies the sources to `/usr/src/HwsUHDX1CaptureForLinux-<version>`, builds the module for every installed kernel that has headers, and loads it. It ends with one of these lines:

- `Loaded.` The driver is running.
- `The driver is already loaded. Reboot to use the version just installed (<version>).`
- `This kernel lacks modules the driver needs`, followed by the package to install (see step 1).
- A Secure Boot message (see the next step).

The path may contain spaces. The installer writes nothing into the folder you run it from.

### 3. Secure Boot: enrol the signing key once

With Secure Boot on, the kernel loads only modules signed with a key it trusts. DKMS signs the module with its own key, and you enrol that key once. `install.sh` checks this and prints the steps. They are:

1. Import the key and choose a one-time password:
   - Fedora and Arch: `sudo mokutil --import /var/lib/dkms/mok.pub`
   - Ubuntu: `sudo mokutil --import /var/lib/shim-signed/mok/MOK.der`

   On Ubuntu, DKMS may ask for this password itself during the install. Then the key is already queued and `install.sh` says so. Skip to step 2.
2. Reboot. A blue screen says **Press any key to perform MOK management**. Press a key within 10 seconds. If the countdown runs out, the request is dropped. Run the import again and reboot.
3. Choose **Enroll MOK**, then **Continue**, then **Yes**, type the password, and choose **Reboot**.

To check: `mokutil --test-key /var/lib/dkms/mok.pub` (or the Ubuntu path) prints `is already enrolled`. Later installs and kernel updates reuse the same key.

### 4. Check that it works

With the card fitted, the driver loads at boot. The checks below use `v4l2-ctl` and `arecord`. Install them if you don't have them:

| Distribution | Command |
|---|---|
| Fedora | `sudo dnf install v4l-utils alsa-utils` |
| Ubuntu, Debian | `sudo apt install v4l-utils alsa-utils` |
| Arch | `sudo pacman -S v4l-utils alsa-utils` |

Then run:

```sh
v4l2-ctl --list-devices        # shows "HUHDVideo 1" and its /dev/video node
arecord -l                     # shows a card "HUHDAudio 1"
sudo dmesg | grep HwsUHDX1Capture
modinfo -F version HwsUHDX1Capture
```

The kernel log shows a line like `DMA buffers: 4 x 4608 KiB, input up to 4096x2160` when the driver finds the card.

## Use it in OBS

1. Add a **Video Capture Device (V4L2)** source and choose **HUHDVideo 1**.
2. Set **Resolution** to the input's size (for example 3840x2160) and **Video Format** to **YUYV 4:2:2**. Another size works too. The driver then scales on the CPU.
3. Set **Color Range** to **Full**. The card delivers full-range video whatever the source sends. With **Partial**, blacks look grey.
4. For sound, add an **Audio Input Capture** source and choose **HUHDAudio 1**. To hear it, set the source's monitoring in **Advanced Audio Properties**.

Any V4L2 program works the same way. For example, to view the input with GStreamer:

```sh
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,format=YUY2,width=3840,height=2160 ! videoconvert ! autovideosink
```

Several programs can capture from the card at the same time. They share one frame size: while one program is capturing, the others get its size. Each can choose its own frame rate.

## Upgrade

```sh
cd AVMATRIX-VC12-4K-CAPTURE
git pull
./uninstall.sh
./install.sh
```

Reboot if the installer says the driver is already loaded.

## Uninstall

```sh
./uninstall.sh
```

`uninstall.sh` removes every version this installer, `tools/dkms-install.sh` or the old `scripts/hws-*.sh` installers put on the system: the DKMS entries, `/usr/src/HwsUHDX1Capture*`, `/usr/local/share/HWS`, the `/usr/bin/hws-*.sh` links and any `HwsUHDX1Capture.ko*` under `/lib/modules`. It keeps `/etc/modprobe.d/hws.conf` if you made one. If the driver is loaded, it stays loaded until you reboot.

## Options

Set options in `/etc/modprobe.d/hws.conf`, for example:

```
options HwsUHDX1Capture zero_copy=1
```

They take effect the next time the driver loads, so reboot after a change.

| Option | Default | What it does |
|---|---|---|
| `zero_copy` | off | The card writes each frame straight into the capturing program's buffers instead of the driver copying it there. At 3840×2160 this saves about 500 ms of CPU time per 600 frames. |
| `debug_dma` | off | Turns on DMA checks and debug files for driver development. Leave it off. |

`zero_copy` is used only when all of these hold. Otherwise the driver copies frames as usual and says why in the kernel log (`zero_copy is on but unavailable: ...`):

- the IOMMU translates addresses (the default on most systems; not with `iommu=pt` or with the IOMMU off);
- the card is a VC12-4K (`8888:8581`) with FPGA 122 or later;
- the kernel has `videobuf2-dma-contig` (RHEL 9 and its rebuilds don't);
- the program captures at the input's size with memory-mapped buffers, as OBS does.

`zero_copy` was tested for 8 hours at 3840×2160 at 60 Hz without errors. It is new, which is why it is off by default.

## Troubleshooting

| Symptom | Cause and fix |
|---|---|
| No HUHDVideo device | Run `lspci -d 8888:`. If the card isn't listed, reseat it. If it is, run `sudo dmesg \| grep -i hws` and see the next rows. |
| `modprobe: ERROR: ... Key was rejected by service` | Secure Boot is on and the DKMS key isn't enrolled. Follow [step 3](#3-secure-boot-enrol-the-signing-key-once). |
| `Unknown symbol in module` | The kernel's video and sound modules are missing. Install the package from [step 1](#1-install-the-build-tools-and-kernel-headers). |
| `build failed` during install | The headers don't match the running kernel. Install the headers for `uname -r`, or reboot into the newest kernel. The full log is in `/var/lib/dkms/HwsUHDX1CaptureForLinux/<version>/build/make.log`. |
| A solid blue picture | The card sees no signal. Check the cable and that the source is on and set to a supported mode (see [Limitations](#limitations)). |
| A black picture, and `HDCP active, delivering black frames` in the kernel log | The source protects its output with HDCP. The driver never passes protected video. Turn HDCP off on the source (for example a PS5's **HDCP** setting). |
| A short silence when the source changes resolution | Expected. The source stops sending audio while it changes mode, and the driver fills the gap with silence. |
| Colours look washed out in OBS | Set the source's **Color Range** to **Full**. |

## Limitations

- **Video format:** YUYV 4:2:2 only.
- **Input:** one HDMI input, up to 3840×2160 at 60 Hz and 1920×1080 at 240 Hz. The card itself scales a 4096×2160 source to 3840×2160. Interlaced input arrives line-doubled as progressive frames.
- **Output sizes:** the input's size, or one of 17 standard sizes from 720×480 to 4096×2160, scaled on the CPU.
- **Audio:** 48 kHz, stereo, 16-bit. Other sample rates from the source don't come through: the card advertises only 48 kHz to the source.
- **No HDR or 10-bit video.**
- **HDCP:** protected video is delivered as black frames.

## Known issues

- **Capturing your own PC's GPU output.** When the card captures the same PC's graphics output, changing that output's resolution (or changing it shortly after the driver loads) has three times made an AMD Radeon RX 9070 drop off the PCIe bus and lock up the desktop. With the driver unloaded, or loaded but idle, 20 changes each ran cleanly; nothing found ties it to a driver defect. Consoles and other PCs as sources weren't affected (over 120 resolution changes from a Nintendo Switch 2 while capturing). If you capture your own GPU, set its resolution before you start capturing and leave it.

## What's in this repository

- `install.sh`, `uninstall.sh`: install and remove the driver. `dkms-install.sh` runs `install.sh`.
- `CHANGELOG.md`: what changed in each release. `SECURITY.md`: how to report a security problem.
- `src/`: the driver source.
- `tests/`, `tools/`: test scripts and developer tools. `docs/` holds the developer documentation: the plan, the audit, the register notes and the test results.
- `doc/`, `quick_start.txt` and `play/` are AVMatrix's original material. Their install steps are out of date; follow this README instead.

## Background and licence

The original README of this repository, by GloriousEggroll, who ported the vendor driver to newer kernels and added DKMS support:

> Preface:
>
> I am in no way affiliated with AVMATRIX.
>
> I bought this card on Amazon for around $250 because it has the same capabilities as the Magewell Pro Capture HDMI 4k LT, and I wanted to see if it held up in comparison. The Magewell card is a $900 card, as opposed to this one.
>
> Once I got the card, I found the standard ./install -did not work- on Fedora, and the code was slightly outdated for kernel 6.1. On top of that the drivers were being hosted on OneDrive.. which is sketchy.
>
> To add to that, since I also own the Magewell card already, I was able to do a driver comparison, and I found that this company had done a copy/paste of the Magewell driver install bash scripts and just modified them with their own driver sources.  This made porting over the DKMS functionality (and required kernel module signing) much easier!
>
> Therefore I updated their drivers for them, pointed out all the things wrong, and sent them an e-mail about it. In the meantime, here is the full driver source, which is has been updated and working on Fedora 37 as of Kernel 6.2.

The license for this project is GPL, as both the AVMATRIX driver -AND- the Magewell driver use GPL:

AVMATRIX:
```
$ grep -rni . -e license
./src/hws_video.c:5794:MODULE_LICENSE("GPL");
```

Magewell:
```
$ grep -rni . -e license
./src/sources/avstream/capture.c:611:MODULE_LICENSE("GPL");
```
