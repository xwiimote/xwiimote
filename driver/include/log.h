/*
 * XWiimote - driver - log.h
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

/*
 * Log Interface
 * The log interface opens a logfile when created, prints the
 * messages with additional information like time to the file
 * and optionally removes the logfile if destroyed.
 * Every logfile has a unique name and is owned by this process.
 *
 * Namespace: WII_LOG_*
 *            wii_log_*
 *
 * w_log_open() opens a new logfile or returns false on failure.
 * w_log_do() and w_log_vdo() print a message to an open logfile
 * and return false on failure. Log failures should be ignored
 * by the application because there is no nice way to recover.
 * w_log_close() frees all allocated memory.
 */

#ifndef WII_LOG_H
#define WII_LOG_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

struct wii_log {
	signed int fd;
	char path[100];
	bool tmp;
};

extern void wii_log_open(struct wii_log *logger, const char *name, bool tmp);
extern void wii_log_close(struct wii_log *logger);
extern bool wii_log_do(struct wii_log *logger, const char *format, ...);
extern bool wii_log_vdo(struct wii_log *logger, const char *format, va_list list);

#endif /* WII_LOG_H */
