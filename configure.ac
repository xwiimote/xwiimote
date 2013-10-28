#
# XWiimote - configure
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

AC_PREREQ(2.68)

AC_INIT([xwiimote],
        [2],
        [http://github.com/dvdhrm/xwiimote/issues],
        [xwiimote],
        [http://dvdhrm.github.io/xwiimote])
AC_CONFIG_SRCDIR([lib/xwiimote.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER(config.h)
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign 1.11 subdir-objects dist-xz no-dist-gzip tar-pax -Wall -Werror -Wno-portability])
AM_SILENT_RULES([yes])

# Don't add a default "-g -O2" if CFLAGS wasn't specified
: ${CFLAGS=""}

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_LN_S
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

LT_PREREQ(2.2)
LT_INIT

PKG_CHECK_MODULES([UDEV], [libudev])
AC_SUBST(UDEV_CFLAGS)
AC_SUBST(UDEV_LIBS)

PKG_CHECK_MODULES([NCURSES], [ncurses])
AC_SUBST(NCURSES_CFLAGS)
AC_SUBST(NCURSES_LIBS)

AC_MSG_CHECKING([whether to build with debugging on])
AC_ARG_ENABLE([debug],
              [AS_HELP_STRING([--enable-debug], [whether to build with debugging on)])],
              [debug="$enableval"],
              [debug=no; AC_DEFINE([NDEBUG], [1], [No Debug])])
AM_CONDITIONAL([DEBUG], [test x$debug = xyes])
AC_MSG_RESULT([$debug])

#
# Check for doxygen
#

AC_ARG_VAR([DOXYGEN], [Path to doxygen command])
AC_ARG_WITH(doxygen,
            AS_HELP_STRING([--with-doxygen],
                           [Use doxygen to generate documentation]),
            [use_doxygen=$withval],
            [use_doxygen=auto])
have_doxygen=no
if test ! "x$use_doxygen" = "xno" ; then
        AC_PATH_PROG([DOXYGEN], [doxygen])
        if test "x$DOXYGEN" = "x" ; then
                if test "x$use_doxygen" = "xyes" ; then
                        AC_MSG_ERROR([--with-doxygen=yes specified but doxygen not found in PATH])
                fi
        else
                have_doxygen=yes
        fi
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test "$have_doxygen" = yes])

#
# Write configuration
#

AC_CONFIG_FILES([Makefile libxwiimote.pc doc/Doxyfile])
AC_OUTPUT

AC_MSG_NOTICE([Build configuration:

               prefix: $prefix
          exec-prefix: $exec_prefix
               libdir: $libdir
           includedir: $includedir

                debug: $debug
              doxygen: $have_doxygen

        Run "${MAKE-make}" to start compilation process])
