/*
 * XWiimote - lib
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * Device Control
 * Use sysfs to control connected wiimotes.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/input.h>
#include <linux/limits.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "xwiimote.h"

struct xwii_device {
	struct udev_device *udev;
	struct xwii_state state;
	struct xwii_state cache;
	uint16_t cache_bits;
};

struct xwii_device *xwii_device_new(void *dev)
{
	struct xwii_device *ret;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;

	memset(ret, 0, sizeof(*ret));
	ret->udev = udev_device_ref(dev);
	return ret;
}

void xwii_device_free(struct xwii_device *dev)
{
	udev_device_unref(dev->udev);
	free(dev);
}

static char *concat_path(const char *path, const char *add, bool do_free)
{
	char *str;
	size_t plen, alen;

	plen = strlen(path);
	alen = strlen(add);

	str = malloc(plen + alen + 1);
	if (str) {
		memcpy(str, path, plen);
		memcpy(&str[plen], add, alen + 1);
	}

	if (do_free)
		free((void*)path);

	return str;
}

static char *find_input_path(const char *dev)
{
	DIR *dir;
	struct dirent *e;
	char *path;

	path = concat_path(dev, "/input/", false);
	if (!path)
		return NULL;

	dir = opendir(path);
	if (!dir) {
		free(path);
		return NULL;
	}

	while ((e = readdir(dir))) {
		if (strncmp(e->d_name, "input", 5))
			continue;

		path = concat_path(path, e->d_name, true);
		closedir(dir);
		return path;
	}

	closedir(dir);
	free(path);

	return NULL;
}

static char *find_event_name(const char *dev_input)
{
	DIR *dir;
	struct dirent *e;
	char *event;

	dir = opendir(dev_input);
	if (!dir)
		return NULL;

	while ((e = readdir(dir))) {
		if (strncmp(e->d_name, "event", 5))
			continue;

		event = strdup(e->d_name);
		closedir(dir);
		return event;
	}

	closedir(dir);

	return NULL;
}

static int open_dev_event(const char *event, bool wr)
{
	char *path;
	int fd;
	uint16_t id[4];

	path = concat_path("/dev/input/", event, false);
	if (!path)
		return -1;

	fd = open(path, wr?O_RDWR:O_RDONLY);
	free(path);

	if (fd < 0)
		return -1;

	if (!ioctl(fd, EVIOCGID, id)) {
		if (id[ID_BUS] == 0x0005 &&
			id[ID_VENDOR] == 0x057e &&
			id[ID_PRODUCT] == 0x0306)
			return fd;
	}

	close(fd);

	return -1;
}

int xwii_device_open_input(struct xwii_device *dev, bool wr)
{
	const char *path;
	char *input, *event, *event2;
	int fd;

	path = udev_device_get_syspath(dev->udev);
	if (!path)
		return -1;

	input = find_input_path(path);
	if (!input)
		return -1;

	event = find_event_name(input);
	if (!event) {
		free(input);
		return -1;
	}

	fd = open_dev_event(event, wr);

	/*
	 * Find event name again to go sure our device didn't disconnect
	 * and another wiimote connected and got the same event name.
	 */
	if (fd >= 0) {
		event2 = find_event_name(input);
		if (!event2 || strcmp(event, event2)) {
			close(fd);
			fd = -1;
		}
		free(event2);
	}

	free(event);
	free(input);

	return fd;
}

const struct xwii_state *xwii_device_state(struct xwii_device *dev)
{
	return &dev->state;
}

static uint16_t keymap[] = {
	[KEY_LEFT] = XWII_KEY_LEFT,
	[KEY_RIGHT] = XWII_KEY_RIGHT,
	[KEY_UP] = XWII_KEY_UP,
	[KEY_DOWN] = XWII_KEY_DOWN,
	[KEY_NEXT] = XWII_KEY_PLUS,
	[KEY_PREVIOUS] = XWII_KEY_MINUS,
	[BTN_1] = XWII_KEY_ONE,
	[BTN_2] = XWII_KEY_TWO,
	[BTN_A] = XWII_KEY_A,
	[BTN_B] = XWII_KEY_B,
	[BTN_MODE] = XWII_KEY_HOME,
};

static void cache_key(struct xwii_device *dev, struct input_event *ev)
{
	uint16_t key;

	key = keymap[ev->code];
	if (!key)
		return;

	if (ev->value == 0) {
		dev->cache.keys &= ~key;
	} else {
		dev->cache.keys |= key;
	}
}

static void cache_abs(struct xwii_device *dev, struct input_event *ev)
{
	switch (ev->code) {
		case ABS_X:
			dev->cache_bits |= XWII_ACCEL;
			dev->cache.accelx = ev->value;
			break;
		case ABS_Y:
			dev->cache_bits |= XWII_ACCEL;
			dev->cache.accely = ev->value;
			break;
		case ABS_Z:
			dev->cache_bits |= XWII_ACCEL;
			dev->cache.accelz = ev->value;
			break;
		case ABS_HAT0X:
			dev->cache_bits |= XWII_IR;
			dev->cache.irx[0] = ev->value;
			break;
		case ABS_HAT0Y:
			dev->cache_bits |= XWII_IR;
			dev->cache.iry[0] = ev->value;
			break;
		case ABS_HAT1X:
			dev->cache_bits |= XWII_IR;
			dev->cache.irx[1] = ev->value;
			break;
		case ABS_HAT1Y:
			dev->cache_bits |= XWII_IR;
			dev->cache.iry[1] = ev->value;
			break;
		case ABS_HAT2X:
			dev->cache_bits |= XWII_IR;
			dev->cache.irx[2] = ev->value;
			break;
		case ABS_HAT2Y:
			dev->cache_bits |= XWII_IR;
			dev->cache.iry[2] = ev->value;
			break;
		case ABS_HAT3X:
			dev->cache_bits |= XWII_IR;
			dev->cache.irx[3] = ev->value;
			break;
		case ABS_HAT3Y:
			dev->cache_bits |= XWII_IR;
			dev->cache.iry[3] = ev->value;
			break;
	}
}

uint16_t xwii_device_poll(struct xwii_device *dev, int fd)
{
	struct input_event ev;
	int ret;
	uint16_t result;

try_again:

	ret = read(fd, &ev, sizeof(ev));
	if (ret != sizeof(ev)) {
		if (ret < 0 && errno == EAGAIN)
			return XWII_BLOCKING;
		else
			return XWII_CLOSED;
	}

	switch (ev.type) {
		case EV_KEY:
			cache_key(dev, &ev);
			break;
		case EV_ABS:
			cache_abs(dev, &ev);
			break;
		case EV_SYN:
			result = dev->state.keys ^ dev->cache.keys;
			result |= dev->cache_bits;
			dev->cache_bits = 0;
			memcpy(&dev->state, &dev->cache, sizeof(dev->state));
			return result;
	}

	goto try_again;
}

bool xwii_device_read_led(struct xwii_device *dev, int led)
{
	return false;
}

bool xwii_device_read_rumble(struct xwii_device *dev)
{
	return false;
}

bool xwii_device_read_accel(struct xwii_device *dev)
{
	return false;
}

enum xwii_ir xwii_device_read_ir(struct xwii_device *dev)
{
	return XWII_IR_OFF;
}
