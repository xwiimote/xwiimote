#
# XWiimote - tools - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: all clean

all: xwiiconn xwii

clean:
	@rm -fv xwiiconn xwii

xwiiconn: xwiiconn.c
	gcc -o xwiiconn xwiiconn.c -Wall -O0 -g -lbluetooth `pkg-config --libs --cflags dbus-glib-1 glib-2.0`

xwii: xwii.c ../lib/libxwiimote.so
	gcc -o xwii xwii.c -Wall -O0 -g ../lib/libxwiimote.so -I../lib

../lib/libxwiimote.so:
	@echo Making library
	@cd ../lib && make --no-print-directory
	@echo Library done
