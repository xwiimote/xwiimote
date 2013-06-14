/*
 * XWiimote - tools - xwiishow
 * Written 2010-2013 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * Interactive Wiimote Testing Tool
 * If you run this tool without arguments, then it shows usage information. If
 * you pass "list" as first argument, it lists all connected Wii Remotes.
 * You need to pass one path as argument and the given wiimote is opened and
 * printed to the screen. When wiimote events are received, then the screen is
 * updated correspondingly. You can use the keyboard to control the wiimote.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <math.h>
#include <ncurses.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "xwiimote.h"

enum window_mode {
	MODE_ERROR,
	MODE_NORMAL,
	MODE_EXTENDED,
};

static struct xwii_iface *iface;
static unsigned int mode = MODE_ERROR;
static bool freeze = false;

/* error messages */

static void print_info(const char *format, ...)
{
	va_list list;
	char str[58 + 1];

	va_start(list, format);
	vsnprintf(str, sizeof(str), format, list);
	str[sizeof(str) - 1] = 0;
	va_end(list);

	mvprintw(22, 22, "                                                          ");
	mvprintw(22, 22, "%s", str);
}

static void print_error(const char *format, ...)
{
	va_list list;
	char str[58 + 80 + 1];

	va_start(list, format);
	vsnprintf(str, sizeof(str), format, list);
	if (mode == MODE_EXTENDED)
		str[sizeof(str) - 1] = 0;
	else
		str[58] = 0;
	va_end(list);

	mvprintw(23, 22, "                                                          ");
	if (mode == MODE_EXTENDED)
		mvprintw(23, 80, "                                                                                ");
	mvprintw(23, 22, "%s", str);
}

/* key events */

static void key_show(const struct xwii_event *event)
{
	unsigned int code = event->v.key.code;
	bool pressed = event->v.key.state;
	char *str = NULL;

	if (pressed)
		str = "X";
	else
		str = " ";

	if (code == XWII_KEY_LEFT) {
		mvprintw(4, 7, "%s", str);
	} else if (code == XWII_KEY_RIGHT) {
		mvprintw(4, 11, "%s", str);
	} else if (code == XWII_KEY_UP) {
		mvprintw(2, 9, "%s", str);
	} else if (code == XWII_KEY_DOWN) {
		mvprintw(6, 9, "%s", str);
	} else if (code == XWII_KEY_A) {
		if (pressed)
			str = "A";
		mvprintw(10, 5, "%s", str);
	} else if (code == XWII_KEY_B) {
		if (pressed)
			str = "B";
		mvprintw(10, 13, "%s", str);
	} else if (code == XWII_KEY_HOME) {
		if (pressed)
			str = "HOME+";
		else
			str = "     ";
		mvprintw(13, 7, "%s", str);
	} else if (code == XWII_KEY_MINUS) {
		if (pressed)
			str = "-";
		mvprintw(13, 3, "%s", str);
	} else if (code == XWII_KEY_PLUS) {
		if (pressed)
			str = "+";
		mvprintw(13, 15, "%s", str);
	} else if (code == XWII_KEY_ONE) {
		if (pressed)
			str = "1";
		mvprintw(20, 9, "%s", str);
	} else if (code == XWII_KEY_TWO) {
		if (pressed)
			str = "2";
		mvprintw(21, 9, "%s", str);
	}
}

static void key_clear(void)
{
	struct xwii_event ev;
	unsigned int i;

	ev.type = XWII_EVENT_KEY;
	for (i = 0; i < XWII_KEY_NUM; ++i) {
		ev.v.key.code = i;
		ev.v.key.state = 0;
		key_show(&ev);
	}
}

static void key_toggle(void)
{
	int ret;

	if (xwii_iface_opened(iface) & XWII_IFACE_CORE) {
		xwii_iface_close(iface, XWII_IFACE_CORE);
		key_clear();
		print_info("Info: Disable key events");
	} else {
		ret = xwii_iface_open(iface, XWII_IFACE_CORE |
					     XWII_IFACE_WRITABLE);
		if (ret)
			print_error("Error: Cannot enable key events: %d", ret);
		else
			print_error("Info: Enable key events");
	}
}

/* accelerometer events */

static void accel_show_ext_x(double val)
{
	if (val < -10)
		mvprintw(6, 81, "<=========##          ");
	else if (val < -9)
		mvprintw(6, 81, " <========##          ");
	else if (val < -8)
		mvprintw(6, 81, "  <=======##          ");
	else if (val < -7)
		mvprintw(6, 81, "   <======##          ");
	else if (val < -6)
		mvprintw(6, 81, "    <=====##          ");
	else if (val < -5)
		mvprintw(6, 81, "     <====##          ");
	else if (val < -4)
		mvprintw(6, 81, "      <===##          ");
	else if (val < -3)
		mvprintw(6, 81, "       <==##          ");
	else if (val < -2)
		mvprintw(6, 81, "        <=##          ");
	else if (val < -0.3)
		mvprintw(6, 81, "         <##          ");
	else if (val < 0.3)
		mvprintw(6, 81, "          ##          ");
	else if (val < 2)
		mvprintw(6, 81, "          ##>         ");
	else if (val < 3)
		mvprintw(6, 81, "          ##=>        ");
	else if (val < 4)
		mvprintw(6, 81, "          ##==>       ");
	else if (val < 5)
		mvprintw(6, 81, "          ##===>      ");
	else if (val < 6)
		mvprintw(6, 81, "          ##====>     ");
	else if (val < 7)
		mvprintw(6, 81, "          ##=====>    ");
	else if (val < 8)
		mvprintw(6, 81, "          ##======>   ");
	else if (val < 9)
		mvprintw(6, 81, "          ##=======>  ");
	else if (val < 10)
		mvprintw(6, 81, "          ##========> ");
	else
		mvprintw(6, 81, "          ##=========>");
}

