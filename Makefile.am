#
# XWiimote - makefile
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

#
# miscellaneous
#

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_MAKEFLAGS = --no-print-directory
AUTOMAKE_OPTIONS = color-tests
AM_DISTCHECK_CONFIGURE_FLAGS = --enable-debug

SUBDIRS = .

.DELETE_ON_ERROR:

#
# Distribution includes
#

EXTRA_DIST = \
	README \
	COPYING \
	DEV \
	LICENSE \
	res/50-xorg-fix-xwiimote.conf

#
# Build targets
#

lib_LTLIBRARIES = libxwiimote.la
bin_PROGRAMS = xwiishow
noinst_PROGRAMS = xwiidump
include_HEADERS = lib/xwiimote.h
man_MANS = \
	doc/xwiimote.7 \
	doc/libxwiimote.7 \
	doc/xwiishow.1
EXTRA_DIST += $(man_MANS)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libxwiimote.pc
EXTRA_DIST += \
	$(pkgconfig_DATA) \
	libxwiimote.pc.in

#
# Default c/l-flags
#

AM_CFLAGS = \
	-Wall \
	-pipe \
	-fno-common \
	-ffast-math \
	-fdiagnostics-show-option \
	-fno-strict-aliasing \
	-ffunction-sections \
	-fdata-sections \
	-fstack-protector
AM_CPPFLAGS = \
	-include $(top_builddir)/config.h \
	-I $(srcdir)/lib
AM_LDFLAGS = \
	-Wall \
	-Wl,--as-needed \
	-Wl,--gc-sections \
	-Wl,-z,relro \
	-Wl,-z,now

if DEBUG
AM_CFLAGS += -O0 -g
else
AM_CFLAGS += -O2
endif

#
# libxwiimote
#

libxwiimote_la_SOURCES = \
	lib/xwiimote.h \
	lib/core.c \
	lib/monitor.c

libxwiimote_la_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(UDEV_CFLAGS)
libxwiimote_la_LIBADD = \
	$(AM_LIBADD) \
	$(UDEV_LIBS)
libxwiimote_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info 1:0:0

#
# xwiishow
#

xwiishow_SOURCES = \
	tools/xwiishow.c
xwiishow_CPPFLAGS = \
	$(AM_CPPFLAGS) \
	$(NCURSES_CFLAGS)
xwiishow_LDADD = \
	$(NCURSES_LIBS) \
	libxwiimote.la \
	-lm
xwiishow_LDFLAGS = \
	$(AM_LDFLAGS)

#
# xwiidump
#

xwiidump_SOURCES = \
	tools/xwiidump.c
xwiidump_CPPFLAGS = \
	$(AM_CPPFLAGS)
xwiidump_LDFLAGS = \
	$(AM_LDFLAGS)
