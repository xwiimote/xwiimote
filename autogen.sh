#!/bin/sh

#
# XWiimote - Generate Build Files
# Written by David Herrmann, 2011
# Dedicated to the Public Domain
#

mkdir -p m4/
autoreconf -i
./configure --enable-debug $*
