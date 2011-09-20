#
# XWiimote - lib - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

SFSPATH=../../misc/libmisc/libsfs

all: libxwiimote.so

# build xwiimote library
libxwiimote.so: event.c udev.c sfs.c xwiimote.h libsfs.h
	gcc -o $@ $^ -ludev -Wall -shared -fPIC

#
# The copy rule is only used by maintainers to copy the most recent version of
# the libsfs snippet. This snippet is maintained elsewhere and needs to be kept
# up to date here.
#
copy:
	@cp -v $(SFSPATH)/src/*.h $(SFSPATH)/src/*.c .

# clean directory from generated binaries
clean:
	@rm -vfr *.o *.so *.bin

.PHONY: all copy build clean
