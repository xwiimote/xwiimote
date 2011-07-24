#
# XWiimote - driver - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

#
# This builds the wiimote driver for the currently running kernel. This
# distribution contains a recent stable source code of the driver
# (hid-wiimote.c), but if you want a more recent one with unstable features,
# copy the ./hid/drivers/hid-wiimote.c file from the xwiimote_kernel repository
# into this directy.
# "make copy" does exactly this, assuming you have checked out "xwiimote_kernel"
# in the same directory as "xwiimote".
# Patch the file with "remove_hidids.patch" before compiling it, otherwise you
# will get some errors.
#
# Run "make" or "make build" to compile the source. This will produce a
# hid-wiimote.ko file which can be loaded with: "insmod ./hid-wiimote.ko"
#
# "make clean" will remove all temporary files.
#

ifneq ($(KERNELRELEASE),)

obj-m := hid-wiimote.o

else

KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
.PHONY: default clean

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

copy:
	@cp -v ../../xwiimote_kernel/drivers/hid/hid-wiimote.c .
	@patch -p0 <remove_hidids.patch

clean:
	@rm -rvf .tmp_versions/ .*.cmd *.o *.ko *.mod.? modules.order Module.symvers *.orig *.rej

endif
