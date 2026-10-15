# Builds the DKMS .deb for the HWS (AVMATRIX VC12-4K) capture driver.
# The module is also compiled against this Ubuntu release's generic kernel headers, so a broken build fails here.
#   docker build -f packaging/Dockerfile --build-arg UBUNTU_VERSION=24.04 --output out .
ARG UBUNTU_VERSION=24.04

FROM ubuntu:${UBUNTU_VERSION} AS build
ARG UBUNTU_VERSION
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends build-essential linux-headers-generic \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
COPY src/ src/
COPY packaging/debian/ debian/

# Compile check against the release's kernel headers.
RUN KVER=$(ls /lib/modules | head -n1) \
 && cp -r src /tmp/check \
 && make -C /tmp/check KERNELDIR=/lib/modules/$KVER/build \
 && test -f /tmp/check/HwsUHDX1Capture.ko

RUN . src/dkms.conf \
 && DEB_VERSION="${PACKAGE_VERSION}-1~ubuntu${UBUNTU_VERSION}" \
 && PKG=pkg/usr/src/${PACKAGE_NAME}-${PACKAGE_VERSION} \
 && mkdir -p $PKG pkg/DEBIAN /out \
 && cp src/dkms.conf src/Makefile src/*.c src/*.h $PKG/ \
 && sed -e "s/@VERSION@/${DEB_VERSION}/" debian/control > pkg/DEBIAN/control \
 && for s in postinst prerm; do \
      sed -e "s/@NAME@/${PACKAGE_NAME}/g; s/@MODVER@/${PACKAGE_VERSION}/g" debian/$s > pkg/DEBIAN/$s; \
      chmod 755 pkg/DEBIAN/$s; \
    done \
 && dpkg-deb --root-owner-group --build pkg /out/hws-uhdx1-capture-dkms_$(echo ${DEB_VERSION} | tr "~" .)_all.deb \
 && dpkg-deb --info /out/*.deb

FROM scratch
COPY --from=build /out/ /
