# AVMATRIX VC12-4K on Linux: full guide

Back to the [project README](../README.md).

- [Hardware](#hardware)
- [How it works](#how-it-works)
- [Install](#install)
- [Check the installation](#check-the-installation)
- [Usage](#usage)
- [Uninstall](#uninstall)
- [Troubleshooting](#troubleshooting)
- [Building the packages](#building-the-packages)
- [CI and releases](#ci-and-releases)
- [Repository layout](#repository-layout)
- [Where to buy](#where-to-buy)
- [History](#history)

## Hardware

| | |
|---|---|
| Card | AVMATRIX VC12-4K (OEM chip by Longtime Electronic / lt-link "HWS UHD") |
| Input | 1× HDMI 2.0, up to 4K60 (4096×2160) |
| Output | 1× HDMI 2.0 loop-out (zero-latency passthrough) |
| Bus | PCIe Gen2 x4 |
| PCI IDs | `8888:8581`, `8888:85a1`, `8888:8591`, `1f33:8581` |
| Kernel module | `HwsUHDX1Capture` (DKMS package `HwsUHDX1CaptureForLinux`) |
| Video | V4L2 `/dev/videoN`, named `HUHDVideo 1` |
| Pixel formats | YUYV (default), UYVY, YVYU, VYUY, RGB565, RGB555, RGB24, BGR24, RGB32, BGR32 |
| Audio | ALSA card named `HUHDAudio 1` |

Check that the card is detected:

```sh
lspci -nn | grep -iE '8888:85|1f33:8581'
```

The **VC41 / VC42** (4-channel SDI / HDMI, 1080p60) are different cards with a different module (`HwsCapture`):
use [AVMATRIX-VC41-VC42-CAPTURE](https://github.com/GloriousEggroll/AVMATRIX-VC41-VC42-CAPTURE). Both drivers can be
installed at the same time; each one only binds to its own PCI IDs.

## How it works

![Architecture](architecture.png)

([SVG](architecture.svg))

- **Capture path:** the HDMI signal goes into the card (and out of the loop-out to a monitor); the card DMAs frames
  and audio over PCIe; the `HwsUHDX1Capture` module exposes them as a V4L2 video device and an ALSA audio device.
- **Install path:** the `.deb` (package `hws-uhdx1-capture-dkms`) ships only the source code into
  `/usr/src/HwsUHDX1CaptureForLinux-<version>`. Its `postinst` runs `dkms add` and `dkms install` for **every
  installed kernel that has headers**; DKMS then rebuilds the module automatically whenever a new kernel is installed.
  `prerm` unloads the module and runs `dkms remove --all`.

## Install

### `.deb` package (Ubuntu 24.04 / 26.04)

```sh
sudo apt install linux-headers-$(uname -r)
gh release download -R GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE -p '*ubuntu24.04*' -p SHA256SUMS
sha256sum -c SHA256SUMS --ignore-missing
sudo apt install ./hws-uhdx1-capture-dkms_*ubuntu24.04_all.deb
```

Use the `*ubuntu26.04*` file on 26.04. Without the GitHub CLI, download the files from the
[releases page](https://github.com/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE/releases/latest).
No release yet? Build the packages yourself: see [Building the packages](#building-the-packages).

### DKMS install script (any distribution)

Requires `make`, `gcc`, `dkms` and the headers for the running kernel (`linux-headers-$(uname -r)` on
Debian/Ubuntu, `kernel-devel` on Fedora).

```sh
git clone https://github.com/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE
cd AVMATRIX-VC12-4K-CAPTURE
./dkms-install.sh
```

The script copies the driver to `/usr/local/share/HWS`, registers it with DKMS, loads it and links
`hws-repair.sh` / `hws-uninstall.sh` into `/usr/bin`. `./install.sh` does the same without DKMS, so the module
must be rebuilt after every kernel update (`sudo hws-repair.sh`).

> Use either the `.deb` or the scripts, not both: they register the same DKMS module.

Reboot if the installer reports that a previous version was still loaded.

### Secure Boot

With Secure Boot enabled the module must be signed with a key the firmware trusts. On Ubuntu, DKMS signs it with
`/var/lib/dkms/mok.pub`; enroll that key once:

```sh
sudo mokutil --import /var/lib/dkms/mok.pub   # choose a one-time password
sudo reboot                                    # "Enroll MOK" in the blue MOK manager screen
```

## Check the installation

```sh
dkms status                       # HwsUHDX1CaptureForLinux/1.0.0.230324, <kernel>, x86_64: installed
lsmod | grep HwsUHDX1Capture      # module loaded
v4l2-ctl --list-devices           # HUHDVideo 1 → /dev/videoN        (package: v4l-utils)
arecord -l                        # card N: HUHDAudio 1               (package: alsa-utils)
sudo dmesg | grep -i hws          # driver messages
```

## Usage

### OBS Studio

1. **Sources → + → Video Capture Device (V4L2)** → Device: `HUHDVideo 1`.
2. **Sources → + → Audio Input Capture (ALSA)** → Device: `HUHDAudio 1`.
3. Set the resolution and frame rate to match the HDMI source.

### ffmpeg

Find the video device and ALSA card number (`v4l2-ctl --list-devices`, `arecord -l`), then:

```sh
# preview
ffplay -f v4l2 -input_format yuyv422 -video_size 1920x1080 -framerate 60 /dev/video0

# record video + audio (replace /dev/video0 and hw:2,0)
ffmpeg -f v4l2 -input_format yuyv422 -video_size 3840x2160 -framerate 30 -i /dev/video0 \
       -f alsa -i hw:2,0 \
       -c:v libx264 -preset veryfast -crf 20 -c:a aac capture.mkv
```

Formats and resolutions offered by the card:

```sh
v4l2-ctl -d /dev/video0 --list-formats-ext
```

### Other players

```sh
mpv av://v4l2:/dev/video0
vlc v4l2:///dev/video0
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
| `modprobe: ERROR: ... Key was rejected by service` | Secure Boot is on and the DKMS key is not enrolled: see [Secure Boot](#secure-boot), or disable Secure Boot. |
| No `/dev/video*` device | Check `lspci -nn` for the card and `sudo dmesg \| grep -i hws`; use a PCIe x4 (or wider) slot. |
| Black image | The source must output a supported mode; turn HDCP off on the source (consoles: HDCP setting). |
| Module missing after a kernel update (scripts install) | `sudo hws-repair.sh` or reinstall; the `.deb` rebuilds automatically. |
| `WARNING: HwsUHDX1Capture failed to build for <kernel>` during `apt install` | That kernel's headers are incomplete or the kernel is too new; check `/var/lib/dkms/HwsUHDX1CaptureForLinux/*/build/make.log`. |

## Building the packages

Requirements: `make` and Docker with BuildKit (Docker 23+).

```sh
make              # builds the packages for 24.04 and 26.04 into ./out
make deb-24.04    # Ubuntu 24.04 only
make deb-26.04    # Ubuntu 26.04 only
make clean        # removes ./out
```

Variables: `UBUNTU_VERSIONS` (default `24.04 26.04`) and `OUT` (default `out`), e.g.
`make UBUNTU_VERSIONS=26.04 OUT=/tmp/debs`. Without `make`:

```sh
docker build -f packaging/Dockerfile --build-arg UBUNTU_VERSION=24.04 --output out .
```

What the Dockerfile does, for the chosen `ubuntu:<version>` image:

1. installs `build-essential` and `linux-headers-generic`;
2. compiles the module against those headers (a broken build stops here, no package is produced);
3. assembles `hws-uhdx1-capture-dkms_<version>-1.ubuntu<release>_all.deb` from `src/` and `packaging/debian/`.

The package version is `PACKAGE_VERSION` from `src/dkms.conf` plus `-1~ubuntu<release>`; the file name uses `.`
instead of `~` so it matches the GitHub release asset name.

Test a package in a clean container:

```sh
docker run --rm -v "$PWD/out:/out:ro" ubuntu:24.04 bash -c '
  apt-get update -qq && apt-get install -y -qq linux-headers-generic >/dev/null
  apt-get install -y ./out/*ubuntu24.04*.deb && dkms status'
```

## CI and releases

`.github/workflows/build-deb.yml` runs on every push and pull request: one `build` job per Ubuntu release runs
`make deb-<release>` and uploads the package as an artifact.

On a `v*` tag, a `release` job runs after both builds succeed: it collects the packages, writes `SHA256SUMS` and
publishes everything to the tag's GitHub release.

Only tag a `main` commit whose `Build .deb` run is green:

```sh
gh run list --workflow build-deb.yml --commit "$(git rev-parse HEAD)" -L 1
git tag -a v1.0.0.230324-1 -m "Release v1.0.0.230324-1"
git push origin v1.0.0.230324-1
```

## Repository layout

| Path | Purpose |
|---|---|
| `src/` | Driver source (`hws_video.c`, `hws.h`, `hws_reg.h`), kernel `Makefile`, `dkms.conf` |
| `scripts/` | Vendor install / DKMS install / repair / uninstall scripts |
| `install.sh`, `dkms-install.sh`, `uninstall.sh` | Wrappers for the scripts |
| `packaging/Dockerfile` | Compile check and `.deb` build for `UBUNTU_VERSION` |
| `packaging/debian/` | `control`, `postinst` (dkms add/install), `prerm` (dkms remove) |
| `Makefile` | `make deb`, `make deb-<release>`, `make clean` |
| `.github/workflows/build-deb.yml` | CI builds and releases |
| `doc/` | This guide, architecture diagram, vendor user guides (`UserGuideforLinux.*.html`) |
| `play/` | `xawtv` preview examples |

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

## History

The original AVMATRIX driver did not build on recent kernels and was distributed from OneDrive.
[GloriousEggroll](https://github.com/GloriousEggroll) noticed that its installer scripts were adapted from the
Magewell Pro Capture driver, ported over the DKMS and module-signing support, fixed the code for newer kernels and
published the full source. Upstream keeps importing the vendor's changes from
[lt-link/HWS-HDMI-1CHUHD-CaptureCard](https://github.com/lt-link/HWS-HDMI-1CHUHD-CaptureCard) and fixes for new kernels
(6.13, 6.18, 7.0), and ships Ubuntu `.deb` packaging with CI.

The driver is licensed under the GPL (`MODULE_LICENSE("GPL")`). Not affiliated with AVMATRIX.

More: [AVMATRIX downloads](https://www.avmatrix.com/download/) · [vendor user guide](UserGuideforLinux.eng.html) ·
[video review](https://www.youtube.com/watch?v=RpLASLDXxiM)
