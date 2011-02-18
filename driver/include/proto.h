/*
 * XWiimote - driver - proto.h
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

/*
 * Protocol Parser
 * Transmission layer independent protocol parser that has no
 * external dependencies. This is an abstraction layer which may
 * be used to implement wiimote drivers on other operating systems.
 *
 * It takes as input the raw input buffers, parses it, performs
 * internal operations and manages an output buffer which is used
 * to send data back to the device.
 * No callbacks are used, this integrates into any kind of event
 * engine or any other kind of application layout.
 *
 * Namespace: WII_PROTO_*
 *            wii_proto_*
 *
 * TODO:
 *  - components:
 *    - add speaker unit
 *    - add IR unit
 *    - add extensions
 *  - specification:
 *    - check whether WII_SRK1_POWER is right
 *    - investigate whether report 0x10 is a rumble only output report
 */

#ifndef WII_PROTO_H
#define WII_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

/* ISO C89 includes */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* returns unsigned 64bit integer with the X'th bit set */
#define WII_PROTO_BIT(X) (0x1ULL << (X))

/* this is used to create bitmasks in enums */
#define WII_PROTO_MASK(NAME) NAME ## __MASK_BEGIN, NAME = WII_PROTO_BIT(NAME ## __MASK_BEGIN), NAME ## __MASK_END = NAME ## __MASK_BEGIN

/*
 * Protocol Specification
 * Provides constants and structures describing the hardware details
 * of the wiimote and the used protocol. This is used to encode and decode
 * the protocol.
 *
 * Namespace: WII_PROTO_S?_*
 *            wii_proto_s?_*
 *
 * Bluetooth HID related parameters use the namespace:
 *   WII_PROTO_SH_*
 *   wii_proto_sh_*
 * All protocol reports (input and output) use the namespace:
 *   WII_PROTO_SR_*
 *   wii_proto_sr_*
 */

/* max package size */
#define WII_PROTO_SH_MAX 23

/* HID commands */
#define WII_PROTO_SH_CMD_OUT 0x52
#define WII_PROTO_SH_CMD_IN 0xa1

/*
 * Common Report (output)
 * This is no single output but included as the first byte
 * in many other outputs. 2 flags have always the same meaning.
 * The other flags are redefined by each output using them.
 */
struct wii_proto_sr_common {
	uint8_t flags;
} __attribute__((__packed__));

#define WII_PROTO_SR_COMMON_RUMBLE 0x01
#define WII_PROTO_SR_COMMON_X2 0x02
#define WII_PROTO_SR_COMMON_ENABLE 0x04
#define WII_PROTO_SR_COMMON_X4 0x08
#define WII_PROTO_SR_COMMON_X5 0x10
#define WII_PROTO_SR_COMMON_X6 0x20
#define WII_PROTO_SR_COMMON_X7 0x40
#define WII_PROTO_SR_COMMON_X8 0x80

/*
 * LED Report (output)
 * The LED report sets the LEDs on the wiimote. It redefines
 * the high 4 bits of the common output.
 */
#define WII_PROTO_SR_LED 0x11

struct wii_proto_sr_led {
	struct wii_proto_sr_common common;
} __attribute__((__packed__));

#define WII_PROTO_SR_COMMON_LED1 WII_PROTO_SR_COMMON_X5
#define WII_PROTO_SR_COMMON_LED2 WII_PROTO_SR_COMMON_X6
#define WII_PROTO_SR_COMMON_LED3 WII_PROTO_SR_COMMON_X7
#define WII_PROTO_SR_COMMON_LED4 WII_PROTO_SR_COMMON_X8

/*
 * Format Report (output)
 * This reports sets the data format used by the device to report
 * input. Its payload includes the common output and the requested
 * data reporting mode.
 */
#define WII_PROTO_SR_FORMAT 0x12

struct wii_proto_sr_format {
	struct wii_proto_sr_common common;
	uint8_t mode;
} __attribute__((__packed__));

/*
 * Query Report (output)
 * The query report requests a new status report from the device. Its
 * payload includes only the common output.
 */
#define WII_PROTO_SR_QUERY 0x15

struct wii_proto_sr_query {
	struct wii_proto_sr_common common;
} __attribute__((__packed__));

/*
 * Key Report (input)
 * This is no report but common information that is used by many
 * input reports. It contains the information about the key state
 * on the device. Five bits are unused and can be redefined by other
 * reports.
 */
struct wii_proto_sr_key {
	uint8_t k1;
	uint8_t k2;
} __attribute__((__packed__));

