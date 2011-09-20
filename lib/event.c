/*
 * XWiimote - lib
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "libsfs.h"
#include "xwiimote.h"

struct xwii_iface {
	size_t ref;
	char *syspath;

	unsigned int ifaces;
	int if_core;
	int if_accel;
	int if_ir;

	struct xwii_event_abs accel_cache;
	struct xwii_event_abs ir_cache[4];
};

/*
 * Create new interface structure
 * This does not validate \syspath. \syspath is first opened when calling
 * xwii_iface_open(). It must point to the base-directory in sysfs of the
 * xwiimote HID device. This is most often:
 *	/sys/bus/hid/devices/<device>
 * On success 0 is returned and the new device is stored in \dev. On failure,
 * \dev is not touched and a negative error is returned.
 * Initial refcount is 1 so you need to call *_unref() to free the device.
 */
int xwii_iface_new(struct xwii_iface **dev, const char *syspath)
{
	struct xwii_iface *d;

	assert(dev);
	assert(syspath);

	d = malloc(sizeof(*d));
	if (!d)
		return -ENOMEM;

	memset(d, 0, sizeof(*d));
	d->if_core = -1;
	d->if_accel = -1;
	d->if_ir = -1;

	d->syspath = strdup(syspath);
	if (!d->syspath) {
		free(d);
		return -ENOMEM;
	}

	*dev = xwii_iface_ref(d);
	return 0;
}

/* increment refcount by 1; always returns \dev */
struct xwii_iface *xwii_iface_ref(struct xwii_iface *dev)
{
	assert(dev);
	dev->ref++;
	assert(dev->ref);
	return dev;
}

/* decrement refcount by 1; frees the device if refcount becomes 0 */
void xwii_iface_unref(struct xwii_iface *dev)
{
	if (!dev)
		return;

	assert(dev->ref);

	if (--dev->ref)
		return;

	xwii_iface_close(dev, XWII_IFACE_ALL);
	free(dev->syspath);
	free(dev);
}

/* maps interface ID to interface name */
static const char *id2name_map[] = {
	[XWII_IFACE_CORE] = XWII_NAME_CORE,
	[XWII_IFACE_ACCEL] = XWII_NAME_ACCEL,
	[XWII_IFACE_IR] = XWII_NAME_IR,
};

/* make fd nonblocking */
static int make_nblock(int fd)
{
	int set;

	set = fcntl(fd, F_GETFL);
	if (set < 0)
		return -errno;

	set |= O_NONBLOCK;

	if (fcntl(fd, F_SETFL, set))
		return -errno;

	return 0;
}

/*
 * Opens an event interface and checks whether it is of the right type.
 * \path: absolute path to event interface (eg., /dev/input/event5)
 * \type: type of event interface (eg., XWII_IFACE_CORE)
 * \wr: Open file as writable
 * Returns positive fd on success or negative error on error.
 */
static int open_ev(const char *path, unsigned int type, bool wr)
{
	uint16_t id[4];
	char name[256];
	int ret, fd;

	ret = open(path, (wr ? O_RDWR : O_RDONLY) | O_CLOEXEC, 0);
	if (ret < 0)
		return -abs(errno);
	fd = ret;

	if (ioctl(fd, EVIOCGID, id)) {
		close(fd);
		return -abs(errno);
	}

	if (id[ID_BUS] != XWII_ID_BUS || id[ID_VENDOR] != XWII_ID_VENDOR ||
					id[ID_PRODUCT] != XWII_ID_PRODUCT) {
		close(fd);
		return -EINVAL;
	}

	if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
		close(fd);
		return -abs(errno);
	}

	name[sizeof(name) - 1] = 0;
	if (strcmp(id2name_map[type], name)) {
		close(fd);
		return -EINVAL;
	}

	ret = make_nblock(fd);
	if (ret) {
		close(fd);
		return -abs(ret);
	}

	return 0;
}

/*
 * Search in \list for a matching entry of \iface.
 * This allocates a new string containig the path to the event device:
 *	/dev/input/eventX
 * It returns NULL if \list does not contain a matching entry or on memory
 * allocation failure.
 * You must free the returned buffer with free()!
 */
static char *find_event(struct sfs_input_dev *list, unsigned int iface)
{
	char *res;
	static const char *prefix = "/dev/input/";
	static const size_t plen = sizeof(prefix) - 1;
	size_t len, size;

	while (list) {
		if (list->name && !strcmp(list->name, id2name_map[iface]))
			break;
		list = list->next;
	}

	if (!list)
		return NULL;

	len = strlen(list->name);
	size = plen + len;
	res = malloc(size + 1);
	if (!res)
		return NULL;

	res[size] = 0;
	memcpy(res, prefix, plen);
	memcpy(&res[plen], list->name, len);
	return res;
}

/*
 * Open interface \iface.
 * Opens the iface always in readable mode and if \wr is true also in writable
 * mode. \list is used to find the related event interface.
 * Returns <0 on error. Otherwise returns the open FD.
 */
static int open_iface(struct xwii_iface *dev, unsigned int iface,
					bool wr, struct sfs_input_dev *list)
{
	char *path;
	int ret;

	path = find_event(list, iface);
	if (!path)
		return -ENOENT;

	ret = open_ev(path, iface, wr);
	free(path);

	if (ret < 0)
		return ret;

	dev->ifaces |= iface;
	return ret;
}

