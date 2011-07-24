#
# XWiimote - tools - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: all clean

all: xwiiconn xwii xwiishow xwiidump

clean:
	@rm -fv xwiiconn xwii xwiishow xwiidump

xwiiconn: xwiiconn.c
	gcc -o xwiiconn xwiiconn.c -Wall -O0 -g -lbluetooth `pkg-config --libs --cflags dbus-glib-1 glib-2.0`

xwii: xwii.c ../libxwiimote.so
	gcc -o xwii xwii.c -Wall -O0 -g ../libxwiimote.so -I../lib

xwiishow: xwiishow.c ../libxwiimote.so
	gcc -o xwiishow xwiishow.c -Wall -O0 -g ../libxwiimote.so -I../lib

xwiidump: xwiidump.c
	gcc -o xwiidump xwiidump.c -Wall -O0 -g

../libxwiimote.so:
	@echo Making library
	@cd .. && make libxwiimote.so --no-print-directory
	@echo Library done
