#
# XWiimote - driver - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

ifneq ($(KERNELRELEASE),)

obj-m := src/hid-nintendo.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
.PHONY: default clean

default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

clean:
	@rm -vf src/*.o src/*.ko src/*.mod.? modules.order Module.symvers

endif
