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
};

struct xwii_device *xwii_device_new(void *dev)
{
	struct xwii_device *ret;

	ret = malloc(sizeof(*ret));
	if (!ret)
		return NULL;

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