static void accel_show_ext_y(double val)
{

	if (val > 5) {
		mvprintw(1,  93, "   __.");
		mvprintw(2,  93, "   //|");
		mvprintw(3,  93, "  // ");
		mvprintw(4,  93, " // ");
		mvprintw(5,  93, "// ");
		mvprintw(7,  86, "     ");
		mvprintw(8,  86, "     ");
		mvprintw(9,  86, "    ");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > 4) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "  __. ");
		mvprintw(3,  93, "  //|");
		mvprintw(4,  93, " // ");
		mvprintw(5,  93, "// ");
		mvprintw(7,  86, "     ");
		mvprintw(8,  86, "     ");
		mvprintw(9,  86, "    ");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > 3) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, " __. ");
		mvprintw(4,  93, " //|");
		mvprintw(5,  93, "// ");
		mvprintw(7,  86, "     ");
		mvprintw(8,  86, "     ");
		mvprintw(9,  86, "    ");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > 2) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, "     ");
		mvprintw(4,  93, "__. ");
		mvprintw(5,  93, "//|");
		mvprintw(7,  86, "     ");
		mvprintw(8,  86, "     ");
		mvprintw(9,  86, "    ");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > 0.3) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, "     ");
		mvprintw(4,  93, "    ");
		mvprintw(5,  93, "-. ");
		mvprintw(7,  86, "     ");
		mvprintw(8,  86, "     ");
		mvprintw(9,  86, "    ");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > -0.3) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, "     ");
		mvprintw(4,  93, "    ");
		mvprintw(5,  93, "   ");
		mvprintw(7,  86, "     ");
		mvprintw(8,  86, "     ");
		mvprintw(9,  86, "    ");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > -2) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, "     ");
		mvprintw(4,  93, "    ");
		mvprintw(5,  93, "   ");
		mvprintw(7,  86, "    *");
		mvprintw(8,  86, "     ");
		mvprintw(9,  86, "    ");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > -3) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, "     ");
		mvprintw(4,  93, "    ");
		mvprintw(5,  93, "   ");
		mvprintw(7,  86, "   |//");
		mvprintw(8,  86, "   *-");
		mvprintw(9,  86, "    ");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > -4) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, "     ");
		mvprintw(4,  93, "    ");
		mvprintw(5,  93, "   ");
		mvprintw(7,  86, "    //");
		mvprintw(8,  86, "  |//");
		mvprintw(9,  86, "  *-");
		mvprintw(10, 86, "   ");
		mvprintw(11, 86, "  ");
	} else if (val > -5) {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, "     ");
		mvprintw(4,  93, "    ");
		mvprintw(5,  93, "   ");
		mvprintw(7,  86, "    //");
		mvprintw(8,  86, "   //");
		mvprintw(9,  86, " |//");
		mvprintw(10, 86, " *-");
		mvprintw(11, 86, "  ");
	} else {
		mvprintw(1,  93, "      ");
		mvprintw(2,  93, "      ");
		mvprintw(3,  93, "     ");
		mvprintw(4,  93, "    ");
		mvprintw(5,  93, "   ");
		mvprintw(7,  86, "    //");
		mvprintw(8,  86, "   //");
		mvprintw(9,  86, "  //");
		mvprintw(10, 86, "|//");
		mvprintw(11, 86, "*-");
	}
}

static void accel_show_ext_z(double val)
{
	if (val < -5) {
		mvprintw(1, 91, "/\\");
		mvprintw(2, 91, "||");
		mvprintw(3, 91, "||");
		mvprintw(4, 91, "||");
		mvprintw(5, 91, "||");
		mvprintw(7,  91, "  ");
		mvprintw(8,  91, "  ");
		mvprintw(9,  91, "  ");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < -4) {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "/\\");
		mvprintw(3, 91, "||");
		mvprintw(4, 91, "||");
		mvprintw(5, 91, "||");
		mvprintw(7,  91, "  ");
		mvprintw(8,  91, "  ");
		mvprintw(9,  91, "  ");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < -3) {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "/\\");
		mvprintw(4, 91, "||");
		mvprintw(5, 91, "||");
		mvprintw(7,  91, "  ");
		mvprintw(8,  91, "  ");
		mvprintw(9,  91, "  ");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < -2) {
		mvprintw(5, 91, "  ");
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "  ");
		mvprintw(4, 91, "/\\");
		mvprintw(5, 91, "||");
		mvprintw(7,  91, "  ");
		mvprintw(8,  91, "  ");
		mvprintw(9,  91, "  ");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < -0.3) {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "  ");
		mvprintw(4, 91, "  ");
		mvprintw(5, 91, "/\\");
		mvprintw(7,  91, "  ");
		mvprintw(8,  91, "  ");
		mvprintw(9,  91, "  ");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < 0.3) {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "  ");
		mvprintw(4, 91, "  ");
		mvprintw(5, 91, "  ");
		mvprintw(7,  91, "  ");
		mvprintw(8,  91, "  ");
		mvprintw(9,  91, "  ");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < 2) {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "  ");
		mvprintw(4, 91, "  ");
		mvprintw(5, 91, "  ");
		mvprintw(7,  91, "\\/");
		mvprintw(8,  91, "  ");
		mvprintw(9,  91, "  ");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < 3) {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "  ");
		mvprintw(4, 91, "  ");
		mvprintw(5, 91, "  ");
		mvprintw(7,  91, "||");
		mvprintw(8,  91, "\\/");
		mvprintw(9,  91, "  ");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < 4) {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "  ");
		mvprintw(4, 91, "  ");
		mvprintw(5, 91, "  ");
		mvprintw(7,  91, "||");
		mvprintw(8,  91, "||");
		mvprintw(9,  91, "\\/");
		mvprintw(10, 91, "  ");
		mvprintw(11, 91, "  ");
	} else if (val < 5) {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "  ");
		mvprintw(4, 91, "  ");
		mvprintw(5, 91, "  ");
		mvprintw(7,  91, "||");
		mvprintw(8,  91, "||");
		mvprintw(9,  91, "||");
		mvprintw(10, 91, "\\/");
		mvprintw(11, 91, "  ");
	} else {
		mvprintw(1, 91, "  ");
		mvprintw(2, 91, "  ");
		mvprintw(3, 91, "  ");
		mvprintw(4, 91, "  ");
		mvprintw(5, 91, "  ");
		mvprintw(7,  91, "||");
		mvprintw(8,  91, "||");
		mvprintw(9,  91, "||");
		mvprintw(10, 91, "||");
		mvprintw(11, 91, "\\/");
	}
}

