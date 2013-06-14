#!/bin/sh

#
# XWiimote - Generate Build Files
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

set -e
mkdir -p m4/
autoreconf -i

if test ! "x$NOCONFIGURE" = "x1" ; then
        ./configure --enable-debug $*
fi