/*
 * Opens the interfaces that are specified by \ifaces.
 * If an interface is already opened, it is not touched. If \ifaces contains
 * XWII_IFACE_WRITABLE, then the interfaces are also opened for writing. But
 * if an interface is already opened, it is not reopened in write-mode so you
 * need to close them first.
 *
 * If one of the ifaces cannot be opened, a negative error code is returned.
 * Some of the interfaces may be opened successfully, though. So if you need to
 * know which interfaces failed, check xwii_iface_opened() after calling this.
 * This will give you a list of interfaces that were opened before one interface
 * failed. The order of opening is equal to the ascending order of the absolute
 * values of the iface-flags. XWII_IFACE_CORE first, then XWII_IFACE_ACCEL and
 * so on.
 * Returns 0 on success.
 */
int xwii_iface_open(struct xwii_iface *dev, unsigned int ifaces)
{
	bool wr;
	int ret = 0;
	struct sfs_input_dev *list;

	assert(dev);

	wr = ifaces & XWII_IFACE_WRITABLE;

	/* remove invalid and already opened ifaces */
	ifaces &= ~dev->ifaces;
	ifaces &= XWII_IFACE_ALL;

	if (!ifaces)
		return 0;

	/* retrieve event file names */
	ret = sfs_input_list(dev->syspath, &list);
	if (ret)
		return ret;

	if (ifaces & XWII_IFACE_CORE) {
		ret = open_iface(dev, XWII_IFACE_CORE, wr, list);
		if (ret < 0)
			goto err_sys;
		dev->if_core = ret;
	}
	if (ifaces & XWII_IFACE_ACCEL) {
		ret = open_iface(dev, XWII_IFACE_ACCEL, wr, list);
		if (ret < 0)
			goto err_sys;
		dev->if_accel = ret;
	}
	if (ifaces & XWII_IFACE_IR) {
		ret = open_iface(dev, XWII_IFACE_IR, wr, list);
		if (ret < 0)
			goto err_sys;
		dev->if_ir = ret;
	}

err_sys:
	sfs_input_unref(list);
	return ret;
}

/* closes the interfaces given in \ifaces */
void xwii_iface_close(struct xwii_iface *dev, unsigned int ifaces)
{
	assert(dev);

	ifaces &= XWII_IFACE_ALL;

	if (ifaces & XWII_IFACE_CORE && dev->if_core != -1) {
		close(dev->if_core);
		dev->if_core = -1;
	}
	if (ifaces & XWII_IFACE_ACCEL && dev->if_accel != -1) {
		close(dev->if_accel);
		dev->if_accel = -1;
	}
	if (ifaces & XWII_IFACE_IR && dev->if_ir != -1) {
		close(dev->if_ir);
		dev->if_ir = -1;
	}

	dev->ifaces &= ~ifaces;
}

/* returns bitmap of opened interfaces */
unsigned int xwii_iface_opened(struct xwii_iface *dev)
{
	return dev->ifaces;
}

static int read_event(int fd, struct input_event *ev)
{
	int ret;

	ret = read(fd, ev, sizeof(*ev));
	if (ret < 0)
		return -errno;
	else if (ret == 0)
		return 0;
	else if (ret != sizeof(*ev))
		return -EIO;
	else
		return 1;
}

static int read_core(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret;
	struct input_event input;
	unsigned int key;

	if (dev->if_core == -1)
		return -EAGAIN;

try_again:
	ret = read_event(dev->if_core, &input);
	if (ret < 0)
		return ret;
	if (ret == 0) {
		xwii_iface_close(dev, XWII_IFACE_CORE);
		return -EAGAIN;
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
	ev->type = XWII_EVENT_KEY;
	ev->v.key.code = key;
	ev->v.key.state = input.value;
	return 0;
}

static int read_accel(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret;
	struct input_event input;

	if (dev->if_accel == -1)
		return -EAGAIN;

try_again:
	ret = read_event(dev->if_accel, &input);
	if (ret < 0)
		return ret;
	if (ret == 0) {
		xwii_iface_close(dev, XWII_IFACE_ACCEL);
		return -EAGAIN;
	}

	if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
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
	int ret;
	struct input_event input;

	if (dev->if_ir == -1)
		return -EAGAIN;

try_again:
	ret = read_event(dev->if_ir, &input);
	if (ret < 0)
		return ret;
	if (ret == 0) {
		xwii_iface_close(dev, XWII_IFACE_IR);
		return -EAGAIN;
	}

	if (input.type == EV_SYN) {
		memset(ev, 0, sizeof(*ev));
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

/*
 * Read new event from any opened interface of \dev.
 * Returns -EAGAIN if no new event can be read.
 * Returns 0 on success and writes the new event into \ev.
 * Returns negative error on failure.
 * Currently, it is not possible to get notified when an event interface gets
 * closed other than checking xwii_iface_opened() after every call. This may be
 * changed in the future.
 */
int xwii_iface_read(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret;

	assert(dev);
	assert(ev);

	ret = read_core(dev, ev);
	if (ret != -EAGAIN)
		return ret;
	ret = read_accel(dev, ev);
	if (ret != -EAGAIN)
		return ret;
	ret = read_ir(dev, ev);
	if (ret != -EAGAIN)
		return ret;

	return -EAGAIN;
}