static void accel_show_ext(const struct xwii_event *event)
{
	double val;

	/* pow(val, 1/4) for smoother interpolation around the origin */

	val = event->v.abs[0].x;
	val /= 512;
	if (val >= 0)
		val = 10 * pow(val, 0.25);
	else
		val = -10 * pow(-val, 0.25);
	accel_show_ext_x(val);

	val = event->v.abs[0].z;
	val /= 512;
	if (val >= 0)
		val = 5 * pow(val, 0.25);
	else
		val = -5 * pow(-val, 0.25);
	accel_show_ext_z(val);

	val = event->v.abs[0].y;
	val /= 512;
	if (val >= 0)
		val = 5 * pow(val, 0.25);
	else
		val = -5 * pow(-val, 0.25);
	accel_show_ext_y(val);
}

static void accel_show(const struct xwii_event *event)
{
	mvprintw(1, 39, "%5" PRId32, event->v.abs[0].x);
	mvprintw(1, 48, "%5" PRId32, event->v.abs[0].y);
	mvprintw(1, 57, "%5" PRId32, event->v.abs[0].z);
}

static void accel_clear(void)
{
	struct xwii_event ev;

	ev.v.abs[0].x = 0;
	ev.v.abs[0].y = 0;
	ev.v.abs[0].z = 0;
	accel_show_ext(&ev);
	accel_show(&ev);
}

static void accel_toggle(void)
{
	if (xwii_iface_opened(iface) & XWII_IFACE_ACCEL) {
		xwii_iface_close(iface, XWII_IFACE_ACCEL);
		accel_clear();
		print_info("Info: Disable accelerometer");
	} else {
		xwii_iface_open(iface, XWII_IFACE_ACCEL);
		print_info("Info: Enable accelerometer");
	}
}

/* IR events */

static void ir_show_ext(const struct xwii_event *event)
{
	double v;
	uint64_t x[4], y[4], i, j, num;
	char c;

	mvprintw(1,  106, "                          |                          ");
	mvprintw(2,  106, "                                                     ");
	mvprintw(3,  106, "                          |                          ");
	mvprintw(4,  106, "                                                     ");
	mvprintw(5,  106, "                          |                          ");
	mvprintw(6,  106, "- - - - - - - - - - - - - + - - - - - - - - - - - - -");
	mvprintw(7,  106, "                          |                          ");
	mvprintw(8,  106, "                                                     ");
	mvprintw(9,  106, "                          |                          ");
	mvprintw(10, 106, "                                                     ");
	mvprintw(11, 106, "                          |                          ");

	for (i = 0; i < 4; ++i) {
		v = event->v.abs[i].x;
		v *= 52;
		v /= 1024;
		v += 0.5;
		x[i] = v;

		v = event->v.abs[i].y;
		v *= 10;
		v /= 768;
		v += 0.5;
		y[i] = v;
	}

	for (i = 0; i < 4; ++i) {
		if (!xwii_event_ir_is_valid(&event->v.abs[i]))
			continue;

		num = 0;
		for (j = 0; j < 4; ++j) {
			if (x[j] == x[i] && y[j] == y[i])
				++num;
		}

		if (num > 1)
			c = '#';
		else if (i == 0)
			c = 'x';
		else if (i == 1)
			c = '+';
		else if (i == 2)
			c = '*';
		else
			c = '-';

		mvprintw(1 + y[i], 106 + x[i], "%c", c);
	}
}

static void ir_show(const struct xwii_event *event)
{
	if (xwii_event_ir_is_valid(&event->v.abs[0])) {
		mvprintw(3, 27, "%04" PRId32, event->v.abs[0].x);
		mvprintw(3, 32, "%04" PRId32, event->v.abs[0].y);
	} else {
		mvprintw(3, 27, "N/A ");
		mvprintw(3, 32, " N/A");
	}

	if (xwii_event_ir_is_valid(&event->v.abs[1])) {
		mvprintw(3, 41, "%04" PRId32, event->v.abs[1].x);
		mvprintw(3, 46, "%04" PRId32, event->v.abs[1].y);
	} else {
		mvprintw(3, 41, "N/A ");
		mvprintw(3, 46, " N/A");
	}

	if (xwii_event_ir_is_valid(&event->v.abs[2])) {
		mvprintw(3, 55, "%04" PRId32, event->v.abs[2].x);
		mvprintw(3, 60, "%04" PRId32, event->v.abs[2].y);
	} else {
		mvprintw(3, 55, "N/A ");
		mvprintw(3, 60, " N/A");
	}

	if (xwii_event_ir_is_valid(&event->v.abs[3])) {
		mvprintw(3, 69, "%04" PRId32, event->v.abs[3].x);
		mvprintw(3, 74, "%04" PRId32, event->v.abs[3].y);
	} else {
		mvprintw(3, 69, "N/A ");
		mvprintw(3, 74, " N/A");
	}
}

