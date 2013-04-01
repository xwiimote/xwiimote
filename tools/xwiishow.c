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

static struct xwii_iface *iface;

static void print_error(const char *format, ...)
{
	va_list list;

	va_start(list, format);
	move(23, 22);
	vw_printw(stdscr, format, list);
	printw("        ");
	va_end(list);
}

static void show_key_event(const struct xwii_event *event)
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

static void show_accel_event(const struct xwii_event *event)
{
	mvprintw(1, 48, "%5" PRId32, event->v.abs[0].x);
	mvprintw(2, 48, "%5" PRId32, event->v.abs[0].y);
	mvprintw(3, 48, "%5" PRId32, event->v.abs[0].z);
}

static void show_ir_event(const struct xwii_event *event)
{
	mvprintw(5, 29, "%5" PRId32, event->v.abs[0].x);
	mvprintw(6, 29, "%5" PRId32, event->v.abs[0].y);

	mvprintw(5, 56, "%5" PRId32, event->v.abs[1].x);
	mvprintw(6, 56, "%5" PRId32, event->v.abs[1].y);

	mvprintw(8, 29, "%5" PRId32, event->v.abs[2].x);
	mvprintw(9, 29, "%5" PRId32, event->v.abs[2].y);

	mvprintw(8, 56, "%5" PRId32, event->v.abs[3].x);
	mvprintw(9, 56, "%5" PRId32, event->v.abs[3].y);
}

static void show_rumble(bool on)
{
	mvprintw(1, 21, on ? "RUMBLE" : "      ");
}

static int setup_window()
{
	size_t i;

	if (LINES < 24 || COLS < 80) {
		printw("Error: Screen is too small\n");
		return -EINVAL;
	}

	i = 0;
	/* 80x24 Box */
	mvprintw(i++, 0, "+-----------------+ +------+ +-------------------------------------------------+");
	mvprintw(i++, 0, "|       +-+       | |      |  Accelerometer  x:                                |");
	mvprintw(i++, 0, "|       | |       | +------+                 y:                                |");
	mvprintw(i++, 0, "|     +-+ +-+     |                          z:                                |");
	mvprintw(i++, 0, "|     |     |     | +----------------------------------------------------------+");
	mvprintw(i++, 0, "|     +-+ +-+     | IR #1 x:                      #2 x:                        |");
	mvprintw(i++, 0, "|       | |       |       y:                         y:                        |");
	mvprintw(i++, 0, "|       +-+       |                                                            |");
	mvprintw(i++, 0, "|                 |    #3 x:                      #4 x:                        |");
	mvprintw(i++, 0, "|   +-+     +-+   |       y:                         y:                        |");
	mvprintw(i++, 0, "|   | |     | |   | +----------------------------------------------------------+");
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

	return 0;
}

static void toggle_accel()
{
	if (xwii_iface_opened(iface) & XWII_IFACE_ACCEL)
		xwii_iface_close(iface, XWII_IFACE_ACCEL);
	else
		xwii_iface_open(iface, XWII_IFACE_ACCEL);
}

static void toggle_ir()
{
	if (xwii_iface_opened(iface) & XWII_IFACE_IR)
		xwii_iface_close(iface, XWII_IFACE_IR);
	else
		xwii_iface_open(iface, XWII_IFACE_IR);
}

static void toggle_rumble()
{
	static bool on = false;

	on = !on;
	xwii_iface_rumble(iface, on);
	show_rumble(on);
}

static int keyboard()
{
	int key;

	key = getch();
	if (key == ERR)
		return 0;

	switch (key) {
	case 'a':
		toggle_accel();
		break;
	case 'i':
		toggle_ir();
		break;
	case 'r':
		toggle_rumble();
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

	ret = setup_window();
	if (ret)
		return ret;

	while (true) {
		ret = xwii_iface_poll(iface, &event);
		if (ret == -EAGAIN) {
			nanosleep(&(struct timespec)
				{.tv_sec = 0, .tv_nsec = 5000000 }, NULL);
		} else if (ret) {
			print_error("Error: Read failed with err:%d\n", ret);
			break;
		} else {
			switch (event.type) {
			case XWII_EVENT_KEY:
				show_key_event(&event);
				break;
			case XWII_EVENT_ACCEL:
				show_accel_event(&event);
				break;
			case XWII_EVENT_IR:
				show_ir_event(&event);
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
