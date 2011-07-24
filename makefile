#
# XWiimote - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: build clean

build: libxwiimote.so

libxwiimote.so: lib/*.c
	gcc -shared -o libxwiimote.so lib/*.c -fPIC -Wall -O2 -ludev

clean:
	@rm -fv libxwiimote.so
