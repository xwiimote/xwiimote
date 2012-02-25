#
# XWiimote - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	README \
	COPYING \
	DEV \
	LICENSE \
	res/50-xorg-fix-xwiimote.conf

lib_LTLIBRARIES = libxwiimote.la
bin_PROGRAMS = xwiishow xwiikeymap
noinst_PROGRAMS = xwiidump
include_HEADERS = lib/xwiimote.h
man_MANS = \
	doc/xwiimote.7 \
	doc/libxwiimote.7 \
	doc/xwiishow.1 \
	doc/xwiikeymap.1
EXTRA_DIST += $(man_MANS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxwiimote.pc
EXTRA_DIST += $(pkgconfig_DATA)

AM_CFLAGS = \
	-Wall
AM_CPPFLAGS = \
	-I $(srcdir)/lib
AM_LDFLAGS = \
	-Wall

if DEBUG
AM_CFLAGS += -O0 -g
else
AM_CFLAGS += -O2
endif

libxwiimote_la_SOURCES = \
	lib/internal.h \
	lib/xwiimote.h \
	lib/core.c \
	lib/monitor.c \
	lib/sfs.c

libxwiimote_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(UDEV_CFLAGS)
libxwiimote_la_LIBADD = \
	$(UDEV_LIBS)
libxwiimote_la_LDFLAGS = \
	-version-info 1:0:0

xwiishow_SOURCES = \
	tools/xwiishow.c
xwiishow_LDADD = \
	libxwiimote.la
xwiishow_LDFLAGS = \
	$(AM_LDFLAGS) \
	-lncurses

xwiikeymap_SOURCES = \
	tools/xwiikeymap.c \
	tools/eloop.c tools/eloop.h \
	tools/log.c tools/log.h
xwiikeymap_LDADD = \
	libxwiimote.la

xwiidump_SOURCES = \
	tools/xwiidump.c
