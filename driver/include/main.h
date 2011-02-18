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

#include <stdbool.h>
#include <stdlib.h>

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

#endif /* WII_MAIN_H */
