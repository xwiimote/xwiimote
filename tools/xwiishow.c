/*
 * XWiimote - tools
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * XWiimote Visualizer
 * This tool selects a connected wiimote and prints its current state to the
 * screen. It listens to new events and updates the state accordingly.
 * Exit with ctrl+c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <xwiimote.h>

static void show_keys(const struct xwii_state *state)
{
	fprintf(stdout, "Key LEFT RIGHT UP DOWN\n");
	fprintf(stdout, "    %d    %d     %d  %d\n",
					!!(state->keys & XWII_KEY_LEFT),
					!!(state->keys & XWII_KEY_RIGHT),
					!!(state->keys & XWII_KEY_UP),
					!!(state->keys & XWII_KEY_DOWN));
	fprintf(stdout, "Key A B ONE TWO\n");
	fprintf(stdout, "    %d %d %d   %d\n",
					!!(state->keys & XWII_KEY_A),
					!!(state->keys & XWII_KEY_B),
					!!(state->keys & XWII_KEY_ONE),
					!!(state->keys & XWII_KEY_TWO));
	fprintf(stdout, "Key MINUS HOME PLUS\n");
	fprintf(stdout, "    %d     %d    %d\n",
					!!(state->keys & XWII_KEY_MINUS),
					!!(state->keys & XWII_KEY_HOME),
					!!(state->keys & XWII_KEY_PLUS));
}

static void show_accel(const struct xwii_state *state)
{
	fprintf(stdout, "Accel X: %d\n", state->accelx);
	fprintf(stdout, "Accel Y: %d\n", state->accely);
	fprintf(stdout, "Accel Z: %d\n", state->accelz);
}

static void show_ir(const struct xwii_state *state)
{
	fprintf(stdout, "HAT 0 X: %u\n", state->irx[0]);
	fprintf(stdout, "HAT 0 Y: %u\n", state->iry[0]);
	fprintf(stdout, "HAT 1 X: %u\n", state->irx[1]);
	fprintf(stdout, "HAT 1 Y: %u\n", state->iry[1]);
	fprintf(stdout, "HAT 2 X: %u\n", state->irx[2]);
	fprintf(stdout, "HAT 2 Y: %u\n", state->iry[2]);
	fprintf(stdout, "HAT 3 X: %u\n", state->irx[3]);
	fprintf(stdout, "HAT 3 Y: %u\n", state->iry[3]);
}

static void show(struct xwii_dev *dev)
{
	int fd;
	uint16_t ev;
	const struct xwii_state *state;

	fd = xwii_dev_open_input(dev, false);
	if (fd < 0) {
		fprintf(stderr, "Cannot open device input\n");
		return;
	}

	state = xwii_dev_state(dev);

	do {
		ev = xwii_dev_poll(dev);
		fprintf(stdout, "\nReceived device event:\n");
		if (!ev)
			fprintf(stdout, "Empty event\n");
		if (ev & XWII_CLOSED)
			fprintf(stdout, "Device input closed\n");
		if (ev & XWII_BLOCKING)
			fprintf(stdout, "Device poll would block\n");
		fprintf(stdout, "Changed: ");
		if (ev & XWII_KEYS)
			fprintf(stdout, "Keys ");
		if (ev & XWII_ACCEL)
			fprintf(stdout, "Accel ");
		if (ev & XWII_IR)
			fprintf(stdout, "IR ");
		fprintf(stdout, "\n");
		show_keys(state);
		show_accel(state);
		show_ir(state);
	} while (!(ev & XWII_CLOSED));

	close(fd);
}

int main(int argc, char **argv)
{
	struct xwii_monitor *mon;
	struct xwii_dev *dev;

	mon = xwii_monitor_new(false, false);
	if (!mon) {
		fprintf(stderr, "Cannot open xwii-monitor\n");
		return EXIT_FAILURE;
	}

	dev = xwii_monitor_poll(mon);
	if (dev) {
		fprintf(stdout, "Showing device %p\n", dev);
		show(dev);
		xwii_dev_free(dev);
	} else {
		fprintf(stderr, "Didn't find connected wiimote\n");
	}

	xwii_monitor_free(mon);

	return EXIT_SUCCESS;
}
