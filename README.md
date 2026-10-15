# AVMATRIX VC12-4K Linux driver

[![Release](https://img.shields.io/github/v/release/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE?color=blue)](https://github.com/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE/releases/latest)
[![Build .deb](https://github.com/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE/actions/workflows/build-deb.yml/badge.svg)](https://github.com/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE/actions/workflows/build-deb.yml)
[![License: GPL](https://img.shields.io/badge/license-GPL-green)](LICENSE)

Linux kernel driver (`HwsUHDX1Capture`) for the **AVMATRIX VC12-4K** PCIe HDMI 2.0 capture card, with
ready-to-install **DKMS `.deb` packages for Ubuntu 24.04 and 26.04**. The card shows up as a standard
V4L2 video device and an ALSA audio device, so it works with OBS Studio, ffmpeg, VLC and any other V4L2 application.

![Architecture: HDMI source → VC12-4K → kernel module → V4L2/ALSA → applications, and the DKMS .deb build/install path](doc/architecture.png)

([SVG version](doc/architecture.svg))

## Contents

- [Hardware](#hardware)
- [Install](#install)
- [Check the installation](#check-the-installation)
- [Usage](#usage)
- [Uninstall](#uninstall)
- [Troubleshooting](#troubleshooting)
- [Building the packages](#building-the-packages)
- [Where to buy](#where-to-buy)
- [History and license](#history-and-license)

## Hardware

| | |
|---|---|
| Card | AVMATRIX VC12-4K (OEM chip by Longtime Electronic / lt-link "HWS UHD") |
| Input | 1× HDMI 2.0, up to 4K60 (4096×2160) |
| Output | 1× HDMI 2.0 loop-out (zero-latency passthrough) |
| Bus | PCIe Gen2 x4 |
| PCI IDs | `8888:8581`, `8888:85a1`, `8888:8591`, `1f33:8581` |
| Video | V4L2 `/dev/videoN`, named `HUHDVideo N`, YUYV (also UYVY, RGB24, …) |
| Audio | ALSA card named `HUHDAudio N` |

Check that your card is detected:

```sh
lspci -nn | grep -iE '8888:85|1f33:8581'
```

> **VC41 / VC42 are different cards** (4-channel SDI / HDMI) and need a different driver:
> [GloriousEggroll/AVMATRIX-VC41-VC42-CAPTURE](https://github.com/GloriousEggroll/AVMATRIX-VC41-VC42-CAPTURE).

## Install

### Option 1: `.deb` package (Ubuntu 24.04 / 26.04, recommended)

Download the package for your release from the [latest release](https://github.com/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE/releases/latest)
(or build it yourself with `make deb`, see [Building the packages](#building-the-packages)) and install it:

```sh
sudo apt install linux-headers-$(uname -r)
sudo apt install ./hws-uhdx1-capture-dkms_*ubuntu24.04_all.deb   # or *ubuntu26.04*
```

With the GitHub CLI:

```sh
gh release download -R GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE -p '*ubuntu24.04*'
```

DKMS builds the module for every installed kernel that has headers and rebuilds it automatically on kernel updates.
Verify the download with the `SHA256SUMS` file attached to the release (`sha256sum -c SHA256SUMS --ignore-missing`).

### Option 2: DKMS install script (any distribution)

Requires `make`, `gcc`, `dkms` and the headers for your running kernel
(`linux-headers-$(uname -r)` on Debian/Ubuntu, `kernel-devel` on Fedora).

```sh
git clone https://github.com/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE
cd AVMATRIX-VC12-4K-CAPTURE
./dkms-install.sh
```

The script copies the driver to `/usr/local/share/HWS`, registers it with DKMS and loads it.
`./install.sh` does the same without DKMS (the module must then be rebuilt after every kernel update).

> Use either the `.deb` or the scripts, not both: they register the same DKMS module.

Reboot if the installer says a previous version was still loaded.

## Check the installation

```sh
dkms status                       # HwsUHDX1CaptureForLinux/1.0.0.230324, <kernel>, x86_64: installed
lsmod | grep HwsUHDX1Capture      # module loaded
v4l2-ctl --list-devices           # HUHDVideo 1 ... /dev/videoN   (package: v4l-utils)
arecord -l                        # card N: HUHDAudio ...          (package: alsa-utils)
```

## Usage

### OBS Studio

1. **Sources → + → Video Capture Device (V4L2)** → Device: `HUHDVideo 1`.
2. **Sources → + → Audio Input Capture (ALSA)** → Device: `HUHDAudio 1`.
3. Set the resolution and frame rate to match the HDMI source.

### ffmpeg

Find the video device and the ALSA card number first (`v4l2-ctl --list-devices`, `arecord -l`), then:

```sh
# preview
ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 -framerate 60 /dev/video0

# record video + audio (replace /dev/video0 and hw:2,0)
ffmpeg -f v4l2 -input_format yuyv422 -video_size 3840x2160 -framerate 30 -i /dev/video0 \
       -f alsa -i hw:2,0 \
       -c:v libx264 -preset veryfast -crf 20 -c:a aac capture.mkv
```

List the formats and resolutions offered by the card:

```sh
v4l2-ctl -d /dev/video0 --list-formats-ext
```

## Uninstall

| Installed with | Remove with |
|---|---|
| `.deb` | `sudo apt remove hws-uhdx1-capture-dkms` |
| scripts | `sudo ./scripts/hws-uninstall.sh` (or `sudo hws-uninstall.sh`) |

## Troubleshooting

| Problem | Fix |
|---|---|
| `Your kernel headers for kernel X cannot be found` | `sudo apt install linux-headers-$(uname -r)`, then `sudo dpkg-reconfigure hws-uhdx1-capture-dkms` |
| `modprobe: ERROR: ... Key was rejected by service` | Secure Boot is on and the module is not signed with an enrolled key. Enroll the DKMS key (`sudo mokutil --import /var/lib/dkms/mok.pub`, reboot, follow the MOK prompt) or disable Secure Boot. |
| No `/dev/video*` device | Check `lspci -nn` for the card and `sudo dmesg | grep -i hws` for errors; make sure the card sits in a PCIe x4 (or wider) slot. |
| Black image in OBS | The source must output a supported mode; check HDCP is off on the source (consoles: disable HDCP in settings). |
| Module missing after a kernel update (scripts install) | Run `sudo hws-repair.sh` or reinstall; the `.deb` handles this automatically. |

## Building the packages

```sh
make             # builds both .deb packages into ./out with Docker
make deb-24.04   # one release only
```

See [doc/README.md](doc/README.md) for details on the Docker build, CI and publishing releases.

## Where to buy

Prices checked in October 2026; they change often and imports may add taxes.

| Store | Listing | Price |
|---|---|---|
| AliExpress | [007 digital Store (4.9★, 2000+ sold)](https://pt.aliexpress.com/item/1005008747295104.html) | R$ 1,294.75 |
| AliExpress | [VC12-4K PCIe capture card](https://pt.aliexpress.com/item/1005010070726407.html) | R$ 1,523.99 |
| AliExpress | [VC12-4K 4K60 capture system](https://pt.aliexpress.com/item/1005012929048866.html) | R$ 1,540.99 |
| AliExpress | [VC12-4K HDMI 2.0 PCIe2.0 x4](https://pt.aliexpress.com/item/1005012790270965.html) | R$ 2,246.60 |
| AliExpress | [All VC12-4K listings](https://pt.aliexpress.com/w/wholesale-avmatrix-vc12%252d4k.html) | |
| Amazon US | [B0B98W63VW](https://www.amazon.com/dp/B0B98W63VW) | US$ 279.00 |
| Amazon US | [B0B9BNTKRK](https://www.amazon.com/dp/B0B9BNTKRK) | |
| Amazon BR | [B0GWR78PQK](https://www.amazon.com.br/dp/B0GWR78PQK) | R$ 2,370.53 |
| Amazon DE | [B0D3D6LSHP](https://www.amazon.de/dp/B0D3D6LSHP) | € 299.00 |
| eBay | [284928062354](https://www.ebay.com/itm/284928062354) | US$ 279.00 + shipping |
| Official | [avmatrix.com](https://www.avmatrix.com/products/vc12-4k-4k-hdmi-pcie-capture-card/) | |

> Some AliExpress listings bundle **VC41 / VC42 / VC12-4K** as variants of one listing: pick the `VC12-4K` option.
> VC41/VC42 are not supported by this driver.

## History and license

The original AVMATRIX driver did not build on recent kernels and was distributed from OneDrive.
[GloriousEggroll](https://github.com/GloriousEggroll) noticed that its installer scripts were adapted from the
Magewell Pro Capture driver, ported over the DKMS and module-signing support, fixed the code for newer kernels and
published the full source. Upstream keeps importing the vendor's changes from
[lt-link/HWS-HDMI-1CHUHD-CaptureCard](https://github.com/lt-link/HWS-HDMI-1CHUHD-CaptureCard) and fixes for new kernels
(6.13, 6.18, 7.0), and ships Ubuntu `.deb` packaging with CI.

Not affiliated with AVMATRIX. The driver is licensed under the GPL (`MODULE_LICENSE("GPL")`); see [LICENSE](LICENSE).

More: [AVMATRIX downloads](https://www.avmatrix.com/download/) · [vendor user guide](doc/UserGuideforLinux.eng.html) ·
[video review](https://www.youtube.com/watch?v=RpLASLDXxiM)
