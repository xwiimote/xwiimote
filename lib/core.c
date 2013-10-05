/*
 * XWiimote - lib
 * Written 2010-2013 by David Herrmann <dh.herrmann@gmail.com>
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libudev.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "xwiimote.h"

/* interfaces */
enum xwii_if_base_idx {
	/* base interfaces */
	XWII_IF_CORE,
	XWII_IF_ACCEL,
	XWII_IF_IR,

	/* extension interfaces */
	XWII_IF_MOTION_PLUS,
	XWII_IF_NUNCHUK,
	XWII_IF_CLASSIC_CONTROLLER,
	XWII_IF_BALANCE_BOARD,
	XWII_IF_PRO_CONTROLLER,
	XWII_IF_DRUMS,
	XWII_IF_GUITAR,

	XWII_IF_NUM,
};

/* event interface */
struct xwii_if {
	/* device node as /dev/input/eventX or NULL */
	char *node;
	/* open file or -1 */
	int fd;
	/* temporary state during device detection */
	unsigned int available : 1;
};

/* main device interface */
struct xwii_iface {
	/* reference count */
	size_t ref;
	/* epoll file descriptor */
	int efd;
	/* udev context */
	struct udev *udev;
	/* main udev device */
	struct udev_device *dev;
	/* udev monitor */
	struct udev_monitor *umon;

	/* bitmask of open interfaces */
	unsigned int ifaces;
	/* interfaces */
	struct xwii_if ifs[XWII_IF_NUM];
	/* device type attribute */
	char *devtype_attr;
	/* extension attribute */
	char *extension_attr;
	/* battery capacity attribute */
	char *battery_attr;
	/* led brightness attributes */
	char *led_attrs[4];

	/* rumble-id for base-core interface force-feedback or -1 */
	int rumble_id;
	int rumble_fd;
	/* accelerometer data cache */
	struct xwii_event_abs accel_cache;
	/* IR data cache */
	struct xwii_event_abs ir_cache[4];
	/* balance board weight cache */
	struct xwii_event_abs bboard_cache[4];
	/* motion plus cache */
	struct xwii_event_abs mp_cache;
	/* motion plus normalization */
	struct xwii_event_abs mp_normalizer;
	int32_t mp_normalize_factor;
	/* pro controller cache */
	struct xwii_event_abs pro_cache[2];
	/* classic controller cache */
	struct xwii_event_abs classic_cache[3];
	/* nunchuk cache */
	struct xwii_event_abs nunchuk_cache[2];
	/* drums cache */
	struct xwii_event_abs drums_cache[XWII_DRUMS_ABS_NUM];
	/* guitar cache */
	struct xwii_event_abs guitar_cache[3];
};

/* table to convert interface to name */
static const char *if_to_name_table[] = {
	[XWII_IF_CORE] = XWII_NAME_CORE,
	[XWII_IF_ACCEL] = XWII_NAME_ACCEL,
	[XWII_IF_IR] = XWII_NAME_IR,
	[XWII_IF_MOTION_PLUS] = XWII_NAME_MOTION_PLUS,
	[XWII_IF_NUNCHUK] = XWII_NAME_NUNCHUK,
	[XWII_IF_CLASSIC_CONTROLLER] = XWII_NAME_CLASSIC_CONTROLLER,
	[XWII_IF_BALANCE_BOARD] = XWII_NAME_BALANCE_BOARD,
	[XWII_IF_PRO_CONTROLLER] = XWII_NAME_PRO_CONTROLLER,
	[XWII_IF_DRUMS] = XWII_NAME_DRUMS,
	[XWII_IF_GUITAR] = XWII_NAME_GUITAR,
	[XWII_IF_NUM] = NULL,
};

/* convert name to interface or -1 */
static int name_to_if(const char *name)
{
	unsigned int i;

	for (i = 0; i < XWII_IF_NUM; ++i)
		if (!strcmp(name, if_to_name_table[i]))
			return i;

	return -1;
}

/* table to convert interface to public interface */
static unsigned int if_to_iface_table[] = {
	[XWII_IF_CORE] = XWII_IFACE_CORE,
	[XWII_IF_ACCEL] = XWII_IFACE_ACCEL,
	[XWII_IF_IR] = XWII_IFACE_IR,
	[XWII_IF_MOTION_PLUS] = XWII_IFACE_MOTION_PLUS,
	[XWII_IF_NUNCHUK] = XWII_IFACE_NUNCHUK,
	[XWII_IF_CLASSIC_CONTROLLER] = XWII_IFACE_CLASSIC_CONTROLLER,
	[XWII_IF_BALANCE_BOARD] = XWII_IFACE_BALANCE_BOARD,
	[XWII_IF_PRO_CONTROLLER] = XWII_IFACE_PRO_CONTROLLER,
	[XWII_IF_DRUMS] = XWII_IFACE_DRUMS,
	[XWII_IF_GUITAR] = XWII_IFACE_GUITAR,
	[XWII_IF_NUM] = 0,
};

/* table to convert public interface to internal interface */
static int iface_to_if_table[] = {
	[0 ... XWII_IFACE_ALL] = -1,
	[XWII_IFACE_CORE] = XWII_IF_CORE,
	[XWII_IFACE_ACCEL] = XWII_IF_ACCEL,
	[XWII_IFACE_IR] = XWII_IF_IR,
	[XWII_IFACE_MOTION_PLUS] = XWII_IF_MOTION_PLUS,
	[XWII_IFACE_NUNCHUK] = XWII_IF_NUNCHUK,
	[XWII_IFACE_CLASSIC_CONTROLLER] = XWII_IF_CLASSIC_CONTROLLER,
	[XWII_IFACE_BALANCE_BOARD] = XWII_IF_BALANCE_BOARD,
	[XWII_IFACE_PRO_CONTROLLER] = XWII_IF_PRO_CONTROLLER,
	[XWII_IFACE_DRUMS] = XWII_IF_DRUMS,
	[XWII_IFACE_GUITAR] = XWII_IF_GUITAR,
};

/* convert name to interface or -1 */
static int if_to_iface(unsigned int ifs)
{
	return if_to_iface_table[ifs];
}

XWII__EXPORT
const char *xwii_get_iface_name(unsigned int iface)
{
	if (iface > XWII_IFACE_ALL)
		return NULL;
	if (iface_to_if_table[iface] == -1)
		return NULL;

	return if_to_name_table[iface_to_if_table[iface]];
}

/*
 * Scan the device \dev for child input devices and update our device-node
 * cache with the new information. This is called during device setup to
 * find all /dev/input/eventX nodes for all currently available interfaces.
 * We also cache attribute paths for sub-devices like LEDs or batteries.
 *
 * When called during hotplug-events, this updates all currently known
 * information and removes nodes that are no longer present.
 */
