#
# XWiimote - driver - makefile
# Written by David Herrmann, 2011
# Released under the terms of the GPLv2
# See COPYING for details
#

#
# Build the Wii Remote Kernel Driver
#
# Starting with linux-3.1 the upstream linux kernel includes the Wii Remote
# driver. You do not need to compile it on your own. Please read below if you
# want to compile an out-of-tree module, anyway:
#
# This builds the wiimote driver for the currently running kernel. This
# distribution contains a recent stable source code of the driver
# (hid-wiimote-*.c), but if you want a more recent one with unstable features,
# copy the files ./hid/drivers/hid-wiimote* from the xwiimote kernel repository
# into this directory.
# "make copy" does exactly this, assuming you have checked out a kernel source
# in the same directory as "xwiimote". You can adjust DRVPATH to point to your
# kernel source directory if you want to use "make copy". This path is
# relative to this "./driver/" directory.
#
# New wiimote driver releases split the source into multiple files so the
# correct list of files is:
#  hid-wiimote.h hid-wiimote-core.c hid-wiimote-ext.c hid-wiimote-debug.c
#
# Run "make" or "make build" to compile the source. This will produce a
# hid-wiimote.ko file which can be loaded with: "insmod ./hid-wiimote.ko"
#
# "make clean" will remove all temporary files.
#

ifeq ($(KERNELRELEASE),)

DRVPATH=../../kernel/xwiimote_kernel
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)
.PHONY: build copy clean test

build:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules

copy:
	@cp -v $(DRVPATH)/drivers/hid/hid-wiimote*.[ch] .

clean:
	@rm -rvf .tmp_versions/ .*.cmd *.o *.ko *.mod.? modules.order
	@rm -rvf Module.symvers *.orig *.rej

test:
	make copy build ; sudo rmmod hid-wiimote ; sudo insmod ./hid-wiimote.ko

else

hid-wiimote-y		:= hid-wiimote-core.o
ifdef CONFIG_HID_WIIMOTE_EXT
	hid-wiimote-y	+= hid-wiimote-ext.o
endif
ifdef CONFIG_DEBUG_FS
	hid-wiimote-y	+= hid-wiimote-debug.o
endif

obj-m := hid-wiimote.o

endif
