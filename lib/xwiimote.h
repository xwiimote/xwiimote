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

#define XWII_KEY_UP	0x0001
#define XWII_KEY_DOWN	0x0002
#define XWII_KEY_LEFT	0x0004
#define XWII_KEY_RIGHT	0x0008
#define XWII_KEY_A	0x0010
#define XWII_KEY_B	0x0020
#define XWII_KEY_MINUS	0x0040
#define XWII_KEY_PLUS	0x0080
#define XWII_KEY_HOME	0x0100
#define XWII_KEY_ONE	0x0200
#define XWII_KEY_TWO	0x0400
#define XWII_KEYS	0x0fff

#define XWII_ACCEL	0x1000
#define XWII_IR		0x2000
#define XWII_BLOCKING	0x4000
#define XWII_CLOSED	0x8000

struct xwii_state {
	uint16_t keys;
	int16_t accelx;
	int16_t accely;
	int16_t accelz;
	uint16_t irx[4];
	uint16_t iry[4];
};

enum xwii_ir {
	XWII_IR_OFF,
	XWII_IR_BASIC,
	XWII_IR_EXTENDED,
	XWII_IR_FULL
};

extern struct xwii_device *xwii_device_new(void *dev);
extern void xwii_device_free(struct xwii_device *dev);

extern int xwii_device_open_input(struct xwii_device *dev, bool wr);
extern const struct xwii_state *xwii_device_state(struct xwii_device *dev);
extern uint16_t xwii_device_poll(struct xwii_device *dev, int fd);

extern bool xwii_device_read_led(struct xwii_device *dev, int led);
extern bool xwii_device_read_rumble(struct xwii_device *dev);
extern bool xwii_device_read_accel(struct xwii_device *dev);
extern enum xwii_ir xwii_device_read_ir(struct xwii_device *dev);

#ifdef __cplusplus
}
#endif

#endif /* XWII_XWIIMOTE_H */