static void ir_clear(void)
{
	struct xwii_event ev;

	ev.v.abs[0].x = 1023;
	ev.v.abs[0].y = 1023;
	ev.v.abs[1].x = 1023;
	ev.v.abs[1].y = 1023;
	ev.v.abs[2].x = 1023;
	ev.v.abs[2].y = 1023;
	ev.v.abs[3].x = 1023;
	ev.v.abs[3].y = 1023;
	ir_show_ext(&ev);
	ir_show(&ev);
}

static void ir_toggle(void)
{
	if (xwii_iface_opened(iface) & XWII_IFACE_IR) {
		xwii_iface_close(iface, XWII_IFACE_IR);
		ir_clear();
		print_info("Info: Disable IR");
	} else {
		xwii_iface_open(iface, XWII_IFACE_IR);
		print_info("Info: Enable IR");
	}
}

/* motion plus */

static bool mp_do_refresh;

static void mp_show(const struct xwii_event *event)
{
	int32_t x, y, z, factor;

	if (mp_do_refresh) {
		xwii_iface_get_mp_normalization(iface, &x, &y, &z, &factor);
		x = event->v.abs[0].x + x;
		y = event->v.abs[0].y + y;
		z = event->v.abs[0].z + z;
		xwii_iface_set_mp_normalization(iface, x, y, z, factor);

		/* try to stabilize calibration as MP tends to report huge
		 * values during initialization for 1-2s. */
		if (x < 5000 && y < 5000 && z < 5000)
			mp_do_refresh = false;
	}

	x = event->v.abs[0].x;
	y = event->v.abs[0].y;
	z = event->v.abs[0].z;

	mvprintw(5, 25, " %6d", (int16_t)x);
	mvprintw(5, 35, " %6d", (int16_t)y);
	mvprintw(5, 45, " %6d", (int16_t)z);
}

static void mp_clear(void)
{
	struct xwii_event ev;

	ev.v.abs[0].x = 0;
	ev.v.abs[0].y = 0;
	ev.v.abs[0].z = 0;
	mp_show(&ev);
}

static void mp_toggle(void)
{
	if (xwii_iface_opened(iface) & XWII_IFACE_MOTION_PLUS) {
		xwii_iface_close(iface, XWII_IFACE_MOTION_PLUS);
		mp_clear();
		print_info("Info: Disable Motion Plus");
	} else {
		xwii_iface_open(iface, XWII_IFACE_MOTION_PLUS);
		print_info("Info: Enable Motion Plus");
	}
}

static void mp_normalization_toggle(void)
{
	int32_t x, y, z, factor;

	xwii_iface_get_mp_normalization(iface, &x, &y, &z, &factor);
	if (!factor) {
		xwii_iface_set_mp_normalization(iface, x, y, z, 50);
		print_info("Info: Enable MP Norm: (%i:%i:%i)",
			    (int)x, (int)y, (int)z);
	} else {
		xwii_iface_set_mp_normalization(iface, x, y, z, 0);
		print_info("Info: Disable MP Norm: (%i:%i:%i)",
			    (int)x, (int)y, (int)z);
	}
}

static void mp_refresh(void)
{
	mp_do_refresh = true;
}

/* balance board */

static void bboard_show_ext(const struct xwii_event *event)
{
	uint16_t w, x, y, z;

	w = event->v.abs[0].x;
	x = event->v.abs[1].x;
	y = event->v.abs[2].x;
	z = event->v.abs[3].x;

	mvprintw(17, 85, " %5d", y);
	mvprintw(17, 96, " %5d", w);
	mvprintw(20, 85, " %5d", z);
	mvprintw(20, 96, " %5d", x);
	mvprintw(13, 86, " %5d", w + x + y + z);
}

static void bboard_clear(void)
{
	struct xwii_event ev;

	ev.v.abs[0].x = 0;
	ev.v.abs[1].x = 0;
	ev.v.abs[2].x = 0;
	ev.v.abs[3].x = 0;
	bboard_show_ext(&ev);
}

static void bboard_toggle(void)
{
	if (xwii_iface_opened(iface) & XWII_IFACE_BALANCE_BOARD) {
		xwii_iface_close(iface, XWII_IFACE_BALANCE_BOARD);
		bboard_clear();
		print_info("Info: Disable Balance Board");
	} else {
		xwii_iface_open(iface, XWII_IFACE_BALANCE_BOARD);
		print_info("Info: Enable Balance Board");
	}
}

/* pro controller */

