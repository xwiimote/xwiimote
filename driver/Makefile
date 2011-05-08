#
# XWiimote - driver - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

ifneq ($(KERNELRELEASE),)

obj-m := src/hid-wiimote.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
.PHONY: default clean

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	@rm -rvf .tmp_versions/ src/.*.cmd src/*.o src/*.ko src/*.mod.? modules.order Module.symvers

endif
