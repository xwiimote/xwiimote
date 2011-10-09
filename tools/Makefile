#
# XWiimote - tools - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: all clean

all: xwiishow xwiidump

clean:
	@rm -fv xwiishow xwiidump

xwiishow: xwiishow.c ../lib/libxwiimote.so
	gcc -o xwiishow xwiishow.c -Wall ../lib/libxwiimote.so -I../lib -lncurses

xwiidump: xwiidump.c
	gcc -o xwiidump xwiidump.c -Wall

../lib/libxwiimote.so:
	@echo Making library
	@cd ../lib && make libxwiimote.so --no-print-directory
	@echo Library done
