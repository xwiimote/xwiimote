#!/bin/sh

#
# XWiimote - tools
# Written 2011-2013 by David Herrmann
# Dedicated to the Public Domain
#

#
# This script enables auto-reconnect on your wiimote. This script should only be
# used if you really require auto-reconnect now. There is work going on to get
# auto-reconnect of wiimotes into upstream bluez repository.
# If you need this feature now, you may read below, but be aware that this
# method described here is neither fast nor reliable. It is a small hack to get
# it work.
#
# Please specify your device bdaddr as first argument to this script.
#
# To run this tool, you need:
#  - "simple-agent"
#  - "test-device"
#  - "test-input"
# from the "test" directory of the bluez distribution. They are available here:
#   http://git.kernel.org/?p=bluetooth/bluez.git;a=tree;f=test
# They are GPL licensed and hence not included here. They are simple python
# scripts and can be just copied into this directory.
# The scripts uses "bluez-" as default prefix for these tools. You can pass
# another prefix as second argument to overwrite this.
# You can also specify BIN_TESTINP, BIN_TESTDEV, BIN_SIMPLEA as environment
# variables with the absolute/relative path to the given binaries. The prefix
# will not be applied in this case.
#
# The python scripts need "python2" and are not python3 compatible, so specify
# the python interpreter as "PYTHON" below if the default value does not work.
# Or pass PYTHON="<path>" as environment variable.
#
# This script REQUIRES that you have the "wiimote.so" plugin enabled in your
# bluetoothd daemon. It is often NOT part of the official distribution package
# so check whether it is installed and enabled.
#
# Please disable gnome-bluetooth, blueman or any similar bluetooth applet to
# avoid inter-process conflicts.
#
# This script does not check for error codes, so if you see errors, abort the
# script with ctrl+c. This script first removes the device. Then it connects to
# the device without pairing and without connecting to the input service. It
# does this just to retrieve SDP values. After that you should disconnect the
# device again by pressing the power-button for 3 seconds.
# Then press the red-sync button again, the script will now connect to the
# device and perform pairing. If the script asks you for PIN input, then you did
# not install the "wiimote.so" plugin for bluetoothd.
# If you did, bluetoothd chooses the right PIN for you. After pairing it
# directly connects to the input device. If this succeeds, the wiimote is ready
# for auto-reconnect.
#
# To test auto-reconnect, disconnect your wiimote. Then invoke
# "python2 ./simple-agent"
# And you will have an agent that listens for incoming connections. Now a single
# key-press on the remote should be enough to make the wiimote connect to your
# host. You only need to acknowledge the connection in the simple-agent by
# writing "yes" when it prompts you.
#

# pass PYTHON=xy to overwrite this
if test "x$PYTHON" = "x" ; then
	PYTHON="python2"
fi

# first argument is bdaddr
DEV="$1"
if test "x$1" = "x" ; then
	echo "Please specify bdaddr of wiimote as first argument"
	exit 1
fi

# optional second argument is binary prefix (default: "bluez-")
PREFIX="bluez-"
if test $# -gt 1 ; then
	PREFIX="$2"
fi

# Pass BIN_TESTDEV=test-device, BIN_SIMPLEA=simple-agent, BIN_TESTINP=test-input
# to overwrite the "which" statements.
ERR=0
if test "x$BIN_TESTDEV" = "x" ; then
	BIN_TESTDEV=`which "${PREFIX}test-device" 2>/dev/null`
	ERR=$(($ERR + $?))
fi

if test "x$BIN_SIMPLEA" = "x" ; then
	BIN_SIMPLEA=`which "${PREFIX}simple-agent" 2>/dev/null`
	ERR=$(($ERR + $?))
fi

if test "x$BIN_TESTINP" = "x" ; then
	BIN_TESTINP=`which "${PREFIX}test-input" 2>/dev/null`
	ERR=$(($ERR + $?))
fi

if test ! "x$ERR" = "x0" ; then
	echo "Cannot find bluez '${PREFIX}test-device', '${PREFIX}simple-agent' or '${PREFIX}test-input' scripts"
	exit 1
fi

echo "Removing device..."
"$PYTHON" "$BIN_TESTDEV" remove "$DEV"
echo "Device removed, press any key to continue"
read tmp

echo "Please press red sync-button on the back of the wiimote and press any key
to continue"
echo "If this asks you for PIN input, then your bluetoothd daemon does not
include the wiimote.so plugin. Please install it or contact your distributor."
read tmp
"$PYTHON" "$BIN_TESTDEV" create "$DEV"
echo "Please disconnect the device by pressing the power button and then press
any key to continue"
read tmp
echo "Now press the red-sync button again and press any key to continue"
read tmp

echo "Pairing with the remote device..."
"$PYTHON" "$BIN_SIMPLEA" "hci0" "$DEV"
echo "Connecting to input device..."
"$PYTHON" "$BIN_TESTINP" connect "$DEV"
echo "Connected to input device. Autoconnect should be enabled now."
