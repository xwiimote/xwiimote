#
# XWiimote - tools - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: all clean

all: xwiiconn xwiishow xwiidump

clean:
	@rm -fv xwiiconn xwiishow xwiidump

xwiiconn: xwiiconn.c
	gcc -o xwiiconn xwiiconn.c -Wall -lbluetooth `pkg-config --libs --cflags dbus-glib-1 glib-2.0`

xwiishow: xwiishow.c ../lib/libxwiimote.so
	gcc -o xwiishow xwiishow.c -Wall ../lib/libxwiimote.so -I../lib

xwiidump: xwiidump.c
	gcc -o xwiidump xwiidump.c -Wall

../lib/libxwiimote.so:
	@echo Making library
	@cd ../lib && make libxwiimote.so --no-print-directory
	@echo Library done
