/*
 * XWiimote - driver - driver.c
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <unistd.h>
#include "log.h"
#include "main.h"
#include "proto.h"
#include "xwii_input.h"

static struct wii_log logger;
static struct wii_proto_dev dev;
static struct wii_proto_res state;

static bool handle_wiires(struct wii_proto_res *res, signed int uinput);
static bool handle_uinput(struct input_event *ev);

static char *uinput_locations[] = {
	"/dev/uinput",
	"/dev/input/uinput",
	"/dev/misc/uinput",
	NULL
};

static signed int uinput_open_fd()
{
	signed int fd;
	unsigned int i;

	for (i = 0; uinput_locations[i]; ++i) {
		fd = open(uinput_locations[i], O_RDWR);
		if (fd >= 0)
			return fd;
	}

	wii_log_do(&logger, "Error: Cannot open uinput driver");
	return -1;
}

static struct {
	unsigned long cmd;
	uint16_t type;
} uinput_table[] = {
	{ UI_SET_EVBIT, EV_KEY },
	{ UI_SET_EVBIT, EV_MSC },
	{ UI_SET_EVBIT, EV_ABS },
	{ UI_SET_KEYBIT, XWII_EV_KEY_UP },
	{ UI_SET_KEYBIT, XWII_EV_KEY_DOWN },
	{ UI_SET_KEYBIT, XWII_EV_KEY_LEFT },
	{ UI_SET_KEYBIT, XWII_EV_KEY_RIGHT },
	{ UI_SET_KEYBIT, XWII_EV_KEY_A },
	{ UI_SET_KEYBIT, XWII_EV_KEY_B },
	{ UI_SET_KEYBIT, XWII_EV_KEY_MINUS },
	{ UI_SET_KEYBIT, XWII_EV_KEY_PLUS },
	{ UI_SET_KEYBIT, XWII_EV_KEY_HOME },
	{ UI_SET_KEYBIT, XWII_EV_KEY_ONE },
	{ UI_SET_KEYBIT, XWII_EV_KEY_TWO },
	{ UI_SET_ABSBIT, ABS_X },
	{ UI_SET_ABSBIT, ABS_Y },
	{ UI_SET_ABSBIT, ABS_Z },
	{ UI_SET_MSCBIT, MSC_RAW },
	{ 0, 0 },
};

static inline void uinput_set_abs(struct uinput_user_dev *udev, int index, int max, int min, int fuzz, int flat)
{
	udev->absmax[index] = max;
	udev->absmin[index] = min;
	udev->absfuzz[index] = fuzz;
	udev->absflat[index] = flat;
}

static signed int uinput_open()
{
	signed int fd;
	unsigned int i;
	struct uinput_user_dev udev;

	fd = uinput_open_fd();
	if (fd < 0)
		return -1;

	for (i = 0; uinput_table[i].cmd != 0; ++i) {
		if (ioctl(fd, uinput_table[i].cmd, uinput_table[i].type) < 0)
			goto failure;
	}

	memset(&udev, 0, sizeof(udev));
	snprintf(udev.name, sizeof(udev.name), "%s", "Nintendo Wii Remote");
	udev.id.bustype = BUS_BLUETOOTH;
	udev.id.vendor = 0x057e;
	udev.id.product = 0x0306;
	uinput_set_abs(&udev, ABS_X, 512, -512, 4, 8);
	uinput_set_abs(&udev, ABS_Y, 512, -512, 4, 8);
	uinput_set_abs(&udev, ABS_Z, 512, -512, 4, 8);

	if (write(fd, &udev, sizeof(udev)) != sizeof(udev))
		goto failure;

	if (ioctl(fd, UI_DEV_CREATE) < 0) {
		wii_log_do(&logger, "Error: Cannot create uinput device");
		goto failure;
	}

	return fd;

failure:
	close(fd);
	wii_log_do(&logger, "Error: Cannot configure uinput device");
	return -1;
}

static bool uinput_read(struct pollfd *pfd, struct input_event *event)
{
	event->type = EV_MAX;

	if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL)) {
		wii_log_do(&logger, "Error: Hangup on uinput socket");
		return false;
	}

	if (!(pfd->revents & POLLIN))
		return true;

	if (sizeof(*event) != read(pfd->fd, (void*)event, sizeof(*event))) {
		wii_log_do(&logger, "Error: Read-error on uinput socket, errno %d", errno);
		return false;
	}
	return true;
}

static bool uinput_send(signed int fd, uint16_t type, uint16_t code, int32_t value)
{
	struct input_event event;

	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = type;
	event.code = code;
	event.value = value;
	if (write(fd, &event, sizeof(event)) != sizeof(event)) {
		wii_log_do(&logger, "Error: Cannot send uinput event, errno %d", errno);
		return false;
	}
	return true;
}

static bool io_read(struct pollfd *pfd, struct wii_proto_res *res)
{
	char buf[WII_PROTO_SH_MAX];
	signed int num;

	res->modified = 0;

	if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL)) {
		wii_log_do(&logger, "Error: Read-error on input socket");
		return false;
	}

	if (!(pfd->revents & POLLIN))
		return true;

	if (0 >= (num = read(pfd->fd, buf, sizeof(buf)))) {
		wii_log_do(&logger, "Error: Read-error on input socket, errno %d", errno);
		return false;
	}

	wii_proto_decode(&dev, (uint8_t*)buf, num, res);
	return true;
}

/* returns 1 if sent, 0 if blocking, -1 on failure */
static signed int io_send_do(signed int fd, void *buf, size_t size)
{
	signed int ret;

	if ((ret = write(fd, buf, size)) == size)
		return 1;

	if (ret == 0 && (errno == EAGAIN || errno == EINTR))
		return 0;

	if (ret > 0)
		wii_log_do(&logger, "Error: Outgoing packet was truncated");
	else
		wii_log_do(&logger, "Error: Write-error on output socket, errno %d", errno);

	return -1;
}

