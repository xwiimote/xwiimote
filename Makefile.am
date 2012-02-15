#
# XWiimote - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libxwiimote.la libxwiimote_compat.la
bin_PROGRAMS = xwiishow xwiikeymap
noinst_PROGRAMS = xwiidump
include_HEADERS = lib/xwiimote.h

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
	lib/xwiimote.h \
	lib/event.c \
	lib/udev.c \
	lib/libsfs.h lib/sfs.c

libxwiimote_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(UDEV_CFLAGS)
libxwiimote_la_LIBADD = \
	$(UDEV_LIBS)
libxwiimote_la_LDFLAGS = \
	-version-info 0:0:0

libxwiimote_compat_la_SOURCES = \
	compat/xwiimote_cwiid.h \
	compat/xwiimote_wiiuse.h \
	compat/wiiuse.c \
	compat/cwiid.c

libxwiimote_compat_la_CPPFLAGS = \
	$(AM_CPPFLAGS)
libxwiimote_compat_la_LIBADD = \
	libxwiimote.la
libxwiimote_compat_la_LDFLAGS = \
	-version-info 0:0:0

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

EXTRA_DIST = README COPYING DEV