static int xwii_iface_read_nodes(struct xwii_iface *dev)
{
	struct udev_enumerate *e;
	struct udev_list_entry *list;
	struct udev_device *d;
	const char *name, *node, *subs;
	char *n;
	int ret, prev_if, tif, len, i;
	unsigned int ifs;

	e = udev_enumerate_new(dev->udev);
	if (!e)
		return -ENOMEM;

	ret = udev_enumerate_add_match_subsystem(e, "input");
	ret += udev_enumerate_add_match_subsystem(e, "leds");
	ret += udev_enumerate_add_match_subsystem(e, "power_supply");
	ret += udev_enumerate_add_match_parent(e, dev->dev);
	if (ret) {
		udev_enumerate_unref(e);
		return -ENOMEM;
	}

	ret = udev_enumerate_scan_devices(e);
	if (ret) {
		udev_enumerate_unref(e);
		return ret;
	}

	for (i = 0; i < XWII_IF_NUM; ++i)
		dev->ifs[i].available = 0;

	/* The returned list is sorted. So we first get an inputXY entry,
	 * possibly followed by the inputXY/eventXY entry. We remember the type
	 * of a found inputXY entry, and check the next list-entry, whether
	 * it's an eventXY entry. If it is, we save the node, otherwise, it's
	 * skipped.
	 * For other subsystems we simply cache the attribute paths. */
	prev_if = -1;
	for (list = udev_enumerate_get_list_entry(e);
	     list;
	     list = udev_list_entry_get_next(list), udev_device_unref(d)) {

		tif = prev_if;
		prev_if = -1;

		name = udev_list_entry_get_name(list);
		d = udev_device_new_from_syspath(dev->udev, name);
		if (!d)
			continue;

		subs = udev_device_get_subsystem(d);
		if (!strcmp(subs, "input")) {
			name = udev_device_get_sysname(d);
			if (!strncmp(name, "input", 5)) {
				name = udev_device_get_sysattr_value(d, "name");
				if (!name)
					continue;

				tif = name_to_if(name);
				if (tif >= 0)
					prev_if = tif;
			} else if (!strncmp(name, "event", 5)) {
				if (tif < 0)
					continue;

				node = udev_device_get_devnode(d);
				if (!node)
					continue;

				if (dev->ifs[tif].node &&
				    !strcmp(node, dev->ifs[tif].node)) {
					dev->ifs[tif].available = 1;
					continue;
				} else if (dev->ifs[tif].node) {
					xwii_iface_close(dev,
							 if_to_iface(tif));
					free(dev->ifs[tif].node);
					dev->ifs[tif].node = NULL;
				}

				n = strdup(node);
				if (!n)
					continue;

				dev->ifs[tif].node = n;
				dev->ifs[tif].available = 1;
			}
		} else if (!strcmp(subs, "leds")) {
			len = strlen(name);
			if (name[len - 1] == '0')
				i = 0;
			else if (name[len - 1] == '1')
				i = 1;
			else if (name[len - 1] == '2')
				i = 2;
			else if (name[len - 1] == '3')
				i = 3;
			else
				continue;

			if (dev->led_attrs[i])
				continue;

			ret = asprintf(&dev->led_attrs[i], "%s/%s",
				       name, "brightness");
			if (ret <= 0)
				dev->led_attrs[i] = NULL;
		} else if (!strcmp(subs, "power_supply")) {
			if (dev->battery_attr)
				continue;
			ret = asprintf(&dev->battery_attr, "%s/%s",
				       name, "capacity");
			if (ret <= 0)
				dev->battery_attr = NULL;
		}
	}

	udev_enumerate_unref(e);

	/* close no longer available ifaces */
	ifs = 0;
	for (i = 0; i < XWII_IF_NUM; ++i) {
		if (!dev->ifs[i].available && dev->ifs[i].node) {
			free(dev->ifs[i].node);
			dev->ifs[i].node = NULL;
			ifs |= if_to_iface(i);
		}
	}
	xwii_iface_close(dev, ifs);

	return 0;
}

/*
 * Create new interface structure
 * This creates a new interface for a single Wii Remote device. \syspath must
 * point to the base-directory of the device. It can normally be found as:
 *   /sys/bus/hid/devices/<device>
 * The device is validated and 0 is returned on success. On failure, a negative
 * error code is returned.
 * A pointer to the new object is stored in \dev. \dev is left untouched on
 * failure.
 * Initial refcount is 1 so you need to call *_unref() to free the device.
 */
XWII__EXPORT
int xwii_iface_new(struct xwii_iface **dev, const char *syspath)
{
	struct xwii_iface *d;
	const char *driver, *subs;
	int ret, i;

	if (!dev || !syspath)
		return -EINVAL;

	d = malloc(sizeof(*d));
	if (!d)
		return -ENOMEM;

	memset(d, 0, sizeof(*d));
	d->ref = 1;
	d->rumble_id = -1;
	d->rumble_fd = -1;

	for (i = 0; i < XWII_IF_NUM; ++i)
		d->ifs[i].fd = -1;

	d->efd = epoll_create1(EPOLL_CLOEXEC);
	if (d->efd < 0) {
		ret = -EFAULT;
		goto err_free;
	}

	d->udev = udev_new();
	if (!d->udev) {
		ret = -ENOMEM;
		goto err_efd;
	}

	d->dev = udev_device_new_from_syspath(d->udev, syspath);
	if (!d->dev) {
		ret = -ENODEV;
		goto err_udev;
	}

	driver = udev_device_get_driver(d->dev);
	subs = udev_device_get_subsystem(d->dev);
	if (!driver || strcmp(driver, "wiimote") ||
	    !subs || strcmp(subs, "hid")) {
		ret = -ENODEV;
		goto err_dev;
	}

	ret = asprintf(&d->devtype_attr, "%s/%s", syspath, "devtype");
	if (ret <= 0) {
		ret = -ENOMEM;
		goto err_dev;
	}

	ret = asprintf(&d->extension_attr, "%s/%s", syspath, "extension");
	if (ret <= 0) {
		ret = -ENOMEM;
		goto err_attrs;
	}

	ret = xwii_iface_read_nodes(d);
	if (ret)
		goto err_attrs;

	*dev = d;
	return 0;

err_attrs:
	free(d->extension_attr);
	free(d->devtype_attr);
err_dev:
	udev_device_unref(d->dev);
err_udev:
	udev_unref(d->udev);
err_efd:
	close(d->efd);
err_free:
	free(d);
	return ret;
}

XWII__EXPORT
void xwii_iface_ref(struct xwii_iface *dev)
{
	if (!dev || !dev->ref)
		return;

	dev->ref++;
}

