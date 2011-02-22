/*
 * XWiimote - driver - log.c
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include "log.h"

static signed int open_log(char *path, size_t len, const char *name, bool tmp)
{
	signed int fd;
	unsigned int i;

	if (tmp) {
		for (i = 0; i < 100; ++i) {
			snprintf(path, len, "/tmp/xwiimote-%s.%d.log", name, rand());
			fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_APPEND, S_IRUSR | S_IWUSR);
			if (fd >= 0)
				return fd;
		}
		return -1;
	}
	else {
		snprintf(path, len, "/var/log/xwiimote-%s.log", name);
		return open(path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
	}
}

void wii_log_open(struct wii_log *logger, const char *name, bool tmp)
{
	logger->fd = open_log(logger->path, sizeof(logger->path), name, tmp);
	if (logger->fd < 0)
		logger->fd = 2;
	logger->tmp = tmp;
}

void wii_log_close(struct wii_log *logger)
{
	close(logger->fd);
	if (logger->tmp)
		unlink(logger->path);
}

bool wii_log_do(struct wii_log *logger, const char *format, ...)
{
	va_list list;
	bool ret;

	va_start(list, format);
	ret = wii_log_vdo(logger, format, list);
	va_end(list);
	return ret;
}

bool wii_log_vdo(struct wii_log *logger, const char *format, va_list list)
{
	if (dprintf(logger->fd, "%ld: ", time(NULL)) < 1)
		return false;
	if (vdprintf(logger->fd, format, list) < 1)
		return false;
	if (dprintf(logger->fd, "\n") < 1)
		return false;
	return true;
}