#define WII_PROTO_SR_KEY1_LEFT 0x01
#define WII_PROTO_SR_KEY1_RIGHT 0x02
#define WII_PROTO_SR_KEY1_DOWN 0x04
#define WII_PROTO_SR_KEY1_UP 0x08
#define WII_PROTO_SR_KEY1_PLUS 0x10
#define WII_PROTO_SR_KEY1_X6 0x20
#define WII_PROTO_SR_KEY1_X7 0x40
#define WII_PROTO_SR_KEY1_X8 0x80
#define WII_PROTO_SR_KEY2_TWO 0x01
#define WII_PROTO_SR_KEY2_ONE 0x02
#define WII_PROTO_SR_KEY2_B 0x04
#define WII_PROTO_SR_KEY2_A 0x08
#define WII_PROTO_SR_KEY2_MINUS 0x10
#define WII_PROTO_SR_KEY2_X6 0x20
#define WII_PROTO_SR_KEY2_X7 0x40
#define WII_PROTO_SR_KEY2_HOME 0x80

/*
 * Status Report (input)
 * The status reports contains information about the wiimote's status.
 * The payload includes the key information, special flags and the
 * battery status.
 */
#define WII_PROTO_SR_STATUS 0x20

struct wii_proto_sr_status {
	struct wii_proto_sr_key key;
	uint8_t flags;
	uint16_t unknown;
	uint8_t battery;
} __attribute__((__packed__));

#define WII_PROTO_SR_STATUS_EMPTY 0x01
#define WII_PROTO_SR_STATUS_EXT 0x02
#define WII_PROTO_SR_STATUS_SPKR 0x04
#define WII_PROTO_SR_STATUS_IR 0x08
#define WII_PROTO_SR_STATUS_LED1 0x10
#define WII_PROTO_SR_STATUS_LED2 0x20
#define WII_PROTO_SR_STATUS_LED3 0x40
#define WII_PROTO_SR_STATUS_LED4 0x80

/*
 * K DRM Report (input)
 * Data report mode which contains only key information. Payload includes
 * the common key information only.
 */
#define WII_PROTO_SR_K 0x30

struct wii_proto_sr_K {
	struct wii_proto_sr_key key;
} __attribute__((__packed__));

/*
 * KA DRM Report (input)
 * Data report mode which contains key and accelerometer data.
 */
#define WII_PROTO_SR_KA 0x31

struct wii_proto_sr_KA {
	struct wii_proto_sr_key key;
	uint8_t accel[3];
} __attribute__((__packed__));

/*
 * Components
 * Describtion of each component of a wiimote and the possible
 * states of the components. This provides commands structures
 * and report structures to control the wiimote. This is the
 * first abstraction layer of the wiimote device.
 *
 * Namespace: WII_PROTO_C?_*
 *            wii_proto_c?_*
 *
 * A component is a single unit on a wiimote. Each component has
 * a set of reports which it sends to the host and a set of
 * commands that it accepts from the host. A single component must
 * support at least one report or command.
 * Each report and command has a payload which specifies the
 * arguments for that report or command.
 *
 * All units are defined as WII_PROTO_CU_<name>, reports are
 * defined as WII_PROTO_CR_<name> and commands are defined as
 * WII_PROTO_CC_<name>. All these constants are flags that can
 * be set in a single integer.
 * The payload for each command is declared as wii_proto_cc_<name>
 * and for reports as wii_proto_cr_<name>. Not all commands or reports
 * have payloads.
 *
 * Available units:
 *   CU_STATUS:
 *     The status unit provides basic information about the wiimote. It
 *     generates the report CR_BATTERY which gives information about the
 *     battery charge of the wiimote. This report is not sent continuously
 *     but must be requested with CC_QUERY.
 *     This unit also controls the rumble chip on the wiimote which can be
 *     enabled and disabled with CC_RUMBLE. The leds on the wiimote can
 *     be controlled with CC_LED.
 *     Last, the data report mode can be reset with CC_FORMAT. The actual
 *     data report mode that is requested with this command is set by this
 *     library depending on which units are enabled.
 *     This unit is always enabled and cannot be disabled.
 *   CU_INPUT:
 *     This unit controls the keyboard with all its buttons on the wiimote.
 *     It generates the CR_KEY report when a key state changes. It always
 *     reports the state of all keys. No commands are supported.
 *   CU_ACCEL:
 *     This unit controls the accelerometer on the wiimote. It generates the
 *     CR_MOVE event if the wiimote gets moved and the accelerometer reports
 *     data. The accelerometer can be calibrated with the CC_ACALIB command.
 */