XWII__EXPORT
void xwii_iface_unref(struct xwii_iface *dev)
{
	unsigned int i;

	if (!dev || !dev->ref || --dev->ref)
		return;

	xwii_iface_close(dev, XWII_IFACE_ALL);
	xwii_iface_watch(dev, false);

	for (i = 0; i < XWII_IF_NUM; ++i)
		free(dev->ifs[i].node);
	for (i = 0; i < 4; ++i)
		free(dev->led_attrs[i]);
	free(dev->battery_attr);
	free(dev->extension_attr);
	free(dev->devtype_attr);

	udev_device_unref(dev->dev);
	udev_unref(dev->udev);
	close(dev->efd);
	free(dev);
}

XWII__EXPORT
const char *xwii_iface_get_syspath(struct xwii_iface *dev)
{
	if (!dev)
		return NULL;

	return udev_device_get_syspath(dev->dev);
}

XWII__EXPORT
int xwii_iface_get_fd(struct xwii_iface *dev)
{
	if (!dev)
		return -1;

	return dev->efd;
}

XWII__EXPORT
int xwii_iface_watch(struct xwii_iface *dev, bool watch)
{
	int fd, ret, set;
	struct epoll_event ep;

	if (!dev)
		return -EINVAL;

	if (!watch) {
		/* remove device watch descriptor */

		if (!dev->umon)
			return 0;

		fd = udev_monitor_get_fd(dev->umon);
		epoll_ctl(dev->efd, EPOLL_CTL_DEL, fd, NULL);
		udev_monitor_unref(dev->umon);
		dev->umon = NULL;
		return 0;
	}

	/* add device watch descriptor */

	if (dev->umon)
		return 0;

	dev->umon = udev_monitor_new_from_netlink(dev->udev, "udev");
	if (!dev->umon)
		return -ENOMEM;

	ret = udev_monitor_filter_add_match_subsystem_devtype(dev->umon,
							      "input", NULL);
	if (ret) {
		ret = -errno;
		goto err_mon;
	}

	ret = udev_monitor_filter_add_match_subsystem_devtype(dev->umon,
							      "hid", NULL);
	if (ret) {
		ret = -errno;
		goto err_mon;
	}

	ret = udev_monitor_enable_receiving(dev->umon);
	if (ret) {
		ret = -errno;
		goto err_mon;
	}

	fd = udev_monitor_get_fd(dev->umon);

	set = fcntl(fd, F_GETFL);
	if (set < 0) {
		ret = -errno;
		goto err_mon;
	}

	set |= O_NONBLOCK;
	ret = fcntl(fd, F_SETFL, set);
	if (ret < 0) {
		ret = -errno;
		goto err_mon;
	}

	memset(&ep, 0, sizeof(ep));
	ep.events = EPOLLIN;
	ep.data.ptr = dev->umon;

	ret = epoll_ctl(dev->efd, EPOLL_CTL_ADD, fd, &ep);
	if (ret) {
		ret = -errno;
		goto err_mon;
	}

	return 0;

err_mon:
	udev_monitor_unref(dev->umon);
	dev->umon = NULL;
	return ret;
}

static int xwii_iface_open_if(struct xwii_iface *dev, unsigned int tif,
			      bool wr)
{
	char name[256];
	struct epoll_event ep;
	unsigned int flags;
	int fd, err;

	if (dev->ifs[tif].fd >= 0)
		return 0;
	if (!dev->ifs[tif].node)
		return -ENODEV;

	flags = O_NONBLOCK | O_CLOEXEC;
	flags |= wr ? O_RDWR : O_RDONLY;
	fd = open(dev->ifs[tif].node, flags);
	if (fd < 0)
		return -errno;

	if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
		close(fd);
		return -errno;
	}

	name[sizeof(name) - 1] = 0;
	if (strcmp(if_to_name_table[tif], name)) {
		close(fd);
		return -ENODEV;
	}

	memset(&ep, 0, sizeof(ep));
	ep.events = EPOLLIN;
	ep.data.ptr = &dev->ifs[tif];
	if (epoll_ctl(dev->efd, EPOLL_CTL_ADD, fd, &ep) < 0) {
		err = -errno;
		close(fd);
		return err;
	}

	dev->ifs[tif].fd = fd;
	return 0;
}

/*
 * Upload the generic rumble event to the device. This may later be used for
 * force-feedback effects. The event id is safed for later use.
 */
static void xwii_iface_upload_rumble(struct xwii_iface *dev, int fd)
{
	struct ff_effect effect = {
		.type = FF_RUMBLE,
		.id = -1,
		.u.rumble.strong_magnitude = 1,
		.replay.length = 0,
		.replay.delay = 0,
	};

	if (ioctl(fd, EVIOCSFF, &effect) != -1) {
		dev->rumble_id = effect.id;
		dev->rumble_fd = fd;
	}
}

XWII__EXPORT
int xwii_iface_open(struct xwii_iface *dev, unsigned int ifaces)
{
	bool wr;
	int ret, err;

	if (!dev)
		return -EINVAL;

	wr = ifaces & XWII_IFACE_WRITABLE;
	ifaces &= XWII_IFACE_ALL;
	ifaces &= ~dev->ifaces;
	if (!ifaces)
		return 0;

	err = 0;
	if (ifaces & XWII_IFACE_CORE) {
		ret = xwii_iface_open_if(dev, XWII_IF_CORE, wr);
		if (!ret) {
			dev->ifaces |= XWII_IFACE_CORE;
			xwii_iface_upload_rumble(dev,
						 dev->ifs[XWII_IF_CORE].fd);
		} else {
			err = ret;
		}
	}

	if (ifaces & XWII_IFACE_ACCEL) {
		ret = xwii_iface_open_if(dev, XWII_IF_ACCEL, wr);
		if (!ret)
			dev->ifaces |= XWII_IFACE_ACCEL;
		else
			err = ret;
	}

	if (ifaces & XWII_IFACE_IR) {
		ret = xwii_iface_open_if(dev, XWII_IF_IR, wr);
		if (!ret)
			dev->ifaces |= XWII_IFACE_IR;
		else
			err = ret;
	}

	if (ifaces & XWII_IFACE_MOTION_PLUS) {
		ret = xwii_iface_open_if(dev, XWII_IF_MOTION_PLUS, wr);
		if (!ret)
			dev->ifaces |= XWII_IFACE_MOTION_PLUS;
		else
			err = ret;
	}

	if (ifaces & XWII_IFACE_NUNCHUK) {
		ret = xwii_iface_open_if(dev, XWII_IF_NUNCHUK, wr);
		if (!ret)
			dev->ifaces |= XWII_IFACE_NUNCHUK;
		else
			err = ret;
	}

	if (ifaces & XWII_IFACE_CLASSIC_CONTROLLER) {
		ret = xwii_iface_open_if(dev, XWII_IF_CLASSIC_CONTROLLER, wr);
		if (!ret)
			dev->ifaces |= XWII_IFACE_CLASSIC_CONTROLLER;
		else
			err = ret;
	}

	if (ifaces & XWII_IFACE_BALANCE_BOARD) {
		ret = xwii_iface_open_if(dev, XWII_IF_BALANCE_BOARD, wr);
		if (!ret)
			dev->ifaces |= XWII_IFACE_BALANCE_BOARD;
		else
			err = ret;
	}

	if (ifaces & XWII_IFACE_PRO_CONTROLLER) {
		ret = xwii_iface_open_if(dev, XWII_IF_PRO_CONTROLLER, wr);
		if (!ret) {
			dev->ifaces |= XWII_IFACE_PRO_CONTROLLER;
			xwii_iface_upload_rumble(dev,
						 dev->ifs[XWII_IF_PRO_CONTROLLER].fd);
		} else {
			err = ret;
		}
	}

	if (ifaces & XWII_IFACE_DRUMS) {
		ret = xwii_iface_open_if(dev, XWII_IF_DRUMS, wr);
		if (!ret)
			dev->ifaces |= XWII_IFACE_DRUMS;
		else
			err = ret;
	}

	if (ifaces & XWII_IFACE_GUITAR) {
		ret = xwii_iface_open_if(dev, XWII_IF_GUITAR, wr);
		if (!ret)
			dev->ifaces |= XWII_IFACE_GUITAR;
		else
			err = ret;
	}

	return err;
}

