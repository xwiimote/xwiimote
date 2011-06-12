/*
 * XWiimote - lib
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

#ifndef XWII_XWIIMOTE_H
#define XWII_XWIIMOTE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdlib.h>

/*
 * Device Enumeration and Monitorig
 * Use libudev to enumerate all currently connected devices and allow
 * monitoring the system for new devices.
 */

struct xwii_monitor;
struct xwii_device;

extern struct xwii_monitor *xwii_monitor_new(bool poll, bool direct);
extern void xwii_monitor_free(struct xwii_monitor *monitor);
extern int xwii_monitor_get_fd(struct xwii_monitor *monitor, bool blocking);
extern struct xwii_device *xwii_monitor_poll(struct xwii_monitor *monitor);

/*
 * Device Control
 * This API is used to control and modify connected devices.
 */

extern struct xwii_device *xwii_device_new(void *dev);
extern void xwii_device_free(struct xwii_device *dev);
extern int xwii_device_open_input(struct xwii_device *dev, bool wr);

#ifdef __cplusplus
}
#endif

#endif /* XWII_XWIIMOTE_H */
