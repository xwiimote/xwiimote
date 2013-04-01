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
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "xwiimote.h"

enum window_mode {
	MODE_ERROR,
	MODE_NORMAL,
	MODE_EXTENDED,
};

static struct xwii_iface *iface;
static unsigned int mode = MODE_ERROR;

/* error messages */

static void print_error(const char *format, ...)
{
	va_list list;

	va_start(list, format);
	move(23, 22);
	vw_printw(stdscr, format, list);
	printw("        ");
	va_end(list);
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
	accel_show(&ev);
}

static void accel_toggle(void)
{
	if (xwii_iface_opened(iface) & XWII_IFACE_ACCEL) {
		xwii_iface_close(iface, XWII_IFACE_ACCEL);
		accel_clear();
	} else {
		xwii_iface_open(iface, XWII_IFACE_ACCEL);
	}
}

/* IR events */

static void ir_show(const struct xwii_event *event)
{
	mvprintw(3, 27, "%04" PRId32, event->v.abs[0].x);
	mvprintw(3, 32, "%04" PRId32, event->v.abs[0].y);

	mvprintw(3, 41, "%04" PRId32, event->v.abs[1].x);
	mvprintw(3, 46, "%04" PRId32, event->v.abs[1].y);

	mvprintw(3, 55, "%04" PRId32, event->v.abs[2].x);
	mvprintw(3, 60, "%04" PRId32, event->v.abs[2].y);

	mvprintw(3, 69, "%04" PRId32, event->v.abs[3].x);
	mvprintw(3, 74, "%04" PRId32, event->v.abs[3].y);
}

static void ir_clear(void)
{
	struct xwii_event ev;

	ev.v.abs[0].x = 0;
	ev.v.abs[0].y = 0;
	ev.v.abs[1].x = 0;
	ev.v.abs[1].y = 0;
	ev.v.abs[2].x = 0;
	ev.v.abs[2].y = 0;
	ev.v.abs[3].x = 0;
	ev.v.abs[3].y = 0;
	ir_show(&ev);
}

static void ir_toggle(void)
{
	if (xwii_iface_opened(iface) & XWII_IFACE_IR) {
		xwii_iface_close(iface, XWII_IFACE_IR);
		ir_clear();
	} else {
		xwii_iface_open(iface, XWII_IFACE_IR);
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

/* basic window setup */

static void setup_window(void)
{
	size_t i;

	i = 0;
	/* 80x24 Box */
	mvprintw(i++, 0, "+-----------------+ +------+ +-------------------------------------------------+");
	mvprintw(i++, 0, "|       +-+       | |      |  Accel x:       y:       z:                       |");
	mvprintw(i++, 0, "|       | |       | +------+ +-------------------------------------------------+");
	mvprintw(i++, 0, "|     +-+ +-+     | IR #1:     x     #2:     x     #3:     x     #4:     x     |");
	mvprintw(i++, 0, "|     |     |     | +----------------------------------------------------------+");
	mvprintw(i++, 0, "|     +-+ +-+     |                                                            |");
	mvprintw(i++, 0, "|       | |       |                                                            |");
	mvprintw(i++, 0, "|       +-+       |                                                            |");
	mvprintw(i++, 0, "|                 |                                                            |");
	mvprintw(i++, 0, "|   +-+     +-+   |                                                            |");
	mvprintw(i++, 0, "|   | |     | |   |                                                            |");
	mvprintw(i++, 0, "|   +-+     +-+   |                                                            |");
	mvprintw(i++, 0, "|                 |                                                            |");
	mvprintw(i++, 0, "| ( ) |     | ( ) |                                                            |");
	mvprintw(i++, 0, "|                 |                                                            |");
	mvprintw(i++, 0, "|      +++++      |                                                            |");
	mvprintw(i++, 0, "|      +   +      |                                                            |");
	mvprintw(i++, 0, "|      +   +      |                                                            |");
	mvprintw(i++, 0, "|      +++++      |                                                            |");
	mvprintw(i++, 0, "|                 |                                                            |");
	mvprintw(i++, 0, "|       | |       |                                                            |");
	mvprintw(i++, 0, "|       | |       |                                                            |");
	mvprintw(i++, 0, "|                 | +----------------------------------------------------------+");
	mvprintw(i++, 0, "+-----------------+ |");
}

static void setup_ext_window(void)
{
	size_t i;

	i = 0;
	/* 160x40 Box */
	mvprintw(i++, 80, " +---------------------+-------------------------------------------------------+");
	mvprintw(i++, 80, "                       |                                                       |");
	mvprintw(i++, 80, "                    Z  |                                                       |");
	mvprintw(i++, 80, "                       |                                                       |");
	mvprintw(i++, 80, "                       |                                                       |");
	mvprintw(i++, 80, "                       |                                                       |");
	mvprintw(i++, 80, "           ##          |                                                       |");
	mvprintw(i++, 80, " X                     |                                                       |");
	mvprintw(i++, 80, "                       |                                                       |");
	mvprintw(i++, 80, "                       |                                                       |");
	mvprintw(i++, 80, "                       |                                                       |");
	mvprintw(i++, 80, "              Y        |                                                       |");
	mvprintw(i++, 80, " +---------------------+-------------------------------------------------------+");
}

static void handle_resize(void)
{
	if (LINES < 24 || COLS < 80) {
		mode = MODE_ERROR;
		erase();
		mvprintw(0, 0, "Error: Screen is too small");
	} else if (LINES < 40 || COLS < 160) {
		mode = MODE_NORMAL;
		erase();
		setup_window();
	} else {
		mode = MODE_EXTENDED;
		erase();
		setup_ext_window();
		setup_window();
	}
}

/* keyboard handling */

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
	case 'a':
		accel_toggle();
		break;
	case 'i':
		ir_toggle();
		break;
	case 'r':
		rumble_toggle();
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

	handle_resize();
	accel_clear();
	ir_clear();

	while (true) {
		ret = xwii_iface_poll(iface, &event);
		if (ret == -EAGAIN) {
			nanosleep(&(struct timespec)
				{.tv_sec = 0, .tv_nsec = 5000000 }, NULL);
		} else if (ret) {
			print_error("Error: Read failed with err:%d", ret);
			break;
		} else {
			switch (event.type) {
			case XWII_EVENT_KEY:
				key_show(&event);
				break;
			case XWII_EVENT_ACCEL:
				if (mode == MODE_EXTENDED)
					accel_show_ext(&event);
				accel_show(&event);
				break;
			case XWII_EVENT_IR:
				ir_show(&event);
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
		printf("\tr: Toggle rumble motor\n");
		printf("\ta: Toggle accelerometer\n");
		printf("\ti: Toggle IR camera\n");
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
			ret = xwii_iface_open(iface, XWII_IFACE_CORE |
							XWII_IFACE_WRITABLE);
			if (ret) {
				printf("Cannot open core iface '%s' err:%d\n",
								argv[1], ret);
			} else {
				initscr();
				curs_set(0);
				raw();
				noecho();
				timeout(0);
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
	}

	return abs(ret);
}