static void xwii_iface_close_if(struct xwii_iface *dev, unsigned int tif)
{
	if (dev->ifs[tif].fd < 0)
		return;

	epoll_ctl(dev->efd, EPOLL_CTL_DEL, dev->ifs[tif].fd, NULL);
	close(dev->ifs[tif].fd);
	dev->ifs[tif].fd = -1;
}

XWII__EXPORT
void xwii_iface_close(struct xwii_iface *dev, unsigned int ifaces)
{
	if (!dev)
		return;

	ifaces &= XWII_IFACE_ALL;
	if (!ifaces)
		return;

	if (ifaces & XWII_IFACE_CORE) {
		if (dev->rumble_fd == dev->ifs[XWII_IF_CORE].fd) {
			dev->rumble_id = -1;
			dev->rumble_fd = -1;
		}
		xwii_iface_close_if(dev, XWII_IF_CORE);
	}
	if (ifaces & XWII_IFACE_ACCEL)
		xwii_iface_close_if(dev, XWII_IF_ACCEL);
	if (ifaces & XWII_IFACE_IR)
		xwii_iface_close_if(dev, XWII_IF_IR);
	if (ifaces & XWII_IFACE_MOTION_PLUS)
		xwii_iface_close_if(dev, XWII_IF_MOTION_PLUS);
	if (ifaces & XWII_IFACE_NUNCHUK)
		xwii_iface_close_if(dev, XWII_IF_NUNCHUK);
	if (ifaces & XWII_IFACE_CLASSIC_CONTROLLER)
		xwii_iface_close_if(dev, XWII_IF_CLASSIC_CONTROLLER);
	if (ifaces & XWII_IFACE_BALANCE_BOARD)
		xwii_iface_close_if(dev, XWII_IF_BALANCE_BOARD);
	if (ifaces & XWII_IFACE_PRO_CONTROLLER) {
		if (dev->rumble_fd == dev->ifs[XWII_IF_PRO_CONTROLLER].fd) {
			dev->rumble_id = -1;
			dev->rumble_fd = -1;
		}
		xwii_iface_close_if(dev, XWII_IF_PRO_CONTROLLER);
	}
	if (ifaces & XWII_IFACE_DRUMS)
		xwii_iface_close_if(dev, XWII_IF_DRUMS);
	if (ifaces & XWII_IFACE_GUITAR)
		xwii_iface_close_if(dev, XWII_IF_GUITAR);

	dev->ifaces &= ~ifaces;
}

XWII__EXPORT
unsigned int xwii_iface_opened(struct xwii_iface *dev)
{
	if (!dev)
		return 0;

	return dev->ifaces;
}

XWII__EXPORT
unsigned int xwii_iface_available(struct xwii_iface *dev)
{
	unsigned int ifs = 0, i;

	if (!dev)
		return 0;

	for (i = 0; i < XWII_IF_NUM; ++i)
		ifs |= dev->ifs[i].node ? if_to_iface(i) : 0;

	return ifs;
}

static int read_umon(struct xwii_iface *dev, struct epoll_event *ep,
		     struct xwii_event *ev)
{
	struct udev_device *ndev, *p;
	const char *act, *path, *npath, *ppath, *node;
	bool hotplug, remove;

	if (ep->events & EPOLLIN) {
		hotplug = false;
		remove = false;
		path = udev_device_get_syspath(dev->dev);

		/* try to merge as many hotplug events as possible */
		while (true) {
			ndev = udev_monitor_receive_device(dev->umon);
			if (!ndev)
				break;

			/* We are interested in three kinds of events:
			 *  1) "change" events on the main HID device notify
			 *     us of device-detection events.
			 *  1) "remove" events on the main HID device notify
			 *     us of device-removal.
			 *  3) "add"/"remove" events on input events (not
			 *     the evdev events with "devnode") notify us
			 *     of extension changes. */

			act = udev_device_get_action(ndev);
			npath = udev_device_get_syspath(ndev);
			node = udev_device_get_devnode(ndev);
			p = udev_device_get_parent_with_subsystem_devtype(ndev,
								"hid", NULL);
			if (p)
				ppath = udev_device_get_syspath(p);

			if (act && !strcmp(act, "change") &&
			    !strcmp(path, npath))
				hotplug = true;
			else if (act && !strcmp(act, "remove") &&
				 !strcmp(path, npath))
				remove = true;
			else if (!node && p && !strcmp(ppath, path))
				hotplug = true;

			udev_device_unref(ndev);
		}

		/* notify caller of removals via special event */
		if (remove) {
			memset(ev, 0, sizeof(*ev));
			ev->type = XWII_EVENT_GONE;
			xwii_iface_read_nodes(dev);
			return 0;
		}

		/* notify caller via generic hotplug event */
		if (hotplug) {
			memset(ev, 0, sizeof(*ev));
			ev->type = XWII_EVENT_WATCH;
			xwii_iface_read_nodes(dev);
			return 0;
		}
	}

	if (ep->events & (EPOLLHUP | EPOLLERR))
		return -EPIPE;

	return -EAGAIN;
}

