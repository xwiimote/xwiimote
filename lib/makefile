#
# XWiimote - lib - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: clean all

all:
	gcc -shared -o libxwiimote.so *.c -fPIC -Wall -O2 -ludev

clean:
	@rm -fv libxwiimote.so
