#
# XWiimote - configure
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

AC_PREREQ(2.68)

AC_INIT([xwiimote], [0.3])
AC_SUBST(PACKAGE_URL, [https://github.com/dvdhrm/xwiimote])
AC_CONFIG_SRCDIR([lib/xwiimote.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER(config.h)

AM_INIT_AUTOMAKE([foreign 1.11 subdir-objects dist-bzip2 no-dist-gzip tar-pax -Wall -Werror])
AM_SILENT_RULES([yes])

# Don't add a default "-g -O2" if CFLAGS wasn't specified
: ${CFLAGS=""}

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AM_PROG_AR

LT_PREREQ(2.2)
LT_INIT

PKG_CHECK_MODULES([UDEV], [libudev])
AC_SUBST(UDEV_CFLAGS)
AC_SUBST(UDEV_LIBS)

AC_MSG_CHECKING([whether to build with debugging on])
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [whether to build with debugging on)])],
              [debug="$enableval"],
              [debug=no; AC_DEFINE([NDEBUG], [1], [No Debug])])
AM_CONDITIONAL([DEBUG], [test x$debug = xyes])
AC_MSG_RESULT([$debug])

AC_CONFIG_FILES([Makefile libxwiimote.pc])
AC_OUTPUT