static int read_event(int fd, struct input_event *ev)
{
	int ret;

	ret = read(fd, ev, sizeof(*ev));
	if (ret < 0)
		return -errno;
	else if (ret == 0)
		return -EAGAIN;
	else if (ret != sizeof(*ev))
		return -EIO;
	else
		return 0;
}

static int read_core(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;
	unsigned int key;

	fd = dev->ifs[XWII_IF_CORE].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_CORE);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type != EV_KEY)
		goto try_again;

	if (input.value < 0 || input.value > 2)
		goto try_again;

	switch (input.code) {
		case KEY_LEFT:
			key = XWII_KEY_LEFT;
			break;
		case KEY_RIGHT:
			key = XWII_KEY_RIGHT;
			break;
		case KEY_UP:
			key = XWII_KEY_UP;
			break;
		case KEY_DOWN:
			key = XWII_KEY_DOWN;
			break;
		case KEY_NEXT:
			key = XWII_KEY_PLUS;
			break;
		case KEY_PREVIOUS:
			key = XWII_KEY_MINUS;
			break;
		case BTN_1:
			key = XWII_KEY_ONE;
			break;
		case BTN_2:
			key = XWII_KEY_TWO;
			break;
		case BTN_A:
			key = XWII_KEY_A;
			break;
		case BTN_B:
			key = XWII_KEY_B;
			break;
		case BTN_MODE:
			key = XWII_KEY_HOME;
			break;
		default:
			goto try_again;
	}

	memset(ev, 0, sizeof(*ev));
	memcpy(&ev->time, &input.time, sizeof(struct timeval));
	ev->type = XWII_EVENT_KEY;
	ev->v.key.code = key;
	ev->v.key.state = input.value;
	return 0;
}

static int read_accel(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;

	fd = dev->ifs[XWII_IF_ACCEL].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_ACCEL);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		memcpy(ev->v.abs, &dev->accel_cache, sizeof(dev->accel_cache));
		ev->type = XWII_EVENT_ACCEL;
		return 0;
	}

	if (input.type != EV_ABS)
		goto try_again;

	if (input.code == ABS_RX)
		dev->accel_cache.x = input.value;
	else if (input.code == ABS_RY)
		dev->accel_cache.y = input.value;
	else if (input.code == ABS_RZ)
		dev->accel_cache.z = input.value;

	goto try_again;
}

static int read_ir(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;

	fd = dev->ifs[XWII_IF_IR].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_IR);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		memcpy(&ev->v.abs, dev->ir_cache, sizeof(dev->ir_cache));
		ev->type = XWII_EVENT_IR;
		return 0;
	}

	if (input.type != EV_ABS)
		goto try_again;

	if (input.code == ABS_HAT0X)
		dev->ir_cache[0].x = input.value;
	else if (input.code == ABS_HAT0Y)
		dev->ir_cache[0].y = input.value;
	else if (input.code == ABS_HAT1X)
		dev->ir_cache[1].x = input.value;
	else if (input.code == ABS_HAT1Y)
		dev->ir_cache[1].y = input.value;
	else if (input.code == ABS_HAT2X)
		dev->ir_cache[2].x = input.value;
	else if (input.code == ABS_HAT2Y)
		dev->ir_cache[2].y = input.value;
	else if (input.code == ABS_HAT3X)
		dev->ir_cache[3].x = input.value;
	else if (input.code == ABS_HAT3Y)
		dev->ir_cache[3].y = input.value;

	goto try_again;
}

static int read_mp(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;

	fd = dev->ifs[XWII_IF_MOTION_PLUS].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_MOTION_PLUS);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));

		ev->v.abs[0].x = dev->mp_cache.x - dev->mp_normalizer.x / 100;
		ev->v.abs[0].y = dev->mp_cache.y - dev->mp_normalizer.y / 100;
		ev->v.abs[0].z = dev->mp_cache.z - dev->mp_normalizer.z / 100;
		dev->mp_normalizer.x += dev->mp_normalize_factor *
					((ev->v.abs[0].x > 0) ? 1 : -1);
		dev->mp_normalizer.y += dev->mp_normalize_factor *
					((ev->v.abs[0].y > 0) ? 1 : -1);
		dev->mp_normalizer.z += dev->mp_normalize_factor *
					((ev->v.abs[0].z > 0) ? 1 : -1);

		ev->type = XWII_EVENT_MOTION_PLUS;
		return 0;
	}

	if (input.type != EV_ABS)
		goto try_again;

	if (input.code == ABS_RX)
		dev->mp_cache.x = input.value;
	else if (input.code == ABS_RY)
		dev->mp_cache.y = input.value;
	else if (input.code == ABS_RZ)
		dev->mp_cache.z = input.value;

	goto try_again;
}

static int read_nunchuk(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;
	unsigned int key;

	fd = dev->ifs[XWII_IF_NUNCHUK].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_NUNCHUK);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_KEY) {
		if (input.value < 0 || input.value > 2)
			goto try_again;

		switch (input.code) {
			case BTN_C:
				key = XWII_KEY_C;
				break;
			case BTN_Z:
				key = XWII_KEY_Z;
				break;
			default:
				goto try_again;
		}

		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		ev->type = XWII_EVENT_NUNCHUK_KEY;
		ev->v.key.code = key;
		ev->v.key.state = input.value;
		return 0;
	} else if (input.type == EV_ABS) {
		if (input.code == ABS_HAT0X)
			dev->nunchuk_cache[0].x = input.value;
		else if (input.code == ABS_HAT0Y)
			dev->nunchuk_cache[0].y = input.value;
		else if (input.code == ABS_RX)
			dev->nunchuk_cache[1].x = input.value;
		else if (input.code == ABS_RY)
			dev->nunchuk_cache[1].y = input.value;
		else if (input.code == ABS_RZ)
			dev->nunchuk_cache[1].z = input.value;
	} else if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		memcpy(&ev->v.abs, dev->nunchuk_cache,
		       sizeof(dev->nunchuk_cache));
		ev->type = XWII_EVENT_NUNCHUK_MOVE;
		return 0;
	} else {
	}

	goto try_again;
}