static bool io_send(struct pollfd *pfd, signed int *timeout)
{
	static bool pending;
	static struct wii_proto_buf buf;
	signed int ret;

	*timeout = -1;

	if (pfd->revents & (POLLERR | POLLHUP | POLLNVAL)) {
		wii_log_do(&logger, "Error: IO hangup on output socket");
		return false;
	}

	/* still waiting */
	if (pfd->events & POLLOUT && !(pfd->revents & POLLOUT))
		return true;

	/* process pending buffer */
	if (pending) {
		ret = io_send_do(pfd->fd, buf.buf, buf.size);
		if (ret < 0)
			return false;
		if (ret == 0)
			return true;
		pfd->events &= ~POLLOUT;
		pending = 0;
		if (buf.wait) {
			*timeout = buf.wait;
			return true;
		}
	}

	while (wii_proto_encode(&dev, &buf)) {
		ret = io_send_do(pfd->fd, buf.buf, buf.size);
		if (ret < 0)
			return false;
		if (ret == 0) {
			pending = 1;
			pfd->events |= POLLOUT;
			return true;
		}
		if (buf.wait) {
			*timeout = buf.wait;
			return true;
		}
	}
	return true;
}

void wii_start_driver(void *drv_arg)
{
	struct wii_drv_io *drv = drv_arg;
	nfds_t psize = 3;
	struct pollfd fds[3];
	signed int uinput, num, timeout;
	size_t errcnt;
	time_t lasterr, now;
	struct wii_proto_res res;
	struct input_event ev;

	wii_log_open(&logger, "driver", true);
	wii_proto_init(&dev);
	memset(&res, 0, sizeof(res));
	uinput = uinput_open();
	if (uinput < 0)
		goto cleanup;

	fds[0].fd = drv->in;
	fds[0].events = POLLIN;
	fds[1].fd = drv->out;
	fds[1].events = 0;
	fds[2].fd = uinput;
	fds[2].events = POLLIN;

	timeout = -1;
	errcnt = 0;
	lasterr = time(NULL);
	wii_log_do(&logger, "Driver started");

	do {
		if (wii_terminating(false)) {
			wii_log_do(&logger, "Error: Interrupted");
			break;
		}
		num = poll(fds, psize, timeout);
		now = time(NULL);
		if (num < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				if (lasterr < now - 5) {
					errcnt = 0;
					lasterr = now;
				}
				if (++errcnt > 10) {
					wii_log_do(&logger, "Error: poll() error threshold reached, errno %d", errno);
					break;
				}
				continue;
			}
			wii_log_do(&logger, "Error: poll() failed, errno %d", errno);
			break;
		}
		if (!io_read(&fds[0], &res))
			break;
		if (!handle_wiires(&res, uinput))
			break;
		if (!uinput_read(&fds[2], &ev))
			break;
		if (!handle_uinput(&ev))
			break;
		if (!io_send(&fds[1], &timeout))
			break;
	} while (1);

cleanup:
	wii_log_do(&logger, "Driver stopped");
	if (uinput >= 0)
		close(uinput);
	wii_proto_deinit(&dev);
	wii_log_close(&logger);
	close(drv->in);
	close(drv->out);
}

