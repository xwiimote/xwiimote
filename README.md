Overview {#mainpage}
========

xwiimote is an open-source device driver for Nintendo Wii / Wii U remotes. It
contains tools and libraries which work together with the official hid-wiimote
kernel driver, available since linux-3.1.

If you want to use a Nintendo Wii Remote or any compatible device (including the
Nintendo Wii Balance Board, Nintendo Wii U Pro Controller, and more) on your
linux system, the xwiimote software stack provides everything you need.

This distribution is hosted at:
  http://dvdhrm.github.io/xwiimote
Use this website to contact the maintainers or developers or to file bug
reports.

Install
=======

To install the libxwiimote.so library and the related tools, use:
	$ ./configure [--enable-debug] [--prefix=/usr]
	$ make
	$ make install
If "configure" is not available, use:
	$ ./autogen.sh [<configure-flags>]

Dependencies:
	- libudev: Used for device enumeration
	- ncurses: Used for UI of xwiishow

This software packages contains:
	libxwiimote.so: A userspace library which helps accessing connected Wii
		Remotes in the system. It can be used by applications to use Wii
		Remotes as input. You can also use the direct kernel interface,
		though.
	xwiishow: A test application which lists all connected Wii Remotes. If a
		Wii Remote is passed as argument, it continuously reads input
		from the device and prints it to the screen.
	50-xorg-fix-xwiimote.conf: X configuration file which should be
		installed into /etc/X11/xorg.conf.d/ by your distribution. It
		adds all Wii Remotes to the input blacklist of the X-server.
		This is needed since the raw Wii Remote kernel interface is
		useless (even irritating) to the X-server. Instead the
		xf86-input-xwiimote driver should be used.
	manpages: Several manpages are provided. One overview page and several
		other pages for each tool and feature. They are installed by
		the autotools scripts automatically alongside the library and
		applications.

The following tools are available but not installed into the system:
	xwiidump: A test application which reads the EEPROM memory of a
		connected Wii Remote and prints it to stdout. This requires
		debugfs support in the kernel and the hid-wiimote kernel module.

Following software is not part of this package:
	hid-wiimote.ko: The wiimote kernel module is available in the official
		linux kernel sources and should already be installed in your
		system if you run linux-3.1 or newer.
	wiimote.so: The BlueZ bluetoothd wiimote plugin is part of the upstream
		BlueZ package and available since bluez-4.96. It should be
		already installed on your system.

Usage
=====

Please see the website for help:
  http://dvdhrm.github.io/xwiimote

Documentation
=============

./doc/website/index.html: Official website of XWiimote and documentation
./doc/*.3: Manpages
./doc/DEVICES: Enumeration of all Nintendo Wii / Wii U related devices
./doc/DEV_*: Device communication/protocol descriptions

Development
===========

Please see ./DEV for development information.

Copying
=======

Please see ./COPYING for more information.

Compatibility
=============

Two other commonly used Wii-Remote libraries are cwiid and wiiuse. Both provide
a full user-space driver and were used heavily in the past. We do not provide
API compatibility to them as both APIs are very restricted:

	- cwiid: This API is highly asynchronous with threads. It does not allow
	  a single-threaded application to use it. This makes it very hard to
	  use and we decided not to provide a compatibility layer as we do not
	  want to support such library-API designs.
	- wiiuse: This API is tightly bound to the idea that the application is
	  supposed to discover and connect the Bluetooth devices. It uses a
	  static array for all used Wii Remotes and requires callbacks for
	  notification. Same as cwiid, we do not provide a compatibility layer
	  as the API is very different from our API.

We designed our API to have as few restrictions as possible. We do not force an
application to use a special programming technique. Instead, an application can
use our API to support event-based, callback-based or thread-based event-loops
or any other kind that it wants. We achieve this by keeping the user-space API
very similar to the kernel-API so the application can integrate this into any
infrastructure it wants.

Contact
=======

Website:
  http://dvdhrm.github.io/xwiimote

For email contact please see ./COPYING for a list of contributors or
write an email to the current maintainer at:
  dh.herrmann@gmail.com
