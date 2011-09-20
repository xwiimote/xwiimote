/*
 * XWiimote - tools - xwiishow
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "xwiimote.h"

static int run_iface(struct xwii_iface *iface)
{
	struct xwii_event event;
	int ret;

	ret = xwii_iface_read(iface, &event);
	printf("%d is-again: %d\n", ret, ret == -EAGAIN);

	return 0;
}

static int enumerate()
{
	struct xwii_monitor *mon;
	char *ent;

	mon = xwii_monitor_new(false, false);
	if (!mon) {
		printf("Cannot create monitor\n");
		return -EINVAL;
	}

	while ((ent = xwii_monitor_poll(mon))) {
		printf("  Found device: %s\n", ent);
		free(ent);
	}

	xwii_monitor_unref(mon);
	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;
	struct xwii_iface *iface;

	if (argc < 2) {
		printf("No device path given. Listing devices:\n");
		ret = enumerate();
		if (ret)
			return -ret;
		printf("End of device list\n");
		return 0;
	}

	ret = xwii_iface_new(&iface, "/sys/bus/hid/devices");
	if (ret) {
		printf("Cannot create xwii_iface %d\n", ret);
		return -ret;
	}

	ret = xwii_iface_open(iface, XWII_IFACE_CORE);
	if (ret)
		printf("Cannot open core iface %d\n", ret);
	else
		ret = run_iface(iface);

	xwii_iface_unref(iface);

	return -ret;
}
