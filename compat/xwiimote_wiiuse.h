/*
 * XWiimote - wiiuse Compatibility
 * Written 2010, 2011, 2012 by David Herrmann
 * Modifications Dedicated to the Public Domain
 */

/*
 * wiiuse Compatibility
 * This is a sanitized header from the wiiuse package. It is binary compatible
 * to the wiiuse library but our implementation uses the xwiimote stack instead
 * of the wiiuse stack.
 *
 * If you want this as drop-in replacement for wiiuse you should copy this
 * header xwiimote_wiiuse.h to wiiuse.h and symlink libwiiuse.so to the lib.
 */

/*
 * Original WiiUse Copyright:
 *
 * wiiuse
 *
 * Written By:
 * Michael Laforest < para >
 * Email: < thepara (--AT--) g m a i l [--DOT--] com >
 *
 * Copyright 2006-2007
 *
 * This file is part of wiiuse.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef XWII_WIIUSE_H
#define XWII_WIIUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

/* we define this to avoid races with original wiiuse.h */
#define WIIUSE_H_INCLUDED

/* we do not support Microsoft Windows (original wiiuse does) */
#ifdef _WIN32
	#error "Microsoft Windows is not supported by this library"
#else
	#include <bluetooth/bluetooth.h>
#endif

#define WIIMOTE_LED_NONE 0x00
#define WIIMOTE_LED_1 0x10
#define WIIMOTE_LED_2 0x20
#define WIIMOTE_LED_3 0x40
#define WIIMOTE_LED_4 0x80

#define WIIMOTE_BUTTON_TWO 0x0001
#define WIIMOTE_BUTTON_ONE 0x0002
#define WIIMOTE_BUTTON_B 0x0004
#define WIIMOTE_BUTTON_A 0x0008
#define WIIMOTE_BUTTON_MINUS 0x0010
#define WIIMOTE_BUTTON_ZACCEL_BIT6 0x0020
#define WIIMOTE_BUTTON_ZACCEL_BIT7 0x0040
#define WIIMOTE_BUTTON_HOME 0x0080
#define WIIMOTE_BUTTON_LEFT 0x0100
#define WIIMOTE_BUTTON_RIGHT 0x0200
#define WIIMOTE_BUTTON_DOWN 0x0400
#define WIIMOTE_BUTTON_UP 0x0800
#define WIIMOTE_BUTTON_PLUS 0x1000
#define WIIMOTE_BUTTON_ZACCEL_BIT4 0x2000
#define WIIMOTE_BUTTON_ZACCEL_BIT5 0x4000
#define WIIMOTE_BUTTON_UNKNOWN 0x8000
#define WIIMOTE_BUTTON_ALL 0x1F9F

#define NUNCHUK_BUTTON_Z 0x01
#define NUNCHUK_BUTTON_C 0x02
#define NUNCHUK_BUTTON_ALL 0x03

#define CLASSIC_CTRL_BUTTON_UP 0x0001
#define CLASSIC_CTRL_BUTTON_LEFT 0x0002
#define CLASSIC_CTRL_BUTTON_ZR 0x0004
#define CLASSIC_CTRL_BUTTON_X 0x0008
#define CLASSIC_CTRL_BUTTON_A 0x0010
#define CLASSIC_CTRL_BUTTON_Y 0x0020
#define CLASSIC_CTRL_BUTTON_B 0x0040
#define CLASSIC_CTRL_BUTTON_ZL 0x0080
#define CLASSIC_CTRL_BUTTON_FULL_R 0x0200
#define CLASSIC_CTRL_BUTTON_PLUS 0x0400
#define CLASSIC_CTRL_BUTTON_HOME 0x0800
#define CLASSIC_CTRL_BUTTON_MINUS 0x1000
#define CLASSIC_CTRL_BUTTON_FULL_L 0x2000
#define CLASSIC_CTRL_BUTTON_DOWN 0x4000
#define CLASSIC_CTRL_BUTTON_RIGHT 0x8000
#define CLASSIC_CTRL_BUTTON_ALL 0xFEFF

#define GUITAR_HERO_3_BUTTON_STRUM_UP	0x0001
#define GUITAR_HERO_3_BUTTON_YELLOW 0x0008
#define GUITAR_HERO_3_BUTTON_GREEN 0x0010
#define GUITAR_HERO_3_BUTTON_BLUE 0x0020
#define GUITAR_HERO_3_BUTTON_RED 0x0040
#define GUITAR_HERO_3_BUTTON_ORANGE 0x0080
#define GUITAR_HERO_3_BUTTON_PLUS 0x0400
#define GUITAR_HERO_3_BUTTON_MINUS 0x1000
#define GUITAR_HERO_3_BUTTON_STRUM_DOWN 0x4000
#define GUITAR_HERO_3_BUTTON_ALL 0xFEFF

#define WIIUSE_SMOOTHING 0x01
#define WIIUSE_CONTINUOUS 0x02
#define WIIUSE_ORIENT_THRESH 0x04
#define WIIUSE_INIT_FLAGS (WIIUSE_SMOOTHING | WIIUSE_ORIENT_THRESH)

