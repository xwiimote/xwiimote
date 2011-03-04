/*
 * XWiimote - driver - main.h
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

/*
 * Miscellaneous Declarations
 * Contains declarations of the main driver core and
 * other stuff that doesn't fit into the other headers.
 */

#ifndef WII_MAIN_H
#define WII_MAIN_H

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

/*
 * If the application catched a signal, this will
 * return true. Otherwise it returns false.
 * If "print" is true, this function prints an
 * error string to stdout if a signal was catched.
 */
extern bool wii_terminating(bool print);

/*
 * Helper function which forks the application twice to
 * make the new process an orphan.
 */
extern bool wii_fork(void (*func)(void *arg), void *arg);

/*
 * This starts the driver infrastructure for the
 * given IO ports.
 *
 * \in is a channel to the wiimote where we can read data.
 * \out is a channel to the wiimote where we can send data.
 * They must not be the same.
 * This function blocks until the driver terminates. Call this
 * with wii_fork().
 */
struct wii_drv_io {
	signed int in;
	signed int out;
};

extern void wii_start_driver(void *drv);

/* clamping */
static inline signed int clamp_int(int64_t val)
{
	if (val > INT_MAX)
		return INT_MAX;
	if (val < INT_MIN)
		return INT_MIN;
	return val;
}

/* get current time of unsettable monotonic clock */
static inline int64_t time_now()
{
	int64_t ret;
	struct timespec val;

	if (clock_gettime(CLOCK_MONOTONIC_RAW, &val) != 0)
		clock_gettime(CLOCK_MONOTONIC, &val);

	ret = val.tv_sec * 1000;
	ret += val.tv_nsec / 1000000;
	return ret;
}

#endif /* WII_MAIN_H */
