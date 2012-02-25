/*
 * XWiimote - lib
 * Written 2010, 2011, 2012 by David Herrmann
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/input.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "internal.h"
#include "xwiimote.h"

struct xwii_iface {
	size_t ref;
	char *syspath;
	int efd;
	int rumble_id;

	unsigned int ifaces;
	int if_core;
	int if_accel;
	int if_ir;
	int if_mp;
	int if_ext;

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
 *
 * Internally we use multiple file-descriptors for all the event devices of each
 * Wii Remote. However, externally we want only one fd for the application so we
 * use epoll to multiplex between all the internal fds. Note that you can poll
 * on epoll-fds like any other fd so the user won't notice.
 */
int xwii_iface_new(struct xwii_iface **dev, const char *syspath)
{
	struct xwii_iface *d;
	int ret;

	if (!dev || !syspath)
		return -EINVAL;

	d = malloc(sizeof(*d));
	if (!d)
		return -ENOMEM;

	memset(d, 0, sizeof(*d));
	d->ref = 1;
	d->if_core = -1;
	d->if_accel = -1;
	d->if_ir = -1;
	d->if_mp = -1;
	d->if_ext = -1;
	d->rumble_id = -1;

	d->syspath = strdup(syspath);
	if (!d->syspath) {
		ret= -ENOMEM;
		goto err;
	}

	d->efd = epoll_create1(EPOLL_CLOEXEC);
	if (d->efd < 0) {
		ret = -EFAULT;
		goto err_path;
	}

	*dev = d;
	return 0;

err_path:
	free(d->syspath);
err:
	free(d);
	return ret;
}

void xwii_iface_ref(struct xwii_iface *dev)
{
	if (!dev || !dev->ref)
		return;

	dev->ref++;
}

void xwii_iface_unref(struct xwii_iface *dev)
{
	if (!dev || !dev->ref)
		return;

	if (--dev->ref)
		return;

	xwii_iface_close(dev, XWII_IFACE_ALL);
	close(dev->efd);
	free(dev->syspath);
	free(dev);
}

int xwii_iface_get_fd(struct xwii_iface *dev)
{
	if (!dev)
		return -1;

	return dev->efd;
}

/* maps interface ID to interface name */
static const char *id2name_map[] = {
	[XWII_IFACE_CORE] = XWII_NAME_CORE,
	[XWII_IFACE_ACCEL] = XWII_NAME_ACCEL,
	[XWII_IFACE_IR] = XWII_NAME_IR,
	[XWII_IFACE_MP] = XWII_NAME_MP,
	[XWII_IFACE_EXT] = XWII_NAME_EXT,
};

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
	int ret, fd, mod;

	if (wr)
		mod = O_RDWR;
	else
		mod = O_RDONLY;

	ret = open(path, mod | O_NONBLOCK | O_CLOEXEC);
	if (ret < 0)
		return -errno;
	fd = ret;

	if (ioctl(fd, EVIOCGID, id)) {
		close(fd);
		return -errno;
	}

	if (id[ID_BUS] != XWII_ID_BUS || id[ID_VENDOR] != XWII_ID_VENDOR ||
					id[ID_PRODUCT] != XWII_ID_PRODUCT) {
		close(fd);
		return -EINVAL;
	}

	if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0) {
		close(fd);
		return -errno;
	}

	name[sizeof(name) - 1] = 0;
	if (strcmp(id2name_map[type], name)) {
		close(fd);
		return -EINVAL;
	}

	return fd;
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
	static const char prefix[] = "/dev/input/";
	static const size_t plen = sizeof(prefix) - 1;
	size_t len, size;

	while (list) {
		if (list->name && !strcmp(list->name, id2name_map[iface]))
			break;
		list = list->next;
	}

	if (!list || !list->event)
		return NULL;

	len = strlen(list->event);
	size = plen + len;
	res = malloc(size + 1);
	if (!res)
		return NULL;

	res[size] = 0;
	memcpy(res, prefix, plen);
	memcpy(&res[plen], list->event, len);
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
	struct epoll_event ep;

	path = find_event(list, iface);
	if (!path)
		return -ENOENT;

	ret = open_ev(path, iface, wr);
	free(path);

	if (ret < 0)
		return ret;

	memset(&ep, 0, sizeof(ep));
	ep.events = EPOLLIN;
	ep.data.ptr = dev;
	if (epoll_ctl(dev->efd, EPOLL_CTL_ADD, ret, &ep) < 0) {
		close(ret);
		return -EFAULT;
	}

	dev->ifaces |= iface;
	return ret;
}

/*
 * Upload the generic rumble event to the device. This may later be used for
 * force-feedback effects. The event id is safed for later use.
 */
static void upload_rumble(struct xwii_iface *dev)
{
	struct ff_effect effect = {
		.type = FF_RUMBLE,
		.id = -1,
		.u.rumble.strong_magnitude = 1,
		.replay.length = 0,
		.replay.delay = 0,
	};

	if (ioctl(dev->if_core, EVIOCSFF, &effect) != -1)
		dev->rumble_id = effect.id;
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

	if (!dev)
		return -EINVAL;

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
		ret = 0;
		if (wr)
			upload_rumble(dev);
	}
	if (ifaces & XWII_IFACE_ACCEL) {
		ret = open_iface(dev, XWII_IFACE_ACCEL, wr, list);
		if (ret < 0)
			goto err_sys;
		dev->if_accel = ret;
		ret = 0;
	}
	if (ifaces & XWII_IFACE_IR) {
		ret = open_iface(dev, XWII_IFACE_IR, wr, list);
		if (ret < 0)
			goto err_sys;
		dev->if_ir = ret;
		ret = 0;
	}
	if (ifaces & XWII_IFACE_MP) {
		ret = open_iface(dev, XWII_IFACE_MP, wr, list);
		if (ret < 0)
			goto err_sys;
		dev->if_mp = ret;
		ret = 0;
	}
	if (ifaces & XWII_IFACE_EXT) {
		ret = open_iface(dev, XWII_IFACE_EXT, wr, list);
		if (ret < 0)
			goto err_sys;
		dev->if_ext = ret;
		ret = 0;
	}