static void pro_show_ext(const struct xwii_event *event)
{
	uint16_t code = event->v.key.code;
	int32_t v;
	bool pressed = event->v.key.state;
	char *str = NULL;

	if (event->type == XWII_EVENT_PRO_CONTROLLER_MOVE) {
		v = event->v.abs[0].x;
		mvprintw(14, 116, "%5d", v);
		if (v > 1000) {
			mvprintw(16, 118, "     ");
			mvprintw(16, 124, "#####");
		} else if (v > 800) {
			mvprintw(16, 118, "     ");
			mvprintw(16, 124, "#### ");
		} else if (v > 600) {
			mvprintw(16, 118, "     ");
			mvprintw(16, 124, "###  ");
		} else if (v > 400) {
			mvprintw(16, 118, "     ");
			mvprintw(16, 124, "##   ");
		} else if (v > 200) {
			mvprintw(16, 118, "     ");
			mvprintw(16, 124, "#    ");
		} else if (v > -200) {
			mvprintw(16, 118, "     ");
			mvprintw(16, 124, "     ");
		} else if (v > -400) {
			mvprintw(16, 118, "    #");
			mvprintw(16, 124, "     ");
		} else if (v > -600) {
			mvprintw(16, 118, "   ##");
			mvprintw(16, 124, "     ");
		} else if (v > -800) {
			mvprintw(16, 118, "  ###");
			mvprintw(16, 124, "     ");
		} else if (v > -1000) {
			mvprintw(16, 118, " ####");
			mvprintw(16, 124, "     ");
		} else {
			mvprintw(16, 118, "#####");
			mvprintw(16, 124, "     ");
		}

		v = event->v.abs[0].y;
		mvprintw(14, 125, "%5d", v);
		if (v > 1000) {
			mvprintw(14, 123, "#");
			mvprintw(15, 123, "#");
			mvprintw(17, 123, " ");
			mvprintw(18, 123, " ");
		} else if (v > 200) {
			mvprintw(14, 123, " ");
			mvprintw(15, 123, "#");
			mvprintw(17, 123, " ");
			mvprintw(18, 123, " ");
		} else if (v > -200) {
			mvprintw(14, 123, " ");
			mvprintw(15, 123, " ");
			mvprintw(17, 123, " ");
			mvprintw(18, 123, " ");
		} else if (v > -1000) {
			mvprintw(14, 123, " ");
			mvprintw(15, 123, " ");
			mvprintw(17, 123, "#");
			mvprintw(18, 123, " ");
		} else {
			mvprintw(14, 123, " ");
			mvprintw(15, 123, " ");
			mvprintw(17, 123, "#");
			mvprintw(18, 123, "#");
		}

		v = event->v.abs[1].x;
		mvprintw(14, 134, "%5d", v);
		if (v > 1000) {
			mvprintw(16, 136, "     ");
			mvprintw(16, 142, "#####");
		} else if (v > 800) {
			mvprintw(16, 136, "     ");
			mvprintw(16, 142, "#### ");
		} else if (v > 600) {
			mvprintw(16, 136, "     ");
			mvprintw(16, 142, "###  ");
		} else if (v > 400) {
			mvprintw(16, 136, "     ");
			mvprintw(16, 142, "##   ");
		} else if (v > 200) {
			mvprintw(16, 136, "     ");
			mvprintw(16, 142, "#    ");
		} else if (v > -200) {
			mvprintw(16, 136, "     ");
			mvprintw(16, 142, "     ");
		} else if (v > -400) {
			mvprintw(16, 136, "    #");
			mvprintw(16, 142, "     ");
		} else if (v > -600) {
			mvprintw(16, 136, "   ##");
			mvprintw(16, 142, "     ");
		} else if (v > -800) {
			mvprintw(16, 136, "  ###");
			mvprintw(16, 142, "     ");
		} else if (v > -1000) {
			mvprintw(16, 136, " ####");
			mvprintw(16, 142, "     ");
		} else {
			mvprintw(16, 136, "#####");
			mvprintw(16, 142, "     ");
		}

		v = event->v.abs[1].y;
		mvprintw(14, 143, "%5d", v);
		if (v > 1000) {
			mvprintw(14, 141, "#");
			mvprintw(15, 141, "#");
			mvprintw(17, 141, " ");
			mvprintw(18, 141, " ");
		} else if (v > 200) {
			mvprintw(14, 141, " ");
			mvprintw(15, 141, "#");
			mvprintw(17, 141, " ");
			mvprintw(18, 141, " ");
		} else if (v > -200) {
			mvprintw(14, 141, " ");
			mvprintw(15, 141, " ");
			mvprintw(17, 141, " ");
			mvprintw(18, 141, " ");
		} else if (v > -1000) {
			mvprintw(14, 141, " ");
			mvprintw(15, 141, " ");
			mvprintw(17, 141, "#");
			mvprintw(18, 141, " ");
		} else {
			mvprintw(14, 141, " ");
			mvprintw(15, 141, " ");
			mvprintw(17, 141, "#");
			mvprintw(18, 141, "#");
		}
	} else if (event->type == XWII_EVENT_PRO_CONTROLLER_KEY) {
		if (pressed)
			str = "X";
		else
			str = " ";

		if (code == XWII_KEY_A) {
			if (pressed)
				str = "A";
			mvprintw(20, 156, "%s", str);
		} else if (code == XWII_KEY_B) {
			if (pressed)
				str = "B";
			mvprintw(21, 154, "%s", str);
		} else if (code == XWII_KEY_X) {
			if (pressed)
				str = "X";
			mvprintw(19, 154, "%s", str);
		} else if (code == XWII_KEY_Y) {
			if (pressed)
				str = "Y";
			mvprintw(20, 152, "%s", str);
		} else if (code == XWII_KEY_PLUS) {
			if (pressed)
				str = "+";
			mvprintw(21, 142, "%s", str);
		} else if (code == XWII_KEY_MINUS) {
			if (pressed)
				str = "-";
			mvprintw(21, 122, "%s", str);
		} else if (code == XWII_KEY_HOME) {
			if (pressed)
				str = "HOME+";
			else
				str = "     ";
			mvprintw(21, 130, "%s", str);
		} else if (code == XWII_KEY_LEFT) {
			mvprintw(18, 108, "%s", str);
		} else if (code == XWII_KEY_RIGHT) {
			mvprintw(18, 112, "%s", str);
		} else if (code == XWII_KEY_UP) {
			mvprintw(16, 110, "%s", str);
		} else if (code == XWII_KEY_DOWN) {
			mvprintw(20, 110, "%s", str);
		} else if (code == XWII_KEY_TL) {
			if (pressed)
				str = "TL";
			else
				str = "  ";
			mvprintw(14, 108, "%s", str);
		} else if (code == XWII_KEY_TR) {
			if (pressed)
				str = "TR";
			else
				str = "  ";
			mvprintw(14, 155, "%s", str);
		} else if (code == XWII_KEY_ZL) {
			if (pressed)
				str = "ZL";
			else
				str = "  ";
			mvprintw(13, 108, "%s", str);
		} else if (code == XWII_KEY_ZR) {
			if (pressed)
				str = "ZR";
			else
				str = "  ";
			mvprintw(13, 155, "%s", str);
		} else if (code == XWII_KEY_THUMBL) {
			if (!pressed)
				str = "+";
			mvprintw(16, 123, "%s", str);
		} else if (code == XWII_KEY_THUMBR) {
			if (!pressed)
				str = "+";
			mvprintw(16, 141, "%s", str);
		}
	}
}

