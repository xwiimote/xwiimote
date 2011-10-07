/*
 * XWiimote - tools - xwiishow
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include "xwiimote.h"

static const char *code2str(unsigned int code)
{
	switch (code) {
	case XWII_KEY_LEFT:
		return "LEFT";
	case XWII_KEY_RIGHT:
		return "RIGHT";
	case XWII_KEY_UP:
		return "UP";
	case XWII_KEY_DOWN:
		return "DOWN";
	case XWII_KEY_A:
		return "A";
	case XWII_KEY_B:
		return "B";
	case XWII_KEY_PLUS:
		return "PLUS";
	case XWII_KEY_MINUS:
		return "MINUS";
	case XWII_KEY_HOME:
		return "HOME";
	case XWII_KEY_ONE:
		return "ONE";
	case XWII_KEY_TWO:
		return "TWO";
	default:
		return "UNKNOWN";
	}
}

static const char *state2str(unsigned int state)
{
	switch (state) {
	case 0:
		return "released";
	case 1:
		return "pressed";
	default:
		return "unknown";
	}
}

static const char *ev2str(unsigned int type)
{
	switch (type) {
	case XWII_EVENT_KEY:
		return "key";
	case XWII_EVENT_ACCEL:
		return "accelerometer";
	case XWII_EVENT_IR:
		return "ir";
	default:
		return "unknown";
	}
}

static void show_key_event(const struct xwii_event *event)
{
	printf("Code: %s (%s)\n", code2str(event->v.key.code),
						state2str(event->v.key.state));
}

static void show_accel_event(const struct xwii_event *event)
{
}

static void show_ir_event(const struct xwii_event *event)
{
}

static int run_iface(struct xwii_iface *iface)
{
	struct xwii_event event;
	int ret;

	printf("Waiting for events\n");
	while (true) {
		ret = xwii_iface_read(iface, &event);
		if (ret == -EAGAIN) {
			nanosleep(&(struct timespec)
				{.tv_sec = 0, .tv_nsec = 5000000 }, NULL);
		} else if (ret) {
			printf("Read error: %d\n", ret);
			break;
		} else {
			printf("Event type: %s\n", ev2str(event.type));
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
	}

	return 0;
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
	struct xwii_iface *iface;

	if (argc < 2) {
		printf("No device path given. Listing devices:\n");
		ret = enumerate();
		if (ret)
			return -ret;
		printf("End of device list\n");
		return 0;
	}

	printf("Opening %s\n", argv[1]);
	ret = xwii_iface_new(&iface, argv[1]);
	if (ret) {
		printf("Cannot create xwii_iface %d\n", ret);
		return -ret;
	}

	printf("Opening core interface\n");
	ret = xwii_iface_open(iface, XWII_IFACE_CORE);
	if (ret)
		printf("Cannot open core iface %d\n", ret);
	else
		ret = run_iface(iface);

	xwii_iface_unref(iface);

	return -ret;
}
