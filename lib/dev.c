/*
 * XWiimote - lib
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * Device Control
 * Use sysfs and evdev to control connected wiimotes.
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/limits.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "xwiimote.h"

struct xwii_dev {
	char *path;
	signed int input;
	struct xwii_state state;
	struct xwii_state cache;
	uint16_t cache_bits;
};

struct xwii_dev *xwii_dev_new(const char *syspath)
{
	struct xwii_dev *dev;

	dev = malloc(sizeof(*dev));
	if (!dev)
		return NULL;

	memset(dev, 0, sizeof(*dev));
	dev->path = strdup(syspath);
	if (!dev->path) {
		free(dev);
		return NULL;
	}

	dev->input = -1;
	return dev;
}

void xwii_dev_free(struct xwii_dev *dev)
{
	if (dev->input >= 0)
		close(dev->input);
	free(dev->path);
	free(dev);
}

/*
 * Concat "add" to "path" and return new allocated string. Free "path" if
 * do_free is true.
 * Returns NULL on malloc failure but "path" is ALWAYS freed if do_free is set.
 */
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

/*
 * Finds the input directory of a wiimote. "dev" must be the sysfs path
 * to the base directory of the wiimote.
 * Returns NULL on failure.
 */
static char *find_input_path(const char *dev)
{
	DIR *dir;
	struct dirent *e;
	char *path;

	path = concat_path(dev, "/input/", false);
	if (!path)
		return NULL;

	dir = opendir(path);
	if (!dir)
		goto error;

	while ((e = readdir(dir))) {
		if (strncmp(e->d_name, "input", 5))
			continue;

		path = concat_path(path, e->d_name, true);
		closedir(dir);
		return path;
	}

	closedir(dir);
error:
	free(path);
	return NULL;
}

/*
 * Finds the event name of a wiimote. Should get as argument the result of
 * find_input_path(). Returns a string like "event5" which then can be used to
 * open /dev/input/eventX.
 * Returns NULL on failure;
 */
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

/*
 * Open event device and test whether it really is a wiimote.
 * Returns an open fd or -1 on failure
 */
static int open_dev_event(const char *event, bool wr)
{
	char *path;
	int fd;
	uint16_t id[4];

	path = concat_path("/dev/input/", event, false);
	if (!path)
		return -1;

	fd = open(path, wr ? O_RDWR : O_RDONLY);
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

int xwii_dev_open_input(struct xwii_dev *dev, bool wr)
{
	char *input, *event;

	if (dev->input >= 0)
		return dev->input;

	input = find_input_path(dev->path);
	if (!input)
		return -1;

	event = find_event_name(input);
	if (!event) {
		free(input);
		return -1;
	}

	dev->input = open_dev_event(event, wr);

	free(event);
	free(input);

	return dev->input;
}

const struct xwii_state *xwii_dev_state(struct xwii_dev *dev)
{
	return &dev->state;
}

static uint16_t map_key(int32_t key)
{
	switch (key) {
	case KEY_LEFT:
		return XWII_KEY_LEFT;
	case KEY_RIGHT:
		return XWII_KEY_RIGHT;
	case KEY_UP:
		return XWII_KEY_UP;
	case KEY_DOWN:
		return XWII_KEY_DOWN;
	case KEY_NEXT:
		return XWII_KEY_PLUS;
	case KEY_PREVIOUS:
		return XWII_KEY_MINUS;
	case BTN_1:
		return XWII_KEY_ONE;
	case BTN_2:
		return XWII_KEY_TWO;
	case BTN_A:
		return XWII_KEY_A;
	case BTN_B:
		return XWII_KEY_B;
	case BTN_MODE:
		return XWII_KEY_HOME;
	default:
		return XWII_KEY_NONE;
	}
}

static void cache_key(struct xwii_dev *dev, struct input_event *ev)
{
	uint16_t key;

	key = map_key(ev->code);
	if (key == XWII_KEY_NONE)
		return;

	if (ev->value == 0) {
		dev->cache.keys &= ~key;
	} else {
		dev->cache.keys |= key;
	}
}

static void cache_abs(struct xwii_dev *dev, struct input_event *ev)
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

uint16_t xwii_dev_poll(struct xwii_dev *dev)
{
	struct input_event ev;
	int ret;
	uint16_t result;

try_again:

	ret = read(dev->input, &ev, sizeof(ev));
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

bool xwii_dev_read_led(struct xwii_dev *dev, int led)
{
	return false;
}

bool xwii_dev_read_rumble(struct xwii_dev *dev)
{
	return false;
}

bool xwii_dev_read_accel(struct xwii_dev *dev)
{
	return false;
}

enum xwii_ir xwii_dev_read_ir(struct xwii_dev *dev)
{
	return XWII_IR_OFF;
}
