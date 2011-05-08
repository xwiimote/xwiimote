#
# XWiimote - tools - hid driver probe
# Written 2011 by David Herrmann
# Dedicated to the Public Domain
#

#
# The xwiimote kernel device driver is called
# hid-wiimote and may not be probed automatically
# on new devices unless the kernel hid-core is
# patched to do so.
# This script iterates over all currently connected
# hid devices and binds the wiimote driver on
# them if they are identified as wiimotes.
#

DRIVER="wiimote"

checkret()
{
	if test ! "x$1" = "x0" ; then
		echo "Aborted..."
		exit 1
	fi
}

if test ! -d "/sys/bus/hid/drivers/$DRIVER" ; then
	echo "Cannot find wiimote hid driver"
	exit 1
fi

for DEV in /sys/bus/hid/devices/* ; do
	if test -d "$DEV" ; then
		NUM=`basename $DEV`
		NAME=`grep HID_NAME $DEV/uevent | grep -o "Nintendo RVL-CNT-01"`
		if test $? = 0 ; then
			echo "Enabling device $NUM '$NAME' at $DEV..."
			echo "$NUM" >/sys/bus/hid/drivers/generic-bluetooth/unbind
			if test $? = 0 ; then
				echo "$NUM" >/sys/bus/hid/drivers/$DRIVER/bind
				checkret $?
				echo "Device $NUM successfully activated"
			fi
		fi
	fi
done