static int read_classic(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;
	unsigned int key;

	fd = dev->ifs[XWII_IF_CLASSIC_CONTROLLER].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_CLASSIC_CONTROLLER);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_KEY) {
		if (input.value < 0 || input.value > 2)
			goto try_again;

		switch (input.code) {
			case BTN_A:
				key = XWII_KEY_A;
				break;
			case BTN_B:
				key = XWII_KEY_B;
				break;
			case BTN_X:
				key = XWII_KEY_X;
				break;
			case BTN_Y:
				key = XWII_KEY_Y;
				break;
			case KEY_NEXT:
				key = XWII_KEY_PLUS;
				break;
			case KEY_PREVIOUS:
				key = XWII_KEY_MINUS;
				break;
			case BTN_MODE:
				key = XWII_KEY_HOME;
				break;
			case KEY_LEFT:
				key = XWII_KEY_LEFT;
				break;
			case KEY_RIGHT:
				key = XWII_KEY_RIGHT;
				break;
			case KEY_UP:
				key = XWII_KEY_UP;
				break;
			case KEY_DOWN:
				key = XWII_KEY_DOWN;
				break;
			case BTN_TL:
				key = XWII_KEY_TL;
				break;
			case BTN_TR:
				key = XWII_KEY_TR;
				break;
			case BTN_TL2:
				key = XWII_KEY_ZL;
				break;
			case BTN_TR2:
				key = XWII_KEY_ZR;
				break;
			default:
				goto try_again;
		}

		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		ev->type = XWII_EVENT_CLASSIC_CONTROLLER_KEY;
		ev->v.key.code = key;
		ev->v.key.state = input.value;
		return 0;
	} else if (input.type == EV_ABS) {
		if (input.code == ABS_HAT1X)
			dev->classic_cache[0].x = input.value;
		else if (input.code == ABS_HAT1Y)
			dev->classic_cache[0].y = input.value;
		else if (input.code == ABS_HAT2X)
			dev->classic_cache[1].x = input.value;
		else if (input.code == ABS_HAT2Y)
			dev->classic_cache[1].y = input.value;
		else if (input.code == ABS_HAT3X)
			dev->classic_cache[2].y = input.value;
		else if (input.code == ABS_HAT3Y)
			dev->classic_cache[2].x = input.value;
	} else if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		memcpy(&ev->v.abs, dev->classic_cache,
		       sizeof(dev->classic_cache));
		ev->type = XWII_EVENT_CLASSIC_CONTROLLER_MOVE;
		return 0;
	} else {
	}

	goto try_again;
}

static int read_bboard(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;

	fd = dev->ifs[XWII_IF_BALANCE_BOARD].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_BALANCE_BOARD);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		memcpy(&ev->v.abs, dev->bboard_cache,
		       sizeof(dev->bboard_cache));
		ev->type = XWII_EVENT_BALANCE_BOARD;
		return 0;
	}

	if (input.type != EV_ABS)
		goto try_again;

	if (input.code == ABS_HAT0X)
		dev->bboard_cache[0].x = input.value;
	else if (input.code == ABS_HAT0Y)
		dev->bboard_cache[1].x = input.value;
	else if (input.code == ABS_HAT1X)
		dev->bboard_cache[2].x = input.value;
	else if (input.code == ABS_HAT1Y)
		dev->bboard_cache[3].x = input.value;

	goto try_again;
}

static int read_pro(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;
	unsigned int key;

	fd = dev->ifs[XWII_IF_PRO_CONTROLLER].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_PRO_CONTROLLER);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_KEY) {
		if (input.value < 0 || input.value > 2)
			goto try_again;

		switch (input.code) {
#ifndef BTN_EAST
#define BTN_EAST 0x131
#endif
			case BTN_EAST:
				key = XWII_KEY_A;
				break;
#ifndef BTN_SOUTH
#define BTN_SOUTH 0x130
#endif
			case BTN_SOUTH:
				key = XWII_KEY_B;
				break;
#ifndef BTN_NORTH
#define BTN_NORTH 0x133
#endif
			case BTN_NORTH:
				key = XWII_KEY_X;
				break;
#ifndef BTN_WEST
#define BTN_WEST 0x134
#endif
			case BTN_WEST:
				key = XWII_KEY_Y;
				break;
			case BTN_START:
				key = XWII_KEY_PLUS;
				break;
			case BTN_SELECT:
				key = XWII_KEY_MINUS;
				break;
			case BTN_MODE:
				key = XWII_KEY_HOME;
				break;
#ifndef BTN_DPAD_LEFT
#define BTN_DPAD_LEFT 0x222
#endif
			case BTN_DPAD_LEFT:
				key = XWII_KEY_LEFT;
				break;
#ifndef BTN_DPAD_RIGHT
#define BTN_DPAD_RIGHT 0x223
#endif
			case BTN_DPAD_RIGHT:
				key = XWII_KEY_RIGHT;
				break;
#ifndef BTN_DPAD_UP
#define BTN_DPAD_UP 0x220
#endif
			case BTN_DPAD_UP:
				key = XWII_KEY_UP;
				break;
#ifndef BTN_DPAD_DOWN
#define BTN_DPAD_DOWN 0x221
#endif
			case BTN_DPAD_DOWN:
				key = XWII_KEY_DOWN;
				break;
			case BTN_TL:
				key = XWII_KEY_TL;
				break;
			case BTN_TR:
				key = XWII_KEY_TR;
				break;
			case BTN_TL2:
				key = XWII_KEY_ZL;
				break;
			case BTN_TR2:
				key = XWII_KEY_ZR;
				break;
			case BTN_THUMBL:
				key = XWII_KEY_THUMBL;
				break;
			case BTN_THUMBR:
				key = XWII_KEY_THUMBR;
				break;
			default:
				goto try_again;
		}

		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		ev->type = XWII_EVENT_PRO_CONTROLLER_KEY;
		ev->v.key.code = key;
		ev->v.key.state = input.value;
		return 0;
	} else if (input.type == EV_ABS) {
		if (input.code == ABS_X)
			dev->pro_cache[0].x = input.value;
		else if (input.code == ABS_Y)
			dev->pro_cache[0].y = input.value;
		else if (input.code == ABS_RX)
			dev->pro_cache[1].x = input.value;
		else if (input.code == ABS_RY)
			dev->pro_cache[1].y = input.value;
	} else if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		memcpy(&ev->v.abs, dev->pro_cache,
		       sizeof(dev->pro_cache));
		ev->type = XWII_EVENT_PRO_CONTROLLER_MOVE;
		return 0;
	} else {
	}

	goto try_again;
}

