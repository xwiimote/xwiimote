/*
 * XWiimote - tools - xwiikeymap
 * Written 2011,2012 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * XWiimote Button Remapper
 * This tool listens for incoming Wii Remotes and for every connected Wii Remote
 * it creates a new input device via uinput and maps the keys as given in the
 * keymap. This allows to map the buttons of the Wii Remote to arbitrary keys.
 */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "eloop.h"
#include "log.h"
#include "xwiimote.h"

#define UINPUT_PATH "/dev/uinput"
#define UINPUT_NAME "XWiimote Keyboard"

struct app {
	struct ev_eloop *eloop;
	struct ev_signal *sig_term;
	struct ev_signal *sig_int;
	struct xwii_monitor *monitor;
	int monitor_fd;
	struct ev_fd *monitor_fdo;
};

struct dev {
	struct xwii_iface *iface;
	int iface_fd;
	struct ev_fd *iface_fdo;
	int uinput_fd;
};

static volatile sig_atomic_t terminate = 0;

static void sig_term(struct ev_signal *sig, int signum, void *data)
{
	terminate = 1;
}

static void uinput_destroy(struct dev *dev)
{
	if (dev->uinput_fd < 0)
		return;

	ioctl(dev->uinput_fd, UI_DEV_DESTROY);
	close(dev->uinput_fd);
}