#define WIIUSE_ORIENT_PRECISION 100.0f

#define EXP_NONE 0
#define EXP_NUNCHUK 1
#define EXP_CLASSIC 2
#define EXP_GUITAR_HERO_3 3

typedef enum ir_position_t {
	WIIUSE_IR_ABOVE,
	WIIUSE_IR_BELOW
} ir_position_t;

#define IS_PRESSED(dev, button) ((dev->btns & button) == button)
#define IS_HELD(dev, button) ((dev->btns_held & button) == button)
#define IS_RELEASED(dev, button) ((dev->btns_released & button) == button)
#define IS_JUST_PRESSED(dev, button) \
	(IS_PRESSED(dev, button) && !IS_HELD(dev, button))

#define WIIUSE_GET_IR_SENSITIVITY(dev, lvl) \
	do { \
		if ((wm->state & 0x0200) == 0x0200) \
			*lvl = 1; \
		else if ((wm->state & 0x0400) == 0x0400) \
			*lvl = 2; \
		else if ((wm->state & 0x0800) == 0x0800) \
			*lvl = 3; \
		else if ((wm->state & 0x1000) == 0x1000) \
			*lvl = 4; \
		else if ((wm->state & 0x2000) == 0x2000) \
			*lvl = 5; \
		else *lvl = 0; \
	} while (0)

#define WIIUSE_USING_ACC(wm) ((wm->state & 0x020) == 0x020)
#define WIIUSE_USING_EXP(wm) ((wm->state & 0x040) == 0x040)
#define WIIUSE_USING_IR(wm) ((wm->state & 0x080) == 0x080)
#define WIIUSE_USING_SPEAKER(wm) ((wm->state & 0x100) == 0x100)