static int read_drums(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;
	unsigned int key;

	fd = dev->ifs[XWII_IF_DRUMS].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_DRUMS);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_KEY) {
		if (input.value < 0 || input.value > 2)
			goto try_again;

		switch (input.code) {
		case BTN_START:
			key = XWII_KEY_PLUS;
			break;
		case BTN_SELECT:
			key = XWII_KEY_MINUS;
			break;
		default:
			goto try_again;
		}

		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		ev->type = XWII_EVENT_DRUMS_KEY;
		ev->v.key.code = key;
		ev->v.key.state = input.value;
		return 0;
	} else if (input.type == EV_ABS) {
		if (input.code == ABS_X)
			dev->drums_cache[XWII_DRUMS_ABS_PAD].x = input.value;
		else if (input.code == ABS_Y)
			dev->drums_cache[XWII_DRUMS_ABS_PAD].y = input.value;
#ifndef ABS_CYMBAL_LEFT
#define ABS_CYMBAL_LEFT 0x45
#endif
		else if (input.code == ABS_CYMBAL_LEFT)
			dev->drums_cache[XWII_DRUMS_ABS_CYMBAL_LEFT].x = input.value;
#ifndef ABS_CYMBAL_RIGHT
#define ABS_CYMBAL_RIGHT 0x46
#endif
		else if (input.code == ABS_CYMBAL_RIGHT)
			dev->drums_cache[XWII_DRUMS_ABS_CYMBAL_RIGHT].x = input.value;
#ifndef ABS_TOM_LEFT
#define ABS_TOM_LEFT 0x41
#endif
		else if (input.code == ABS_TOM_LEFT)
			dev->drums_cache[XWII_DRUMS_ABS_TOM_LEFT].x = input.value;
#ifndef ABS_TOM_RIGHT
#define ABS_TOM_RIGHT 0x42
#endif
		else if (input.code == ABS_TOM_RIGHT)
			dev->drums_cache[XWII_DRUMS_ABS_TOM_RIGHT].x = input.value;
#ifndef ABS_TOM_FAR_RIGHT
#define ABS_TOM_FAR_RIGHT 0x43
#endif
		else if (input.code == ABS_TOM_FAR_RIGHT)
			dev->drums_cache[XWII_DRUMS_ABS_TOM_FAR_RIGHT].x = input.value;
#ifndef ABS_BASS
#define ABS_BASS 0x48
#endif
		else if (input.code == ABS_BASS)
			dev->drums_cache[XWII_DRUMS_ABS_BASS].x = input.value;
#ifndef ABS_HI_HAT
#define ABS_HI_HAT 0x49
#endif
		else if (input.code == ABS_HI_HAT)
			dev->drums_cache[XWII_DRUMS_ABS_HI_HAT].x = input.value;
	} else if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		memcpy(&ev->v.abs, dev->drums_cache,
		       sizeof(dev->drums_cache));
		ev->type = XWII_EVENT_DRUMS_MOVE;
		return 0;
	}

	goto try_again;
}

static int read_guitar(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret, fd;
	struct input_event input;
	unsigned int key;

	fd = dev->ifs[XWII_IF_GUITAR].fd;
	if (fd < 0)
		return -EAGAIN;

try_again:
	ret = read_event(fd, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret < 0) {
		xwii_iface_close(dev, XWII_IFACE_GUITAR);
		memset(ev, 0, sizeof(*ev));
		ev->type = XWII_EVENT_WATCH;
		xwii_iface_read_nodes(dev);
		return 0;
	}

	if (input.type == EV_KEY) {
		if (input.value < 0 || input.value > 2)
			goto try_again;

		switch (input.code) {
#ifndef BTN_FRET_FAR_UP
#define BTN_FRET_FAR_UP 0x224
#endif
		case BTN_FRET_FAR_UP:
			key = XWII_KEY_FRET_FAR_UP;
			break;
#ifndef BTN_FRET_UP
#define BTN_FRET_UP 0x225
#endif
		case BTN_FRET_UP:
			key = XWII_KEY_FRET_UP;
			break;
#ifndef BTN_FRET_MID
#define BTN_FRET_MID 0x226
#endif
		case BTN_FRET_MID:
			key = XWII_KEY_FRET_MID;
			break;
#ifndef BTN_FRET_LOW
#define BTN_FRET_LOW 0x227
#endif
		case BTN_FRET_LOW:
			key = XWII_KEY_FRET_LOW;
			break;
#ifndef BTN_FRET_FAR_LOW
#define BTN_FRET_FAR_LOW 0x228
#endif
		case BTN_FRET_FAR_LOW:
			key = XWII_KEY_FRET_FAR_LOW;
			break;
#ifndef BTN_STRUM_BAR_UP
#define BTN_STRUM_BAR_UP 0x229
#endif
		case BTN_STRUM_BAR_UP:
			key = XWII_KEY_STRUM_BAR_UP;
			break;
#ifndef BTN_STRUM_BAR_DOWN
#define BTN_STRUM_BAR_DOWN 0x22a
#endif
		case BTN_STRUM_BAR_DOWN:
			key = XWII_KEY_STRUM_BAR_DOWN;
			break;
		case BTN_START:
			key = XWII_KEY_PLUS;
			break;
		case BTN_MODE:
			key = XWII_KEY_HOME;
			break;
		default:
			goto try_again;
		}

		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		ev->type = XWII_EVENT_GUITAR_KEY;
		ev->v.key.code = key;
		ev->v.key.state = input.value;
		return 0;
	} else if (input.type == EV_ABS) {
		if (input.code == ABS_X)
			dev->guitar_cache[0].x = input.value;
		else if (input.code == ABS_Y)
			dev->guitar_cache[0].y = input.value;
#ifndef ABS_WHAMMY_BAR
#define ABS_WHAMMY_BAR 0x4b
#endif
		else if (input.code == ABS_WHAMMY_BAR)
			dev->guitar_cache[1].x = input.value;
#ifndef ABS_FRET_BOARD
#define ABS_FRET_BOARD 0x4a
#endif
		else if (input.code == ABS_FRET_BOARD)
			dev->guitar_cache[2].x = input.value;
	} else if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
		memcpy(&ev->time, &input.time, sizeof(struct timeval));
		memcpy(&ev->v.abs, dev->guitar_cache,
		       sizeof(dev->guitar_cache));
		ev->type = XWII_EVENT_GUITAR_MOVE;
		return 0;
	}

	goto try_again;
}

static int dispatch_event(struct xwii_iface *dev, struct epoll_event *ep,
			  struct xwii_event *ev)
{
	if (dev->umon && ep->data.ptr == dev->umon)
		return read_umon(dev, ep, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_CORE])
		return read_core(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_ACCEL])
		return read_accel(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_IR])
		return read_ir(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_MOTION_PLUS])
		return read_mp(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_NUNCHUK])
		return read_nunchuk(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_CLASSIC_CONTROLLER])
		return read_classic(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_BALANCE_BOARD])
		return read_bboard(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_PRO_CONTROLLER])
		return read_pro(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_DRUMS])
		return read_drums(dev, ev);
	else if (ep->data.ptr == &dev->ifs[XWII_IF_GUITAR])
		return read_guitar(dev, ev);

	return -EAGAIN;
}