enum {
/* units */
	WII_PROTO_MASK(WII_PROTO_CU_STATUS),
	WII_PROTO_MASK(WII_PROTO_CU_INPUT),
	WII_PROTO_MASK(WII_PROTO_CU_ACCEL),
/* reports */
	WII_PROTO_MASK(WII_PROTO_CR_BATTERY),
	WII_PROTO_MASK(WII_PROTO_CR_KEY),
	WII_PROTO_MASK(WII_PROTO_CR_MOVE),
/* commands */
	WII_PROTO_MASK(WII_PROTO_CC_RUMBLE),
	WII_PROTO_MASK(WII_PROTO_CC_LED),
	WII_PROTO_MASK(WII_PROTO_CC_QUERY),
	WII_PROTO_MASK(WII_PROTO_CC_FORMAT),
	WII_PROTO_MASK(WII_PROTO_CC_ACALIB),
};

struct wii_proto_cr_battery {
	unsigned int low : 1;
	uint8_t level;
};

struct wii_proto_cr_key {
	unsigned int up : 1;
	unsigned int left : 1;
	unsigned int right : 1;
	unsigned int down : 1;
	unsigned int a : 1;
	unsigned int b : 1;
	unsigned int minus : 1;
	unsigned int home : 1;
	unsigned int plus : 1;
	unsigned int one : 1;
	unsigned int two : 1;
};

struct wii_proto_cr_move {
	int16_t x;
	int16_t y;
	int16_t z;
};

struct wii_proto_cc_rumble {
	unsigned int on : 1;
};

struct wii_proto_cc_led {
	unsigned int one : 1;
	unsigned int two : 1;
	unsigned int three : 1;
	unsigned int four : 1;
};

struct wii_proto_cc_acalib {
	int16_t x;
	int16_t y;
	int16_t z;
};

/*
 * Protocol Handler
 * The Wiimote uses a fake-HID protocol, thus no generic HID
 * parser can be used. This file provides a parser that can
 * handle the protocol related stuff and works as a gateway
 * between the bluetooth raw I/O channels and the event driven
 * device driver.
 *
 * Namespace: WII_PROTO_*
 *            wii_proto_*
 *
 * The I/O handler is defined as struct wii_proto_dev and provides a
 * device which is initialized with wii_proto_init(). It can be
 * deinitialized (which frees all internally allocated memory
 * associated with that device) at any time with wii_proto_deinit().
 * All wii_proto_*() functions are thread safe as long as they use
 * different struct wii_proto_dev device structures. A single device
 * structure can only be used in one thread. That is, no mutexes,
 * threads, cond-vars, signal-masks or any related stuff is used,
 * the functions are simply designed with reentrancy in mind.
 *
 * To use the device you need to connect to your wiimote via
 * bluetooth and establish two L2CAP channels with PSM 0x11
 * and 0x13. Then read L2CAP packets from the channel with PSM 0x13 and
 * pass this raw data to wii_proto_decode(). This function will
 * decode the raw input data, analyse it, adjust internal settings
 * and write all new events into the struct wii_proto_res variable
 * that is passed as last argument.
 * The "modified" member of this variable is set to the reports
 * that have been received so you can see all the new data that
 * is available. The commands inside the struct wii_proto_res are not
 * touched by this functions, nor does "modified" contain any
 * other flags than report-flags.
 * If "modified" is zero and "error" is WII_PROTO_E_NONE, then no
 * event occurred. Any other "error" value marks an error. See
 * below for valid error codes. No errors are fatal to the parser
 * so you can always continue using it, however, it is recommended
 * to abort the connection after two many errors during a short
 * period.
 *
 * By default, no events are reported back but all events are
 * discarded as long as you do not explicitely request those
 * events. To request all events belonging to a specific unit
 * you need to enable this unit. After disabling a unit all
 * related events will no longer be reported.
 * To enable a unit, use wii_proto_enable() with the WII_PROTO_CU_<name>
 * unit flags as parameter. All passed units will be enabled.
 * To disable a unit, use wii_proto_disable().
 * wii_proto_enabled() tests whether the passed units are enabled.
 * The WII_PROTO_CU_STATUS unit is always enabled.
 *
 * The wii_proto_do*() functions can be used to explicitely send
 * commands to the wiimote. The wii_proto_do() function is a wrapper
 * for all other wii_proto_do_*() functions. It is recommended to
 * use wii_proto_do() to supply a whole batch of commands which are
 * all sent at once to reduce bandwidth.
 * As argument to wii_proto_do() pass a struct wii_proto_res structure with
 * all command-flags that you want to supply listed in the "modified"
 * variable. Be sure to pass valid command payloads for all of
 * your commands that need a payload.
 * If a command belongs to a unit that is disabled, then this command is dropped.
 *
 * Any wii_proto_*() function may append outgoing commands to the internal
 * output-buffer. You need to check regularily wether there are pending
 * commands and then send them to the device. If you skip checking for
 * pending commands, your commands may be received with a delay from the
 * device.
 * To check for pending command your need to call
 * wii_proto_encode() to see whether there are pending outgoing messages.
 * If it returns true, the passed struct wii_proto_buf variable is filled with
 * a outgoing message. "buf" contains the message and "size" is the length
 * of the message that you need to send over the l2cap channel.
 * After that you need to call wii_proto_encode() again to see whether there
 * are more messages until it returns false which means there are no more
 * outgoing messages.
 * If your l2cap-out-queue is full you may postpone your next wii_proto_encode()
 * call until the out-queue is free again and you may also invoke
 * any other wii_proto_*() function even if the queue is full, however,
 * be sure the wii_proto_encode() queue does not become too full to avoid
 * long delays.
 * The struct wii_proto_buf variable has one more member named "wait" which is
 * an unsigned 8bit integer which tells you how many milliseconds you
 * need to wait until you should send the next message in the
 * wii_proto_encode() queue. To avoid sleep()'ing this amount of msecs
 * you should of course pass control to your event-engine or other
 * functions and make sure that control is passed to your bluetooth
 * handlers again after this amount of msecs. This is totally up to you.
 * However, this allows to integrate this handler into any event
 * engine.
 * The maximum sleep value is 255 milliseconds so if you use it in a cmd-application
 * you may even use usleep() for this.
 */

