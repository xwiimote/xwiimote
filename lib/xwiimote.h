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
#include <stdint.h>
#include <stdlib.h>

/*
 * Kernel Interface
 * This defines kernel interface constants that we use.
 */

#define XWII_ID_BUS 0x0005
#define XWII_ID_VENDOR 0x057e
#define XWII_ID_PRODUCT 0x0306

#define XWII__NAME "Nintendo Wii Remote"
#define XWII_NAME_CORE XWII__NAME
#define XWII_NAME_ACCEL XWII__NAME " Accelerometer"
#define XWII_NAME_IR XWII__NAME " IR"
#define XWII_NAME_MP XWII__NAME " Motion+"
#define XWII_NAME_EXT XWII__NAME " Extension"

/*
 * Event reader
 * This is a state machine that reads all wiimote event interfaces and returns
 * new events to the caller when receiving a sync-event.
 */

#define XWII_IFACE_CORE		0x0001	/* core interface */
#define XWII_IFACE_ACCEL	0x0002	/* accelerometer interface */
#define XWII_IFACE_IR		0x0004	/* ir interface */
#define XWII_IFACE_ALL (XWII_IFACE_CORE | XWII_IFACE_ACCEL | XWII_IFACE_IR)
#define XWII_IFACE_WRITABLE	0x0100	/* open iface writable */

enum xwii_event_types {
	XWII_EVENT_KEY,		/* key event */
	XWII_EVENT_ACCEL,	/* accelerometer event */
	XWII_EVENT_IR,		/* IR event */
	XWII_EVENT_NUM
};

enum xwii_event_keys {
	XWII_KEY_LEFT,
	XWII_KEY_RIGHT,
	XWII_KEY_UP,
	XWII_KEY_DOWN,
	XWII_KEY_A,
	XWII_KEY_B,
	XWII_KEY_PLUS,
	XWII_KEY_MINUS,
	XWII_KEY_HOME,
	XWII_KEY_ONE,
	XWII_KEY_TWO,
	XWII_KEY_NUM
};

struct xwii_event_key {
	unsigned int code;
	unsigned int state;
};

struct xwii_event_abs {
	int32_t x;
	int32_t y;
	int32_t z;
};

struct xwii_event {
	unsigned int type;
	union xwii_event_union {
		struct xwii_event_key key;
		struct xwii_event_abs abs[4];
	} v;
};

struct xwii_iface;

extern int xwii_iface_new(struct xwii_iface **dev, const char *syspath);
extern struct xwii_iface *xwii_iface_ref(struct xwii_iface *dev);
extern void xwii_iface_unref(struct xwii_iface *dev);

extern int xwii_iface_open(struct xwii_iface *dev, unsigned int ifaces);
extern void xwii_iface_close(struct xwii_iface *dev, unsigned int ifaces);
extern unsigned int xwii_iface_opened(struct xwii_iface *dev);
extern int xwii_iface_read(struct xwii_iface *dev, struct xwii_event *ev);
extern int xwii_iface_rumble(struct xwii_iface *dev, bool on);

/*
 * Device monitor
 * This monitor can be used to enumerate all connected wiimote devices and also
 * monitoring the system for hotplugged wiimote devices.
 * This is a simple wrapper around libudev and should only be used if your
 * application does not use udev on its own.
 * See the implementation of the monitor to integrate wiimote-monitoring into
 * your own udev routines.
 */

struct xwii_monitor;

extern struct xwii_monitor *xwii_monitor_new(bool poll, bool direct);
extern struct xwii_monitor *xwii_monitor_ref(struct xwii_monitor *mon);
extern void xwii_monitor_unref(struct xwii_monitor *mon);

extern int xwii_monitor_get_fd(struct xwii_monitor *monitor, bool blocking);
extern char *xwii_monitor_poll(struct xwii_monitor *monitor);

#ifdef __cplusplus
}
#endif

#endif /* XWII_XWIIMOTE_H */
