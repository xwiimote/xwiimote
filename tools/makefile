#
# XWiimote - tools - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

.PHONY: all clean

all: xwiiconn

clean:
	@rm -fv xwiiconn

xwiiconn: xwiiconn.c
	gcc -o xwiiconn xwiiconn.c -Wall -O0 -g -lbluetooth `pkg-config --libs --cflags dbus-glib-1 glib-2.0`