typedef uint32_t wii_proto_mask_t;

enum {
	WII_PROTO_E_NONE, /* no error occurred */
	WII_PROTO_E_EMPTY, /* the passed data was empty (size=0) */
	WII_PROTO_E_BADHID, /* the HID header was invalid */
	WII_PROTO_E_BADREP, /* the report header was invalid */
	WII_PROTO_E_BADARG, /* the payload was invalid */
};

struct wii_proto_res {
	unsigned int error;
	wii_proto_mask_t modified;
	struct wii_proto_cr_battery battery;
	struct wii_proto_cr_key key;
	struct wii_proto_cr_move move;
	struct wii_proto_cc_rumble rumble;
	struct wii_proto_cc_led led;
	struct wii_proto_cc_acalib acalib;
};

struct wii_proto_buf;
struct wii_proto_buf {
	struct wii_proto_buf *next;
	uint8_t buf[WII_PROTO_SH_MAX];
	size_t size;
	uint8_t wait;
};

struct wii_proto_dev {
	wii_proto_mask_t units;
	struct wii_proto_buf *buf_list;
	struct wii_proto_buf *buf_free;
	struct {
		struct wii_proto_cc_rumble rumble;
		struct wii_proto_cc_led led;
		struct wii_proto_cc_acalib acalib;
	} cache;
};

extern void wii_proto_init(struct wii_proto_dev *dev);
extern void wii_proto_deinit(struct wii_proto_dev *dev);

extern void wii_proto_decode(struct wii_proto_dev *dev, const void *buf, size_t size, struct wii_proto_res *res);
extern bool wii_proto_encode(struct wii_proto_dev *dev, struct wii_proto_buf *buf);

extern void wii_proto_enable(struct wii_proto_dev *dev, wii_proto_mask_t units);
extern void wii_proto_disable(struct wii_proto_dev *dev, wii_proto_mask_t units);
#define wii_proto_enabled(DEV, UNITS) (((DEV)->units & (UNITS)) == (UNITS))
extern void wii_proto_do(struct wii_proto_dev *dev, const struct wii_proto_res *res);
extern void wii_proto_do_led(struct wii_proto_dev *dev, const struct wii_proto_cc_led *pl);
extern void wii_proto_do_rumble(struct wii_proto_dev *dev, const struct wii_proto_cc_rumble *pl);
extern void wii_proto_do_query(struct wii_proto_dev *dev);
extern void wii_proto_do_format(struct wii_proto_dev *dev);
extern void wii_proto_do_acalib(struct wii_proto_dev *dev, const struct wii_proto_cc_acalib *pl);

#ifdef __cplusplus
}
#endif

#endif /* WII_PROTO_H */
