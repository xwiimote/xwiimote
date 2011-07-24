/*
 * XWiimote - tools
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * XWiimote Control Tool
 * This tool can be used to modify the connected wiimotes. It does not, however,
 * provide methods to connect or disconnect wiimotes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <xwiimote.h>

static void list()
{
	struct xwii_monitor *mon;
	struct xwii_dev *dev;

	mon = xwii_monitor_new(false, false);
	if (!mon)
		return;

	printf("Conected Wiimotes:\n");

	while ((dev = xwii_monitor_poll(mon))) {
		printf(" - device %p\n", dev);
		xwii_dev_free(dev);
	}

	printf("End of list\n");

	xwii_monitor_free(mon);
}

int main(int argc, char **argv)
{
	list();
	return EXIT_SUCCESS;
}