static void pro_clear(void)
{
	struct xwii_event ev;
	unsigned int i;

	ev.type = XWII_EVENT_PRO_CONTROLLER_MOVE;
	ev.v.abs[0].x = 0;
	ev.v.abs[0].y = 0;
	ev.v.abs[1].x = 0;
	ev.v.abs[1].y = 0;
	pro_show_ext(&ev);

	ev.type = XWII_EVENT_PRO_CONTROLLER_KEY;
	ev.v.key.state = 0;
	for (i = 0; i < XWII_KEY_NUM; ++i) {
		ev.v.key.code = i;
		pro_show_ext(&ev);
	}
}

static void pro_toggle(void)
{
	if (xwii_iface_opened(iface) & XWII_IFACE_PRO_CONTROLLER) {
		xwii_iface_close(iface, XWII_IFACE_PRO_CONTROLLER);
		pro_clear();
		print_info("Info: Disable Pro Controller");
	} else {
		xwii_iface_open(iface, XWII_IFACE_PRO_CONTROLLER);
		print_info("Info: Enable Pro Controller");
	}
}

/* rumble events */

static void rumble_show(bool on)
{
	mvprintw(1, 21, on ? "RUMBLE" : "      ");
}

static void rumble_toggle(void)
{
	static bool on = false;

	on = !on;
	xwii_iface_rumble(iface, on);
	rumble_show(on);
}

/* LEDs */

static bool led1_state;

static void led1_show(bool on)
{
	mvprintw(5, 59, on ? "(#1)" : " -1 ");
}

static void led1_toggle(void)
{
	led1_state = !led1_state;
	xwii_iface_set_led(iface, XWII_LED(1), led1_state);
	led1_show(led1_state);
}

static void led1_refresh(void)
{
	int ret;

	ret = xwii_iface_get_led(iface, XWII_LED(1), &led1_state);
	if (ret)
		print_error("Error: Cannot read LED state");
	else
		led1_show(led1_state);
}

static bool led2_state;

static void led2_show(bool on)
{
	mvprintw(5, 64, on ? "(#2)" : " -2 ");
}

static void led2_toggle(void)
{
	led2_state = !led2_state;
	xwii_iface_set_led(iface, XWII_LED(2), led2_state);
	led2_show(led2_state);
}

static void led2_refresh(void)
{
	int ret;

	ret = xwii_iface_get_led(iface, XWII_LED(2), &led2_state);
	if (ret)
		print_error("Error: Cannot read LED state");
	else
		led2_show(led2_state);
}

static bool led3_state;

static void led3_show(bool on)
{
	mvprintw(5, 69, on ? "(#3)" : " -3 ");
}

static void led3_toggle(void)
{
	led3_state = !led3_state;
	xwii_iface_set_led(iface, XWII_LED(3), led3_state);
	led3_show(led3_state);
}

static void led3_refresh(void)
{
	int ret;

	ret = xwii_iface_get_led(iface, XWII_LED(3), &led3_state);
	if (ret)
		print_error("Error: Cannot read LED state");
	else
		led3_show(led3_state);
}

static bool led4_state;

static void led4_show(bool on)
{
	mvprintw(5, 74, on ? "(#4)" : " -4 ");
}

static void led4_toggle(void)
{
	led4_state = !led4_state;
	xwii_iface_set_led(iface, XWII_LED(4), led4_state);
	led4_show(led4_state);
}

static void led4_refresh(void)
{
	int ret;

	ret = xwii_iface_get_led(iface, XWII_LED(4), &led4_state);
	if (ret)
		print_error("Error: Cannot read LED state");
	else
		led4_show(led4_state);
}

/* battery status */

static void battery_show(uint8_t capacity)
{
	int i;

	mvprintw(7, 29, "%3u%%", capacity);

	mvprintw(7, 35, "          ");
	for (i = 0; i * 10 < capacity; ++i)
		mvprintw(7, 35 + i, "#");
}

static void battery_refresh(void)
{
	int ret;
	uint8_t capacity;

	ret = xwii_iface_get_battery(iface, &capacity);
	if (ret)
		print_error("Error: Cannot read battery capacity");
	else
		battery_show(capacity);
}

/* device type */

static void devtype_refresh(void)
{
	int ret;
	char *name;

	ret = xwii_iface_get_devtype(iface, &name);
	if (ret) {
		print_error("Error: Cannot read device type");
	} else {
		mvprintw(9, 28, "                                                   ");
		mvprintw(9, 28, "%s", name);
		free(name);
	}
}

/* extension type */

static void extension_refresh(void)
{
	int ret;
	char *name;

	ret = xwii_iface_get_extension(iface, &name);
	if (ret) {
		print_error("Error: Cannot read extension type");
	} else {
		mvprintw(7, 54, "                      ");
		mvprintw(7, 54, "%s", name);
		free(name);
	}
}

/* basic window setup */

static void refresh_all(void)
{
	battery_refresh();
	led1_refresh();
	led2_refresh();
	led3_refresh();
	led4_refresh();
	devtype_refresh();
	extension_refresh();
	mp_refresh();

	if (geteuid() != 0)
		mvprintw(20, 22, "Warning: Please run as root! (sysfs+evdev access needed)");
}

