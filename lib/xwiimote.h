/*
 * XWiimote - lib
 * Written 2010, 2011, 2012 by David Herrmann
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
#include <sys/time.h>
#include <time.h>

#if (__GNUC__ > 3)
#define XWII__DEPRECATED __attribute__((__deprecated__))
#else
#define XWII__DEPRECATED
#endif /* __GNUC__ */

/*
 * Kernel Interface
 * This defines kernel interface constants that we use.
 */

#define XWII_ID_BUS		0x0005
#define XWII_ID_VENDOR		0x057e
#define XWII_ID_PRODUCT		0x0306

#define XWII__NAME		"Nintendo Wii Remote"
#define XWII_NAME_CORE		XWII__NAME
#define XWII_NAME_ACCEL		XWII__NAME " Accelerometer"
#define XWII_NAME_IR		XWII__NAME " IR"
#define XWII_NAME_MP		XWII__NAME " Motion+"
#define XWII_NAME_EXT		XWII__NAME " Extension"

/*
 * Event reader
 * This is a state machine that reads all wiimote event interfaces and returns
 * new events to the caller when receiving a sync-event.
 */

#define XWII_IFACE_CORE		0x000001	/* core interface */
#define XWII_IFACE_ACCEL	0x000002	/* accelerometer interface */
#define XWII_IFACE_IR		0x000004	/* ir interface */
#define XWII_IFACE_MP		0x000008	/* motion+ interface */
#define XWII_IFACE_EXT		0x000010	/* extension interface */
#define XWII_IFACE_ALL		0x00ffff	/* all interfaces */
#define XWII_IFACE_WRITABLE	0x010000	/* open iface writable */

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
	struct timeval time;
	unsigned int type;

	union xwii_event_union {
		struct xwii_event_key key;
		struct xwii_event_abs abs[4];
		uint8_t reserved[128];
	} v;
};

struct xwii_iface;

int xwii_iface_new(struct xwii_iface **dev, const char *syspath);
void xwii_iface_ref(struct xwii_iface *dev);
void xwii_iface_unref(struct xwii_iface *dev);
int xwii_iface_get_fd(struct xwii_iface *dev);

int xwii_iface_open(struct xwii_iface *dev, unsigned int ifaces);
void xwii_iface_close(struct xwii_iface *dev, unsigned int ifaces);
unsigned int xwii_iface_opened(struct xwii_iface *dev);
int xwii_iface_poll(struct xwii_iface *dev, struct xwii_event *ev);

int xwii_iface_rumble(struct xwii_iface *dev, bool on);

/* old deprecated functions */
XWII__DEPRECATED int xwii_iface_read(struct xwii_iface *dev, struct xwii_event *ev);

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

struct xwii_monitor *xwii_monitor_new(bool poll, bool direct);
void xwii_monitor_ref(struct xwii_monitor *mon);
void xwii_monitor_unref(struct xwii_monitor *mon);

int xwii_monitor_get_fd(struct xwii_monitor *monitor, bool blocking);
char *xwii_monitor_poll(struct xwii_monitor *monitor);

#ifdef __cplusplus
}
#endif

#endif /* XWII_XWIIMOTE_H */