/*
 * Poll for events on device \dev.
 *
 * This function always performs any outstanding I/O. If this fails, an error
 * is returned.
 *
 * If @ev is NULL, nothing else is done and 0 is returned.
 *
 * If @ev is non-NULL, this function reads incoming events from the kernel. If
 * no event is available, -EAGAIN is returned. Otherwise, 0 is returned and a
 * single event is stored in @ev. You should call this function in a row until
 * it returns -EAGAIN to get all events.
 *
 * Once this function returns -EAGAIN, you must watch the descriptor returned
 * by xwii_iface_get_fd() for POLLIN/EPOLLIN/read-events. No other events
 * need to be watched for.
 * Once this fd is readable, you should call xwii_iface_poll() again.
 *
 * If an interface gets closed or some hotplug event is detected, this
 * function returns XWII_EVENT_WATCH. This event does not provide any payload
 * and you need to re-open any interfaces if they got closed.
 */
XWII__EXPORT
int xwii_iface_poll(struct xwii_iface *dev, struct xwii_event *ev)
{
	struct epoll_event ep[32];
	int ret, i;
	size_t siz;

	if (!dev)
		return -EFAULT;

	/* write outgoing events here */

	if (!ev)
		return 0;

	siz = sizeof(ep) / sizeof(*ep);
	ret = epoll_wait(dev->efd, ep, siz, 0);
	if (ret < 0)
		return -errno;
	if (ret > siz)
		ret = siz;

	for (i = 0; i < ret; ++i) {
		ret = dispatch_event(dev, &ep[i], ev);
		if (ret != -EAGAIN)
			return ret;
	}

	return -EAGAIN;
}

XWII__EXPORT
int xwii_iface_dispatch(struct xwii_iface *dev, struct xwii_event *u_ev,
			size_t size)
{
	struct epoll_event ep[32];
	int ret, i;
	size_t siz;
	struct xwii_event ev;

	if (!dev)
		return -EFAULT;

	/* write outgoing events here */

	if (!u_ev || size <= 0)
		return 0;
	if (size > sizeof(ev))
		size = sizeof(ev);

	siz = sizeof(ep) / sizeof(*ep);
	ret = epoll_wait(dev->efd, ep, siz, 0);
	if (ret < 0)
		return -errno;
	if (ret > siz)
		ret = siz;

	for (i = 0; i < ret; ++i) {
		ret = dispatch_event(dev, &ep[i], &ev);
		if (ret != -EAGAIN) {
			if (!ret)
				memcpy(u_ev, &ev, size);
			return ret;
		}
	}

	return -EAGAIN;
}

/*
 * Toogle wiimote rumble motor
 * Enable or disable the rumble motor of \dev depending on \on. This requires
 * the core interface to be opened.
 */
XWII__EXPORT
int xwii_iface_rumble(struct xwii_iface *dev, bool on)
{
	struct input_event ev;
	int ret;

	if (!dev)
		return -EINVAL;
	if (dev->rumble_fd < 0 || dev->rumble_id < 0)
		return -ENODEV;

	ev.type = EV_FF;
	ev.code = dev->rumble_id;
	ev.value = on;
	ret = write(dev->rumble_fd, &ev, sizeof(ev));

	if (ret == -1)
		return -errno;
	else
		return 0;
}

static int read_line(const char *path, char **out)
{
	FILE *f;
	char buf[4096], *line;

	f = fopen(path, "re");
	if (!f)
		return -errno;

	if (!fgets(buf, sizeof(buf), f)) {
		if (ferror(f)) {
			fclose(f);
			return errno ? -errno : -EINVAL;
		}
		buf[0] = 0;
	}

	fclose(f);

	line = strdup(buf);
	if (!line)
		return -ENOMEM;
	line[strcspn(line, "\n")] = 0;

	*out = line;
	return 0;
}

static int write_string(const char *path, const char *line)
{
	FILE *f;

	f = fopen(path, "we");
	if (!f)
		return -errno;

	fputs(line, f);
	fflush(f);

	if (ferror(f)) {
		fclose(f);
		return errno ? -errno : -EINVAL;
	}

	fclose(f);
	return 0;
}

static int read_led(const char *path, bool *state)
{
	int ret;
	char *line;

	ret = read_line(path, &line);
	if (ret)
		return ret;

	*state = !!atoi(line);
	free(line);

	return 0;
}

XWII__EXPORT
int xwii_iface_get_led(struct xwii_iface *dev, unsigned int led, bool *state)
{
	if (led > XWII_LED4 || led < XWII_LED1)
		return -EINVAL;
	if (!dev || !state)
		return -EINVAL;

	--led;
	if (!dev->led_attrs[led])
		return -ENODEV;

	return read_led(dev->led_attrs[led], state);
}

XWII__EXPORT
int xwii_iface_set_led(struct xwii_iface *dev, unsigned int led, bool state)
{
	if (!dev || led > XWII_LED4 || led < XWII_LED1)
		return -EINVAL;

	--led;
	if (!dev->led_attrs[led])
		return -ENODEV;

	return write_string(dev->led_attrs[led], state ? "1\n" : "0\n");
}

static int read_battery(const char *path, uint8_t *capacity)
{
	int ret;
	char *line;

	ret = read_line(path, &line);
	if (ret)
		return ret;

	*capacity = atoi(line);
	free(line);

	return 0;
}

XWII__EXPORT
int xwii_iface_get_battery(struct xwii_iface *dev, uint8_t *capacity)
{
	if (!dev || !capacity)
		return -EINVAL;
	if (!dev->battery_attr)
		return -ENODEV;

	return read_battery(dev->battery_attr, capacity);
}

XWII__EXPORT
int xwii_iface_get_devtype(struct xwii_iface *dev, char **devtype)
{
	if (!dev || !devtype)
		return -EINVAL;
	if (!dev->devtype_attr)
		return -ENODEV;

	return read_line(dev->devtype_attr, devtype);
}

XWII__EXPORT
int xwii_iface_get_extension(struct xwii_iface *dev, char **extension)
{
	if (!dev || !extension)
		return -EINVAL;
	if (!dev->extension_attr)
		return -ENODEV;

	return read_line(dev->extension_attr, extension);
}

XWII__EXPORT
void xwii_iface_set_mp_normalization(struct xwii_iface *dev, int32_t x,
				     int32_t y, int32_t z, int32_t factor)
{
	if (!dev)
		return;

	dev->mp_normalizer.x = x * 100;
	dev->mp_normalizer.y = y * 100;
	dev->mp_normalizer.z = z * 100;
	dev->mp_normalize_factor = factor;
}

XWII__EXPORT
void xwii_iface_get_mp_normalization(struct xwii_iface *dev, int32_t *x,
				     int32_t *y, int32_t *z, int32_t *factor)
{
	if (x)
		*x = dev ? dev->mp_normalizer.x / 100 : 0;
	if (y)
		*y = dev ? dev->mp_normalizer.y / 100 : 0;
	if (z)
		*z = dev ? dev->mp_normalizer.z / 100 : 0;
	if (factor)
		*factor = dev ? dev->mp_normalize_factor : 0;
}