static void setup_window(void)
{
	size_t i;

	i = 0;
	/* 80x24 Box */
	mvprintw(i++, 0, "+- Keys ----------+ +------+ +---------------------------------+---------------+");
	mvprintw(i++, 0, "|       +-+       | |      |  Accel x:       y:       z:       | XWIIMOTE SHOW |");
	mvprintw(i++, 0, "|       | |       | +------+ +---------------------------------+---------------+");
	mvprintw(i++, 0, "|     +-+ +-+     | IR #1:     x     #2:     x     #3:     x     #4:     x     |");
	mvprintw(i++, 0, "|     |     |     | +--------------------------------+-------------------------+");
	mvprintw(i++, 0, "|     +-+ +-+     | MP x:        y:        z:        | LED  -0   -1   -2   -3  |");
	mvprintw(i++, 0, "|       | |       | +--------------------------+-----+----------------------+--+");
	mvprintw(i++, 0, "|       +-+       | Battery:      |          | | Ext:                       |  |");
	mvprintw(i++, 0, "|                 | +--------------------------+----------------------------+--+");
	mvprintw(i++, 0, "|   +-+     +-+   | Device:                                                    |");
	mvprintw(i++, 0, "|   | |     | |   | +----------------------------------------------------------+");
	mvprintw(i++, 0, "|   +-+     +-+   |                                                            |");
	mvprintw(i++, 0, "|                 |                                                            |");
	mvprintw(i++, 0, "| ( ) |     | ( ) |                                                            |");
	mvprintw(i++, 0, "|                 |                                                            |");
	mvprintw(i++, 0, "|      +++++      |                                                            |");
	mvprintw(i++, 0, "|      +   +      |                                                            |");
	mvprintw(i++, 0, "|      +   +      |                                                            |");
	mvprintw(i++, 0, "|      +++++      |                                                            |");
	mvprintw(i++, 0, "|                 | +----------------------------------------------------------+");
	mvprintw(i++, 0, "|       | |       | |                                                          |");
	mvprintw(i++, 0, "|       | |       | +----------------------------------------------------------+");
	mvprintw(i++, 0, "|                 | |                                                           ");
	mvprintw(i++, 0, "+-----------------+ |");
}

static void setup_ext_window(void)
{
	size_t i;

	i = 0;
	/* 160x40 Box */
	mvprintw(i++, 80, " +- Accel -------------+ +- IR ---------------------+--------------------------+");
	mvprintw(i++, 80, "                       | |                          |                          |");
	mvprintw(i++, 80, "                    Z  | |                                                     |");
	mvprintw(i++, 80, "                       | |                          |                          |");
	mvprintw(i++, 80, "                       | |                                                     |");
	mvprintw(i++, 80, "                       | |                          |                          |");
	mvprintw(i++, 80, "           ##          | +- - - - - - - - - - - - - + - - - - - - - - - - - - -+");
	mvprintw(i++, 80, " X                     | |                          |                          |");
	mvprintw(i++, 80, "                       | |                                                     |");
	mvprintw(i++, 80, "                       | |                          |                          |");
	mvprintw(i++, 80, "                       | |                                                     |");
	mvprintw(i++, 80, "              Y        | |                          |                          |");
	mvprintw(i++, 80, " +- Balance Board -----+ +- Pro Controller ---------+--------------------------+");
	mvprintw(i++, 80, "  Sum:                 | | |ZL|           +-+               +-+           |ZR| |");
	mvprintw(i++, 80, "                       | | |TL|           | |               | |           |TR| |");
	mvprintw(i++, 80, "            |          | |   +-+     +---     ---+     +---     ---+           |");
	mvprintw(i++, 80, "            |          | |   | |     |     +     |     |     +     |           |");
	mvprintw(i++, 80, "  #1:        #2:       | | +-+ +-+   +---     ---+     +---     ---+           |");
	mvprintw(i++, 80, "            |          | | |     |        | |               | |                |");
	mvprintw(i++, 80, "            |          | | +-+ +-+        +-+               +-+          |X|   |");
	mvprintw(i++, 80, "  #3:        #4:       | |   | |                                       |Y| |A| |");
	mvprintw(i++, 80, "                       | |   +-+         (-)     |HOME+|     (+)         |B|   |");
	mvprintw(i++, 80, " +---------------------+ +-----------------------------------------------------+");

	i = 24;
	mvprintw(i++, 0,  "+-------------------+----------------------------------------------------------+");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "|                                                                              |");
	mvprintw(i++, 0,  "+------------------------------------------------------------------------------+");

	i = 24;
	mvprintw(i++, 80, " +-----------------------------------------------------------------------------+");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, "                                                                               |");
	mvprintw(i++, 80, " +-----------------------------------------------------------------------------+");
}

static void handle_resize(void)
{
	if (LINES < 24 || COLS < 80) {
		mode = MODE_ERROR;
		erase();
		mvprintw(0, 0, "Error: Screen smaller than 80x24; no view");
	} else if (LINES < 40 || COLS < 160) {
		mode = MODE_NORMAL;
		erase();
		setup_window();
		refresh_all();
		print_info("Info: Screen smaller than 160x40; limited view");
	} else {
		mode = MODE_EXTENDED;
		erase();
		setup_ext_window();
		setup_window();
		refresh_all();
		print_info("Info: Screen initialized for extended view");
	}
}

/* device watch events */

static void handle_watch(void)
{
	static unsigned int num;

	print_info("Info: Watch Event #%u", ++num);
	refresh_all();
}

/* keyboard handling */

static void freeze_toggle(void)
{
	freeze = !freeze;
	print_info("Info: %sreeze screen", freeze ? "F" : "Unf");
}

static int keyboard(void)
{
	int key;

	key = getch();
	if (key == ERR)
		return 0;

	switch (key) {
	case KEY_RESIZE:
		handle_resize();
		break;
	case 'k':
		key_toggle();
		break;
	case 'a':
		accel_toggle();
		break;
	case 'i':
		ir_toggle();
		break;
	case 'm':
		mp_toggle();
		break;
	case 'n':
		mp_normalization_toggle();
		break;
	case 'b':
		bboard_toggle();
		break;
	case 'p':
		pro_toggle();
		break;
	case 'r':
		rumble_toggle();
		break;
	case '1':
		led1_toggle();
		break;
	case '2':
		led2_toggle();
		break;
	case '3':
		led3_toggle();
		break;
	case '4':
		led4_toggle();
		break;
	case 'f':
		freeze_toggle();
		break;
	case 's':
		refresh_all();
		break;
	case 'q':
		return -ECANCELED;
	}

	return 0;
}

