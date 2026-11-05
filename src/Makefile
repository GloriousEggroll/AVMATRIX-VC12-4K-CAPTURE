HwsUHDX1Capture-objs := hws_video.o hws_scale.o

obj-m = HwsUHDX1Capture.o

ccflags-y += -I$(src)
# modinfo shows the DKMS package version
HWS_VERSION := $(shell sed -n 's/^PACKAGE_VERSION=//p' $(src)/dkms.conf 2>/dev/null)
ccflags-y += -DHWS_VERSION='"$(or $(HWS_VERSION),unknown)"'

# Kernel API checks. Red Hat backports newer V4L2 APIs into kernels with
# older version numbers (RHEL 9.10 and 10.2 have the 6.18 v4l2_fh_add();
# 9.10 and 10.4 drop vb2_ops.wait_prepare, which mainline dropped in 7.0),
# so read the headers being built against instead of LINUX_VERSION_CODE.
ifneq ($(KERNELRELEASE),)
ifneq ($(shell grep -s 'void v4l2_fh_add(struct v4l2_fh \*fh, struct file \*filp)' $(srctree)/include/media/v4l2-fh.h),)
ccflags-y += -DHWS_FH_TAKES_FILE
endif
ifeq ($(shell grep -s '(\*wait_prepare)' $(srctree)/include/media/videobuf2-core.h),)
ccflags-y += -DHWS_VB2_NO_WAIT_PREPARE
endif
endif

ifneq ($(KERNELRELEASE),)
KERNELDIR ?= /lib/modules/$(KERNELRELEASE)/build
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
endif

PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