#define WIIUSE_IS_LED_SET(wm, num) \
	((wm->leds & WIIMOTE_LED_##num) == WIIMOTE_LED_##num)

#define MAX_PAYLOAD 32

typedef unsigned char byte;
typedef char sbyte;

struct wiimote_t;
struct vec3b_t;
struct orient_t;
struct gforce_t;

typedef void (*wiiuse_read_cb)(struct wiimote_t *wm, byte *data,
							unsigned short len);

struct read_req_t {
	wiiuse_read_cb cb;
	byte* buf;
	unsigned int addr;
	unsigned short size;
	unsigned short wait;
	byte dirty;

	struct read_req_t* next;
};

typedef struct vec2b_t {
	byte x, y;
} vec2b_t;

typedef struct vec3b_t {
	byte x, y, z;
} vec3b_t;

typedef struct vec3f_t {
	float x, y, z;
} vec3f_t;

typedef struct orient_t {
	float roll;
	float pitch;
	float yaw;

	float a_roll;
	float a_pitch;
} orient_t;

typedef struct gforce_t {
	float x, y, z;
} gforce_t;

typedef struct accel_t {
	struct vec3b_t cal_zero;
	struct vec3b_t cal_g;

	float st_roll;
	float st_pitch;
	float st_alpha;
} accel_t;

typedef struct ir_dot_t {
	byte visible;

	unsigned int x;
	unsigned int y;

	short rx;
	short ry;

	byte order;
	byte size;
} ir_dot_t;

typedef enum aspect_t {
	WIIUSE_ASPECT_4_3,
	WIIUSE_ASPECT_16_9
} aspect_t;

typedef struct ir_t {
	struct ir_dot_t dot[4];
	byte num_dots;

	enum aspect_t aspect;

	enum ir_position_t pos;

	unsigned int vres[2];
	int offset[2];
	int state;

	int ax;
	int ay;

	int x;
	int y;

	float distance;
	float z;
} ir_t;

typedef struct joystick_t {
	struct vec2b_t max;
	struct vec2b_t min;
	struct vec2b_t center;

	float ang;
	float mag;
} joystick_t;

typedef struct nunchuk_t {
	struct accel_t accel_calib;
	struct joystick_t js;

	int *flags;

	byte btns;
	byte btns_held;
	byte btns_released;

	float orient_threshold;
	int accel_threshold;

	struct vec3b_t accel;
	struct orient_t orient;
	struct gforce_t gforce;
} nunchuk_t;

typedef struct classic_ctrl_t {
	short btns;
	short btns_held;
	short btns_released;

	float r_shoulder;
	float l_shoulder;

	struct joystick_t ljs;
	struct joystick_t rjs;
} classic_ctrl_t;

typedef struct guitar_hero_3_t {
	short btns;
	short btns_held;
	short btns_released;

	float whammy_bar;

	struct joystick_t js;
} guitar_hero_3_t;

typedef struct expansion_t {
	int type;

	union {
		struct nunchuk_t nunchuk;
		struct classic_ctrl_t classic;
		struct guitar_hero_3_t gh3;
	};
} expansion_t;

typedef enum win_bt_stack_t {
	WIIUSE_STACK_UNKNOWN,
	WIIUSE_STACK_MS,
	WIIUSE_STACK_BLUESOLEIL
} win_bt_stack_t;

typedef struct wiimote_state_t {
	float exp_ljs_ang;
	float exp_rjs_ang;
	float exp_ljs_mag;
	float exp_rjs_mag;
	unsigned short exp_btns;
	struct orient_t exp_orient;
	struct vec3b_t exp_accel;
	float exp_r_shoulder;
	float exp_l_shoulder;

	int ir_ax;
	int ir_ay;
	float ir_distance;

	struct orient_t orient;
	unsigned short btns;

	struct vec3b_t accel;
} wiimote_state_t;

typedef enum WIIUSE_EVENT_TYPE {
	WIIUSE_NONE = 0,
	WIIUSE_EVENT,
	WIIUSE_STATUS,
	WIIUSE_CONNECT,
	WIIUSE_DISCONNECT,
	WIIUSE_UNEXPECTED_DISCONNECT,
	WIIUSE_READ_DATA,
	WIIUSE_NUNCHUK_INSERTED,
	WIIUSE_NUNCHUK_REMOVED,
	WIIUSE_CLASSIC_CTRL_INSERTED,
	WIIUSE_CLASSIC_CTRL_REMOVED,
	WIIUSE_GUITAR_HERO_3_CTRL_INSERTED,
	WIIUSE_GUITAR_HERO_3_CTRL_REMOVED
} WIIUSE_EVENT_TYPE;

typedef struct wiimote_t {
	int unid;

	bdaddr_t bdaddr;
	char bdaddr_str[18];
	int out_sock;
	int in_sock;

	int state;
	byte leds;
	float battery_level;

	int flags;

	byte handshake_state;

	struct read_req_t *read_req;
	struct accel_t accel_calib;
	struct expansion_t exp;

	struct vec3b_t accel;
	struct orient_t orient;
	struct gforce_t gforce;

	struct ir_t ir;

	unsigned short btns;
	unsigned short btns_held;
	unsigned short btns_released;

	float orient_threshold;
	int accel_threshold;

	struct wiimote_state_t lstate;

	WIIUSE_EVENT_TYPE event;
	byte event_buf[MAX_PAYLOAD];
} wiimote;

/* wiiuse.c */
const char *wiiuse_version();
struct wiimote_t **wiiuse_init(int wiimotes);
void wiiuse_disconnected(struct wiimote_t *wm);
void wiiuse_cleanup(struct wiimote_t **wm, int wiimotes);

void wiiuse_rumble(struct wiimote_t *wm, int status);
void wiiuse_toggle_rumble(struct wiimote_t *wm);
void wiiuse_set_leds(struct wiimote_t *wm, int leds);
void wiiuse_motion_sensing(struct wiimote_t *wm, int status);
int wiiuse_read_data(struct wiimote_t *wm, byte *buffer, unsigned int offset,
							unsigned short len);
int wiiuse_write_data(struct wiimote_t *wm, unsigned int addr, byte *data,
								byte len);
void wiiuse_status(struct wiimote_t *wm);
struct wiimote_t *wiiuse_get_by_id(struct wiimote_t **wm, int wiimotes,
								int unid);
int wiiuse_set_flags(struct wiimote_t *wm, int enable, int disable);
float wiiuse_set_smooth_alpha(struct wiimote_t *wm, float alpha);
void wiiuse_set_bluetooth_stack(struct wiimote_t **wm, int wiimotes,
						enum win_bt_stack_t type);
void wiiuse_set_orient_threshold(struct wiimote_t *wm, float threshold);
void wiiuse_resync(struct wiimote_t *wm);
void wiiuse_set_timeout(struct wiimote_t **wm, int wiimotes,
					byte normal_timeout, byte exp_timeout);
void wiiuse_set_accel_threshold(struct wiimote_t *wm, int threshold);

/* connect.c */
int wiiuse_find(struct wiimote_t **wm, int max_wiimotes, int timeout);
int wiiuse_connect(struct wiimote_t **wm, int wiimotes);
void wiiuse_disconnect(struct wiimote_t *wm);

/* events.c */
int wiiuse_poll(struct wiimote_t **wm, int wiimotes);

/* ir.c */
void wiiuse_set_ir(struct wiimote_t *wm, int status);
void wiiuse_set_ir_vres(struct wiimote_t *wm, unsigned int x, unsigned int y);
void wiiuse_set_ir_position(struct wiimote_t *wm, enum ir_position_t pos);
void wiiuse_set_aspect_ratio(struct wiimote_t *wm, enum aspect_t aspect);
void wiiuse_set_ir_sensitivity(struct wiimote_t *wm, int level);

/* nunchuk.c */
void wiiuse_set_nunchuk_orient_threshold(struct wiimote_t *wm, float threshold);
void wiiuse_set_nunchuk_accel_threshold(struct wiimote_t *wm, int threshold);

#ifdef __cplusplus
}
#endif

#endif /* XWII_WIIUSE_H */