static int run_iface(struct xwii_iface *iface)
{
	struct xwii_event event;
	int ret = 0;
	struct pollfd fds[2];

	handle_resize();
	key_clear();
	accel_clear();
	ir_clear();
	mp_clear();
	bboard_clear();
	pro_clear();
	refresh_all();
	refresh();

	memset(fds, 0, sizeof(fds));
	fds[0].fd = 0;
	fds[0].events = POLLIN;
	fds[1].fd = xwii_iface_get_fd(iface);
	fds[1].events = POLLIN;

	ret = xwii_iface_watch(iface, true);
	if (ret)
		print_error("Error: Cannot initialize hotplug watch descriptor");

	while (true) {
		ret = poll(fds, 2, -1);
		if (ret < 0) {
			if (errno != EINTR) {
				ret = -errno;
				print_error("Error: Cannot poll fds: %d", ret);
				break;
			}
		}

		ret = xwii_iface_dispatch(iface, &event, sizeof(event));
		if (ret) {
			if (ret != -EAGAIN) {
				print_error("Error: Read failed with err:%d",
					    ret);
				break;
			}
		} else if (!freeze) {
			switch (event.type) {
			case XWII_EVENT_WATCH:
				handle_watch();
				break;
			case XWII_EVENT_KEY:
				if (mode != MODE_ERROR)
					key_show(&event);
				break;
			case XWII_EVENT_ACCEL:
				if (mode == MODE_EXTENDED)
					accel_show_ext(&event);
				if (mode != MODE_ERROR)
					accel_show(&event);
				break;
			case XWII_EVENT_IR:
				if (mode == MODE_EXTENDED)
					ir_show_ext(&event);
				if (mode != MODE_ERROR)
					ir_show(&event);
				break;
			case XWII_EVENT_MOTION_PLUS:
				if (mode != MODE_ERROR)
					mp_show(&event);
				break;
			case XWII_EVENT_BALANCE_BOARD:
				if (mode == MODE_EXTENDED)
					bboard_show_ext(&event);
				break;
			case XWII_EVENT_PRO_CONTROLLER_KEY:
			case XWII_EVENT_PRO_CONTROLLER_MOVE:
				if (mode == MODE_EXTENDED)
					pro_show_ext(&event);
				break;
			}
		}

		ret = keyboard();
		if (ret == -ECANCELED)
			return 0;
		else if (ret)
			return ret;
		refresh();
	}

	return ret;
}

static int enumerate()
{
	struct xwii_monitor *mon;
	char *ent;
	int num = 0;

	mon = xwii_monitor_new(false, false);
	if (!mon) {
		printf("Cannot create monitor\n");
		return -EINVAL;
	}

	while ((ent = xwii_monitor_poll(mon))) {
		printf("  Found device #%d: %s\n", ++num, ent);
		free(ent);
	}

	xwii_monitor_unref(mon);
	return 0;
}

static char *get_dev(int num)
{
	struct xwii_monitor *mon;
	char *ent;
	int i = 0;

	mon = xwii_monitor_new(false, false);
	if (!mon) {
		printf("Cannot create monitor\n");
		return NULL;
	}

	while ((ent = xwii_monitor_poll(mon))) {
		if (++i == num)
			break;
		free(ent);
	}

	xwii_monitor_unref(mon);

	if (!ent)
		printf("Cannot find device with number #%d\n", num);

	return ent;
}

int main(int argc, char **argv)
{
	int ret = 0;
	char *path = NULL;

	if (argc < 2 || !strcmp(argv[1], "-h")) {
		printf("Usage:\n");
		printf("\txwiishow [-h]: Show help\n");
		printf("\txwiishow list: List connected devices\n");
		printf("\txwiishow <num>: Show device with number #num\n");
		printf("\txwiishow /sys/path/to/device: Show given device\n");
		printf("UI commands:\n");
		printf("\tq: Quit application\n");
		printf("\tf: Freeze/Unfreeze screen\n");
		printf("\ts: Refresh static values (like battery or calibration)\n");
		printf("\tk: Toggle key events\n");
		printf("\tr: Toggle rumble motor\n");
		printf("\ta: Toggle accelerometer\n");
		printf("\ti: Toggle IR camera\n");
		printf("\tm: Toggle motion plus\n");
		printf("\tn: Toggle normalization for motion plus\n");
		printf("\tb: Toggle balance board\n");
		printf("\tp: Toggle pro controller\n");
		printf("\t1: Toggle LED 1\n");
		printf("\t2: Toggle LED 2\n");
		printf("\t3: Toggle LED 3\n");
		printf("\t4: Toggle LED 4\n");
		ret = -1;
	} else if (!strcmp(argv[1], "list")) {
		printf("Listing connected Wii Remote devices:\n");
		ret = enumerate();
		printf("End of device list\n");
	} else {
		if (argv[1][0] != '/')
			path = get_dev(atoi(argv[1]));

		ret = xwii_iface_new(&iface, path ? path : argv[1]);
		free(path);
		if (ret) {
			printf("Cannot create xwii_iface '%s' err:%d\n",
								argv[1], ret);
		} else {

			initscr();
			curs_set(0);
			raw();
			noecho();
			timeout(0);

			ret = xwii_iface_open(iface, XWII_IFACE_CORE |
						     XWII_IFACE_WRITABLE);
			if (ret)
				print_error("Error: Cannot open key iface: %d",
					    ret);

			ret = run_iface(iface);
			xwii_iface_unref(iface);
			if (ret) {
				print_error("Program failed; press any key to exit");
				refresh();
				timeout(-1);
				getch();
			}
			endwin();
		}
	}

	return abs(ret);
}