static bool handle_wiires(struct wii_proto_res *res, signed int uinput)
{
	int32_t payload, value;

	if (res->modified & WII_PROTO_CR_BATTERY) {
		payload = XWII_EV_MK_BATTERY(res->battery.low, res->battery.level);
		value = XWII_EV_MK(XWII_EV_REP_BATTERY, payload);
		if (!uinput_send(uinput, EV_MSC, MSC_RAW, value))
			return false;
	}

	if (res->modified & WII_PROTO_CR_KEY) {
	#define TEST_KEY(FIELD, FLAG) if (res->key.FIELD != state.key.FIELD) \
						if (!uinput_send(uinput, EV_KEY, FLAG, res->key.FIELD)) \
							return false;
		TEST_KEY(left, XWII_EV_KEY_LEFT);
		TEST_KEY(right, XWII_EV_KEY_RIGHT);
		TEST_KEY(up, XWII_EV_KEY_UP);
		TEST_KEY(down, XWII_EV_KEY_DOWN);
		TEST_KEY(a, XWII_EV_KEY_A);
		TEST_KEY(b, XWII_EV_KEY_B);
		TEST_KEY(minus, XWII_EV_KEY_MINUS);
		TEST_KEY(plus, XWII_EV_KEY_PLUS);
		TEST_KEY(home, XWII_EV_KEY_HOME);
		TEST_KEY(one, XWII_EV_KEY_ONE);
		TEST_KEY(two, XWII_EV_KEY_TWO);
	#undef TEST_KEY
		memcpy(&state.key, &res->key, sizeof(state.key));
	}

	if (res->modified & WII_PROTO_CR_MOVE) {
		if (!uinput_send(uinput, EV_ABS, ABS_X, res->move.x))
			return false;
		if (!uinput_send(uinput, EV_ABS, ABS_Y, res->move.y))
			return false;
		if (!uinput_send(uinput, EV_ABS, ABS_Z, res->move.z))
			return false;
	}

	if (res->modified)
		if (!uinput_send(uinput, EV_SYN, SYN_REPORT, 0))
			return false;
	return true;
}

static bool handle_uinput(struct input_event *ev)
{
	uint8_t id;
	uint32_t payload;
	struct wii_proto_res res;
	wii_proto_mask_t enable, disable;

	/* ignore unknown events */
	if (ev->type != EV_MSC || ev->code != MSC_RAW)
		return true;

	memset(&res, 0, sizeof(res));
	enable = 0;
	disable = 0;
	id = XWII_EV_GET_ID(ev->value);
	payload = XWII_EV_GET_ARG(ev->value);

	if (id == XWII_EV_CMD_ENABLE) {
		if (payload & XWII_EV_UNIT_INPUT)
			enable |= WII_PROTO_CU_INPUT;
		if (payload & XWII_EV_UNIT_ACCEL)
			enable |= WII_PROTO_CU_ACCEL;
	} else if (id == XWII_EV_CMD_DISABLE) {
		if (payload & XWII_EV_UNIT_INPUT)
			disable |= WII_PROTO_CU_INPUT;
		if (payload & XWII_EV_UNIT_ACCEL)
			disable |= WII_PROTO_CU_ACCEL;
	} else if (id == XWII_EV_CMD_RUMBLE) {
		res.modified |= WII_PROTO_CC_RUMBLE;
		if (payload)
			res.rumble.on = 1;
	} else if (id == XWII_EV_CMD_LED) {
		res.modified |= WII_PROTO_CC_LED;
		if (payload & XWII_EV_LED1)
			res.led.one = 1;
		if (payload & XWII_EV_LED2)
			res.led.two = 1;
		if (payload & XWII_EV_LED3)
			res.led.three = 1;
		if (payload & XWII_EV_LED4)
			res.led.four = 1;
	} else if (id == XWII_EV_CMD_QUERY) {
		res.modified |= WII_PROTO_CC_QUERY;
	} else if (id == XWII_EV_CMD_ACALIB) {
		res.modified |= WII_PROTO_CC_ACALIB;
		res.acalib.x = XWII_EV_GET_ACALIB_X(payload);
		res.acalib.y = XWII_EV_GET_ACALIB_Y(payload);
		res.acalib.z = XWII_EV_GET_ACALIB_Z(payload);
	}

	if (enable)
		wii_proto_enable(&dev, enable);
	if (disable)
		wii_proto_disable(&dev, disable);
	if (res.modified)
		wii_proto_do(&dev, &res);

	return true;
}
