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
	int uinput_fd;
};

static volatile sig_atomic_t terminate = 0;

static void sig_term(struct ev_signal *sig, int signum, void *data)
{
	terminate = 1;
}

static void uinput_destroy(struct app *app)
{
	if (app->uinput_fd < 0)
		return;

	ioctl(app->uinput_fd, UI_DEV_DESTROY);
	close(app->uinput_fd);
}

static int uinput_init(struct app *app)
{
	int ret, i;
	struct uinput_user_dev dev;
	uint16_t keys[] = { 0 };

	app->uinput_fd = open(UINPUT_PATH, O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if (app->uinput_fd < 0) {
		ret = -errno;
		log_err("app: cannot open uinput device %s: %m (%d)\n",
							UINPUT_PATH, ret);
		return ret;
	}

	memset(&dev, 0, sizeof(dev));
	strncpy(dev.name, UINPUT_NAME, UINPUT_MAX_NAME_SIZE);
	dev.id.bustype = XWII_ID_BUS;
	dev.id.vendor = XWII_ID_VENDOR;
	dev.id.product = XWII_ID_PRODUCT;
	dev.id.version = 0;

	ret = write(app->uinput_fd, &dev, sizeof(dev));
	if (ret != sizeof(dev)) {
		ret = -EFAULT;
		log_err("app: cannot register uinput device\n");
		goto err;
	}

	ret = ioctl(app->uinput_fd, UI_SET_EVBIT, EV_KEY);
	if (ret) {
		ret = -EFAULT;
		log_err("app: cannot initialize uinput device\n");
		goto err;
	}

	for (i = 0; keys[i]; ++i) {
		ret = ioctl(app->uinput_fd, UI_SET_KEYBIT, keys[i]);
		if (ret) {
			ret = -EFAULT;
			log_err("app: cannot initialize uinput device\n");
			goto err;
		}
	}

	ret = ioctl(app->uinput_fd, UI_DEV_CREATE);
	if (ret) {
		ret = -EFAULT;
		log_err("app: cannot create uinput device\n");
		goto err;
	}

	return 0;

err:
	close(app->uinput_fd);
	app->uinput_fd = -1;
	return ret;
}

static void monitor_poll(struct app *app)
{
	char *dev;

	while ((dev = xwii_monitor_poll(app->monitor))) {
		log_info("app: new Wii Remote detected: %s\n", dev);
		free(dev);
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
	uinput_destroy(app);
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

	ret = uinput_init(app);
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
	app.uinput_fd = -1;

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
