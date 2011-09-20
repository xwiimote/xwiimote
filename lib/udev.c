/*
 * XWiimote - lib
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * Device Enumeration and Monitorig
 * Use libudev to enumerate all currently connected devices and allow
 * monitoring the system for new devices.
 * Normal applications should integrate this into their own udev-monitor.
 * However, smaller applications might not use udev on their own so this API
 * wraps the udev API in a small easy xwiimote API.
 */

#include <assert.h>
#include <fcntl.h>
#include <libudev.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "xwiimote.h"

struct xwii_monitor {
	size_t ref;
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *entry;
	struct udev_monitor *monitor;
};

struct xwii_monitor *xwii_monitor_new(bool poll, bool direct)
{
	struct udev *udev;
	struct udev_enumerate *enumerate = NULL;
	struct udev_list_entry *entry;
	struct udev_monitor *monitor = NULL;
	struct xwii_monitor *mon;

	udev = udev_new();
	if (!udev)
		return NULL;

	enumerate = udev_enumerate_new(udev);
	if (!enumerate)
		goto out;
	if (0 != udev_enumerate_add_match_subsystem(enumerate, "input"))
		goto out;
	if (0 != udev_enumerate_scan_devices(enumerate))
		goto out;
	entry = udev_enumerate_get_list_entry(enumerate);

	if (poll) {
		monitor = udev_monitor_new_from_netlink(udev,
						direct ? "kernel" : "udev");
		if (!monitor)
			goto out;
		if (udev_monitor_filter_add_match_subsystem_devtype(monitor,
								"input", NULL))
			goto out;
		if (udev_monitor_enable_receiving(monitor))
			goto out;
	}

	mon = malloc(sizeof(*mon));
	if (!mon)
		goto out;
	mon->ref = 1;
	mon->udev = udev;
	mon->enumerate = enumerate;
	mon->entry = entry;
	mon->monitor = monitor;

	return mon;

out:
	if (monitor)
		udev_monitor_unref(monitor);
	if (enumerate)
		udev_enumerate_unref(enumerate);
	udev_unref(udev);
	return NULL;
}

struct xwii_monitor *xwii_monitor_ref(struct xwii_monitor *mon)
{
	assert(mon);
	mon->ref++;
	assert(mon->ref);
	return mon;
}

static inline void free_enum(struct xwii_monitor *monitor)
{
	if (monitor->enumerate) {
		udev_enumerate_unref(monitor->enumerate);
		monitor->enumerate = NULL;
		monitor->entry = NULL;
	}
}

void xwii_monitor_unref(struct xwii_monitor *monitor)
{
	if (!monitor)
		return;

	assert(monitor->ref);

	if (--monitor->ref)
		return;

	free_enum(monitor);
	if (monitor->monitor)
		udev_monitor_unref(monitor->monitor);
	udev_unref(monitor->udev);
	free(monitor);
}

int xwii_monitor_get_fd(struct xwii_monitor *monitor, bool blocking)
{
	signed int fd, set;

	if (!monitor->monitor)
		return -1;

	fd = udev_monitor_get_fd(monitor->monitor);
	if (fd < 0)
		return -1;

	set = fcntl(fd, F_GETFL);
	if (set < 0)
		return -1;

	if (blocking)
		set &= ~O_NONBLOCK;
	else
		set |= O_NONBLOCK;

	if (0 != fcntl(fd, F_SETFL, set))
		return -1;

	return fd;
}

static struct udev_device *next_enum(struct xwii_monitor *monitor)
{
	struct udev_list_entry *e;
	struct udev_device *dev;
	const char *path;

	while (monitor->entry) {
		e = monitor->entry;
		monitor->entry = udev_list_entry_get_next(e);

		path = udev_list_entry_get_name(e);
		dev = udev_device_new_from_syspath(monitor->udev, path);
		if (dev)
			return dev;
	}

	free_enum(monitor);

	return NULL;
}

static char *make_device(struct udev_device *dev)
{
	const char *tmp;
	struct udev_device *p;
	char *ret = NULL;

	tmp = udev_device_get_action(dev);
	if (tmp && strcmp(tmp, "add"))
		goto out;

	tmp = udev_device_get_sysname(dev);
	if (!tmp || strncmp(tmp, "event", 5))
		goto out;

	p = udev_device_get_parent_with_subsystem_devtype(dev, "hid", NULL);
	if (!p)
		goto out;

	udev_device_ref(p);
	udev_device_unref(dev);
	dev = p;

	tmp = udev_device_get_property_value(dev, "HID_ID");
	if (!tmp || strcmp(tmp, "0005:0000057E:00000306"))
		goto out;

	tmp = udev_device_get_syspath(dev);
	if (tmp)
		ret = strdup(tmp);

out:
	udev_device_unref(dev);
	return ret;
}

char *xwii_monitor_poll(struct xwii_monitor *monitor)
{
	struct udev_device *dev;
	char *ret;

	if (monitor->enumerate) {
		while (1) {
			dev = next_enum(monitor);
			if (!dev)
				/* notify application of end of enum */
				return NULL;

			ret = make_device(dev);
			if (ret)
				return ret;
		}
	} else if (monitor->monitor) {
		while (1) {
			dev = udev_monitor_receive_device(monitor->monitor);
			if (!dev)
				return NULL;

			ret = make_device(dev);
			if (ret)
				return ret;
		}
	}

	return NULL;
}