err_sys:
	sfs_input_unref(list);
	return ret;
}

/* closes the interfaces given in \ifaces */
void xwii_iface_close(struct xwii_iface *dev, unsigned int ifaces)
{
	if (!dev || !ifaces)
		return;

	ifaces &= XWII_IFACE_ALL;

	if (ifaces & XWII_IFACE_CORE && dev->if_core != -1) {
		epoll_ctl(dev->efd, EPOLL_CTL_DEL, dev->if_core, NULL);
		close(dev->if_core);
		dev->if_core = -1;
	}
	if (ifaces & XWII_IFACE_ACCEL && dev->if_accel != -1) {
		epoll_ctl(dev->efd, EPOLL_CTL_DEL, dev->if_accel, NULL);
		close(dev->if_accel);
		dev->if_accel = -1;
	}
	if (ifaces & XWII_IFACE_IR && dev->if_ir != -1) {
		epoll_ctl(dev->efd, EPOLL_CTL_DEL, dev->if_ir, NULL);
		close(dev->if_ir);
		dev->if_ir = -1;
	}
	if (ifaces & XWII_IFACE_MP && dev->if_mp != -1) {
		epoll_ctl(dev->efd, EPOLL_CTL_DEL, dev->if_mp, NULL);
		close(dev->if_mp);
		dev->if_mp = -1;
	}
	if (ifaces & XWII_IFACE_EXT && dev->if_ext != -1) {
		epoll_ctl(dev->efd, EPOLL_CTL_DEL, dev->if_ext, NULL);
		close(dev->if_ext);
		dev->if_ext = -1;
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
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret <= 0) {
		xwii_iface_close(dev, XWII_IFACE_CORE);
		return -ENODEV;
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
	int ret;
	struct input_event input;

	if (dev->if_accel == -1)
		return -EAGAIN;

try_again:
	ret = read_event(dev->if_accel, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret <= 0) {
		xwii_iface_close(dev, XWII_IFACE_ACCEL);
		return -ENODEV;
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
	int ret;
	struct input_event input;

	if (dev->if_ir == -1)
		return -EAGAIN;

try_again:
	ret = read_event(dev->if_ir, &input);
	if (ret == -EAGAIN) {
		return -EAGAIN;
	} else if (ret <= 0) {
		xwii_iface_close(dev, XWII_IFACE_IR);
		return -ENODEV;
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

/*
 * Read new event from any opened interface of \dev.
 * Returns -EAGAIN if no new event can be read.
 * Returns 0 on success and writes the new event into \ev.
 * Returns negative error on failure.
 * Returns -ENODEV *once* if *any* interface failed and got closed. Further
 * reads may succeed on other interfaces but this seems unlikely as all event
 * devices are created and destroyed by the kernel at the same time. Therefore,
 * it is recommended to assume the device was disconnected if this returns
 * -ENODEV.
 * Returns -EAGAIN on further reads if no interface is open anymore.
 */
static int read_iface(struct xwii_iface *dev, struct xwii_event *ev)
{
	int ret;

	if (!dev || !ev)
		return -EFAULT;

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

int xwii_iface_read(struct xwii_iface *dev, struct xwii_event *ev)
{
	return read_iface(dev, ev);
}

/*
 * Poll for events on device \dev.
 * Returns -EAGAIN if no new events can be read.
 * Returns 0 on success and writes the new event into \ev.
 * Returns negative error on failure.
 * Returns -ENODEV *once* if *any* interface failed and got closed. Further
 * reads may succeed on other interfaces but this seems unlikely as all event
 * devices are created and destroyed by the kernel at the same time. Therefore,
 * it is recommended to assume the device was disconnected if this returns
 * -ENODEV.
 * Returns -EAGAIN on further reads if no interface is open anymore.
 *
 * This also writes all pending requests to the devices in contrast to *_read()
 * which only reads for events. If \ev is NULL, only pending requests are
 * written but no read is performed.
 */
int xwii_iface_poll(struct xwii_iface *dev, struct xwii_event *ev)
{
	if (!dev)
		return -EFAULT;

	if (ev)
		return read_iface(dev, ev);

	return 0;
}

/*
 * Toogle wiimote rumble motor
 * Enable or disable the rumble motor of \dev depending on \on. This requires
 * the core interface to be opened.
 */
int xwii_iface_rumble(struct xwii_iface *dev, bool on)
{
	struct input_event ev;
	int ret;

	if (dev->if_core == -1 || dev->rumble_id == -1)
		return -EINVAL;

	ev.type = EV_FF;
	ev.code = dev->rumble_id;
	ev.value = on;
	ret = write(dev->if_core, &ev, sizeof(ev));

	if (ret == -1)
		return -errno;
	else
		return 0;
}
