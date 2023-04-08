ifneq ($(KERNELRELEASE),)

HwsUHDX1Capture-objs := hws_video.o

obj-m = HwsUHDX1Capture.o

EXTRA_CFLAGS += -I$(src)
# strip debug symbols
EXTRA_LDFLAGS += -S

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all: 
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install

clean:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) clean

endif