static int uinput_init(struct dev *dev)
{
	int ret, i;
	struct uinput_user_dev udev;
	uint16_t keys[] = { 0 };

	dev->uinput_fd = open(UINPUT_PATH, O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if (dev->uinput_fd < 0) {
		ret = -errno;
		log_err("app: cannot open uinput device %s: %m (%d)\n",
							UINPUT_PATH, ret);
		return ret;
	}

	memset(&udev, 0, sizeof(udev));
	strncpy(udev.name, UINPUT_NAME, UINPUT_MAX_NAME_SIZE);
	udev.id.bustype = XWII_ID_BUS;
	udev.id.vendor = XWII_ID_VENDOR;
	udev.id.product = XWII_ID_PRODUCT;
	udev.id.version = 0;

	ret = write(dev->uinput_fd, &udev, sizeof(udev));
	if (ret != sizeof(udev)) {
		ret = -EFAULT;
		log_err("app: cannot register uinput device\n");
		goto err;
	}

	ret = ioctl(dev->uinput_fd, UI_SET_EVBIT, EV_KEY);
	if (ret) {
		ret = -EFAULT;
		log_err("app: cannot initialize uinput device\n");
		goto err;
	}

	for (i = 0; keys[i]; ++i) {
		ret = ioctl(dev->uinput_fd, UI_SET_KEYBIT, keys[i]);
		if (ret) {
			ret = -EFAULT;
			log_err("app: cannot initialize uinput device\n");
			goto err;
		}
	}

	ret = ioctl(dev->uinput_fd, UI_DEV_CREATE);
	if (ret) {
		ret = -EFAULT;
		log_err("app: cannot create uinput device\n");
		goto err;
	}

	return 0;

err:
	close(dev->uinput_fd);
	dev->uinput_fd = -1;
	return ret;
}

static void destroy_device(struct dev *dev)
{
	if (!dev)
		return;

	uinput_destroy(dev);
	ev_eloop_rm_fd(dev->iface_fdo);
	xwii_iface_unref(dev->iface);
	free(dev);
}

static void device_event(struct ev_fd *fdo, int mask, void *data)
{
	struct dev *dev = data;
	struct xwii_event event;
	int ret;

	if (mask & (EV_HUP | EV_ERR)) {
		log_err("app: Wii Remote device closed\n");
		destroy_device(dev);
	} else if (mask & EV_READABLE) {
		ret = xwii_iface_read(dev->iface, &event);
		if (ret != -EAGAIN) {
			if (ret) {
				log_err("app: reading Wii Remote failed; "
							"closing device...\n");
				destroy_device(dev);
			} else {
				log_info("app: incoming data\n");
			}
		}
	}
}

static void monitor_poll(struct app *app)
{
	char *str;
	struct dev *dev;
	int ret;

	while ((str = xwii_monitor_poll(app->monitor))) {
		dev = malloc(sizeof(*dev));
		if (!dev) {
			log_warn("app: cannot create new Wii Remote device\n");
			free(str);
			continue;
		}

		memset(dev, 0, sizeof(*dev));
		ret = xwii_iface_new(&dev->iface, str);

		if (ret) {
			log_warn("app: cannot create new Wii Remote device\n");
			free(dev);
			free(str);
			continue;
		}

		ret = xwii_iface_open(dev->iface, XWII_IFACE_CORE);
		if (ret) {
			log_warn("app: cannot open Wii Remote core iface\n");
			xwii_iface_unref(dev->iface);
			free(dev);
			free(str);
			continue;
		}

		dev->iface_fd = xwii_iface_get_fd(dev->iface);
		ret = ev_eloop_new_fd(app->eloop,
					&dev->iface_fdo,
					dev->iface_fd,
					EV_READABLE,
					device_event,
					dev);
		if (ret) {
			log_warn("app: cannot create new Wii Remote device\n");
			xwii_iface_unref(dev->iface);
			free(dev);
			free(str);
			continue;
		}

		ret = uinput_init(dev);
		if (ret) {
			log_warn("app: cannot create new Wii Remote device\n");
			ev_eloop_rm_fd(dev->iface_fdo);
			xwii_iface_unref(dev->iface);
			free(dev);
			free(str);
			continue;
		}

		log_info("app: new Wii Remote detected: %s\n", str);
		free(str);
	}
}

static void monitor_event(struct ev_fd *fdo, int mask, void *data)
{
	struct app *app = data;

	if (mask & (EV_HUP | EV_ERR)) {
		log_err("app: Wii Remote monitor closed unexpectedly\n");
		terminate = 1;
	} else if (mask & EV_READABLE) {
		monitor_poll(app);
	}
}

static void app_destroy(struct app *app)
{
	ev_eloop_rm_fd(app->monitor_fdo);
	xwii_monitor_unref(app->monitor);
	ev_eloop_rm_signal(app->sig_int);
	ev_eloop_rm_signal(app->sig_term);
	ev_eloop_unref(app->eloop);
}

static int app_setup(struct app *app)
{
	int ret;

	ret = ev_eloop_new(&app->eloop);
	if (ret)
		goto err;

	ret = ev_eloop_new_signal(app->eloop,
				&app->sig_term,
				SIGTERM,
				sig_term,
				app);
	if (ret)
		goto err;

	ret = ev_eloop_new_signal(app->eloop,
				&app->sig_int,
				SIGINT,
				sig_term,
				app);
	if (ret)
		goto err;

	app->monitor = xwii_monitor_new(true, false);
	if (!app->monitor) {
		ret = -EFAULT;
		log_err("app: cannot create Wii Remote monitor\n");
		goto err;
	}

	app->monitor_fd = xwii_monitor_get_fd(app->monitor, false);
	if (app->monitor_fd < 0) {
		ret = -EFAULT;
		log_err("app: cannot get monitor fd\n");
		goto err;
	}

	ret = ev_eloop_new_fd(app->eloop,
				&app->monitor_fdo,
				app->monitor_fd,
				EV_READABLE,
				monitor_event,
				app);
	if (ret)
		goto err;

	return 0;

err:
	app_destroy(app);
	return ret;
}

int main(int argc, char **argv)
{
	int ret;
	struct app app;

	log_info("app: initializing\n");

	memset(&app, 0, sizeof(app));

	ret = app_setup(&app);
	if (ret)
		goto err;

	log_info("app: starting\n");
	monitor_poll(&app);

	while (!terminate) {
		ret = ev_eloop_dispatch(app.eloop, -1);
		if (ret)
			break;
	}

	log_info("app: stopping\n");

	/* TODO: add device into global list and remove all devices here */

	app_destroy(&app);
err:
	if (ret) {
		log_info("app: failed with %d\n", ret);
		return EXIT_FAILURE;
	} else {
		log_info("app: terminating\n");
		return EXIT_SUCCESS;
	}
}
