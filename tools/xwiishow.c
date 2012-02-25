/*
 * XWiimote - tools - xwiishow
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * Interactive Wiimote Testing Tool
 * If you run this tool without arguments, then it lists all currently connected
 * wiimotes and exits.
 * You need to pass one path as argument and the given wiimote is opened and
 * printed to the screen. When wiimote events are received, then the screen is
 * updated correspondingly. You can use the keyboard to control the wiimote:
 *    q: quit the application
 *    a: toggle accelerometer
 *    r: toggle rumble
 *
 * Example:
 *  ./xwiishow /sys/bus/hid/devices/<device>
 * This will opened the given wiimote device and print it to the screen.
 */

#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "xwiimote.h"

static struct xwii_iface *iface;

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
}

static void show_ir_event(const struct xwii_event *event)
{
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
	mvprintw(i++, 0, "+-----------------+ +------+");
	mvprintw(i++, 0, "|       +-+       | |      |");
	mvprintw(i++, 0, "|       | |       | +------+");
	mvprintw(i++, 0, "|     +-+ +-+     |");
	mvprintw(i++, 0, "|     |     |     |");
	mvprintw(i++, 0, "|     +-+ +-+     |");
	mvprintw(i++, 0, "|       | |       |");
	mvprintw(i++, 0, "|       +-+       |");
	mvprintw(i++, 0, "|                 |");
	mvprintw(i++, 0, "|   +-+     +-+   |");
	mvprintw(i++, 0, "|   | |     | |   |");
	mvprintw(i++, 0, "|   +-+     +-+   |");
	mvprintw(i++, 0, "|                 |");
	mvprintw(i++, 0, "| ( ) |     | ( ) |");
	mvprintw(i++, 0, "|                 |");
	mvprintw(i++, 0, "|      +++++      |");
	mvprintw(i++, 0, "|      +   +      |");
	mvprintw(i++, 0, "|      +   +      |");
	mvprintw(i++, 0, "|      +++++      |");
	mvprintw(i++, 0, "|                 |");
	mvprintw(i++, 0, "|       | |       |");
	mvprintw(i++, 0, "|       | |       |");
	mvprintw(i++, 0, "|                 |");
	mvprintw(i++, 0, "+-----------------+");

	return 0;
}

static void toggle_accel()
{
	if (xwii_iface_opened(iface) & XWII_IFACE_ACCEL)
		xwii_iface_close(iface, XWII_IFACE_ACCEL);
	else
		xwii_iface_open(iface, XWII_IFACE_ACCEL);
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
			printw("Error: Read failed with err:%d\n", ret);
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

	mon = xwii_monitor_new(false, false);
	if (!mon) {
		printf("Cannot create monitor\n");
		return -EINVAL;
	}

	while ((ent = xwii_monitor_poll(mon))) {
		printf("  Found device: %s\n", ent);
		free(ent);
	}

	xwii_monitor_unref(mon);
	return 0;
}

int main(int argc, char **argv)
{
	int ret = 0;

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
		ret = -1;
	} else if (!strcmp(argv[1], "list")) {
		printf("Listing connected Wii Remote devices:\n");
		ret = enumerate();
		printf("End of device list\n");
	} else {
		ret = xwii_iface_new(&iface, argv[1]);
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
					printw("Program failed; press any key to exit\n");
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
