# Ubuntu .deb packages (DKMS)

The `HwsUHDX1Capture` driver (AVMATRIX VC12-4K) is packaged as **DKMS**: the `.deb` installs the source code into
`/usr/src/HwsUHDX1CaptureForLinux-<version>` and DKMS builds the module on the machine for every installed kernel
that has headers, rebuilding it automatically on every kernel update.

Supported releases: **Ubuntu 24.04** and **Ubuntu 26.04**.

## Download

Prebuilt packages are on the [Releases](https://github.com/GloriousEggroll/AVMATRIX-VC12-4K-CAPTURE/releases) page.
Every push to `main` also builds the packages as GitHub Actions *artifacts* (workflow `Build .deb`).

## Build locally

Requirements: `make` and Docker with BuildKit (Docker 23+).

```sh
make              # builds the packages for 24.04 and 26.04 into ./out
make deb-24.04    # Ubuntu 24.04 only
make deb-26.04    # Ubuntu 26.04 only
make clean        # removes ./out
```

Variables: `UBUNTU_VERSIONS` (default `24.04 26.04`) and `OUT` (default `out`), e.g.
`make UBUNTU_VERSIONS=26.04 OUT=/tmp/debs`.

Without `make`, the equivalent command is:

```sh
docker build -f packaging/Dockerfile --build-arg UBUNTU_VERSION=24.04 --output out .
```

During the build the module is compiled against the release's *generic* kernel headers; if the driver does not
compile, no package is produced.

## Install

```sh
sudo apt install linux-headers-$(uname -r)
sudo apt install ./out/hws-uhdx1-capture-dkms_*ubuntu24.04_all.deb   # pick the file for your release
```

Check:

```sh
dkms status                 # HwsUHDX1CaptureForLinux/..., <kernel>, x86_64: installed
lsmod | grep HwsUHDX1Capture
```

The package replaces `install.sh`/`dkms-install.sh`; do not use both at the same time.

## Remove

```sh
sudo apt remove hws-uhdx1-capture-dkms
```

Removes the module from every kernel and unregisters it from DKMS.

## Publish a release

Only tag a `main` commit whose `Build .deb` run is green. Pushing a `v*` tag triggers the workflow, which builds
both packages and publishes them with a `SHA256SUMS` file to the tag's release:

```sh
git tag -a v1.0.0.230324-1 -m "Release v1.0.0.230324-1"
git push origin v1.0.0.230324-1
```

## Files

| File | Purpose |
|---|---|
| `packaging/Dockerfile` | Compiles the module (check) and assembles the `.deb` for `UBUNTU_VERSION` |
| `packaging/debian/` | `control`, `postinst` (dkms add/install) and `prerm` (dkms remove) |
| `Makefile` | Shortcuts `make deb`, `make deb-<version>`, `make clean` |
| `.github/workflows/build-deb.yml` | CI: builds the packages and publishes them to `v*` tag releases |
