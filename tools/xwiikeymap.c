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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "eloop.h"
#include "log.h"
#include "xwiimote.h"

struct app {
	struct ev_eloop *eloop;
	struct ev_signal *sig_term;
	struct ev_signal *sig_int;
	struct xwii_monitor *monitor;
	int monitor_fd;
	struct ev_fd *monitor_fdo;
	bool daemon;
};

struct dev {
	struct xwii_iface *iface;
	int iface_fd;
	struct ev_fd *iface_fdo;
	int uinput_fd;
};

static const char *uinput_path = "/dev/uinput";
static const char *uinput_name = "XWiimote Keyboard";

static uint16_t mapping[] = {
	[XWII_KEY_LEFT] = KEY_LEFT,
	[XWII_KEY_RIGHT] = KEY_RIGHT,
	[XWII_KEY_UP] = KEY_UP,
	[XWII_KEY_DOWN] = KEY_DOWN,
	[XWII_KEY_A] = KEY_ENTER,
	[XWII_KEY_B] = KEY_SPACE,
	[XWII_KEY_PLUS] = KEY_VOLUMEUP,
	[XWII_KEY_MINUS] = KEY_VOLUMEDOWN,
	[XWII_KEY_HOME] = KEY_ESC,
	[XWII_KEY_ONE] = KEY_1,
	[XWII_KEY_TWO] = KEY_2,
	[XWII_KEY_NUM] = 0,
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

	dev->uinput_fd = open(uinput_path, O_RDWR | O_CLOEXEC | O_NONBLOCK);
	if (dev->uinput_fd < 0) {
		ret = -errno;
		log_err("app: cannot open uinput device %s: %m (%d)\n",
							uinput_path, ret);
		return ret;
	}

	memset(&udev, 0, sizeof(udev));
	strncpy(udev.name, uinput_name, UINPUT_MAX_NAME_SIZE);
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

	for (i = 0; i < XWII_KEY_NUM; ++i) {
		if (!mapping[i])
			continue;

		ret = ioctl(dev->uinput_fd, UI_SET_KEYBIT, mapping[i]);
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

static void handle_data(struct dev *dev, struct xwii_event *ev)
{
	struct input_event output;

	if (ev->type != XWII_EVENT_KEY)
		return;

	if (ev->v.key.code >= XWII_KEY_NUM || !mapping[ev->v.key.code])
		return;

	memset(&output, 0, sizeof(output));
	output.type = EV_KEY;
	output.code = mapping[ev->v.key.code];
	output.value = ev->v.key.state;
	gettimeofday(&output.time, NULL);

	if (write(dev->uinput_fd, &output, sizeof(output)) != sizeof(output))
		log_warn("app: cannot write to uinput device\n");
}

static void device_event(struct ev_fd *fdo, int mask, void *data)
{
	struct dev *dev = data;
	struct xwii_event event;
	int ret;
	struct input_event ev;

	if (mask & (EV_HUP | EV_ERR)) {
		log_err("app: Wii Remote device closed\n");
		destroy_device(dev);
	} else if (mask & EV_READABLE) {
		while (1) {
			ret = xwii_iface_poll(dev->iface, &event);
			if (ret != -EAGAIN) {
				if (ret) {
					log_err("app: reading Wii Remote "
						"failed; closing device...\n");
					destroy_device(dev);
					return;
				} else {
					handle_data(dev, &event);
				}
			} else {
				break;
			}
		}

		memset(&ev, 0, sizeof(ev));
		ev.type = EV_SYN;
		ev.code = SYN_REPORT;
		ev.value = 0;
		gettimeofday(&ev.time, NULL);

		if (write(dev->uinput_fd, &ev, sizeof(ev)) != sizeof(ev))
			log_warn("app: cannot write to uinput device\n");
	}
}

static void monitor_poll(struct app *app)
{
	char *str;
	struct dev *dev;
	int ret;

	while ((str = xwii_monitor_poll(app->monitor))) {
		/* sleep short time (10ms) to have the device fully setup */
		usleep(10000);

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

struct arg {
	char name_s;		/* short argument name */
	const char *name_l;	/* long argument name */
	bool need_arg;		/* does it require an argument? */
	const char *arg;	/* the parsed argument or "no_value" */
};

struct arg args[] = {
	{ 'L', "left", true, NULL },
	{ 'R', "right", true, NULL },
	{ 'U', "up", true, NULL },
	{ 'D', "down", true, NULL },
	{ 'A', "a", true, NULL },
	{ 'B', "b", true, NULL },
	{ 'P', "plus", true, NULL },
	{ 'M', "minus", true, NULL },
	{ 'H', "home", true, NULL },
	{ 'O', "one", true, NULL },
	{ 'T', "two", true, NULL },
	{ 'u', "uinput", true, NULL },
	{ 'n', "name", true, NULL },
	{ 'd', "daemon", false, NULL },
	{ 0, NULL, false, NULL },
};

static const char *no_value = "no value";

static void parse_args(struct app *app, int argc, char **argv)
{
	int i, j;
	bool handled;
	char *v;
	struct arg *arg;

	for (i = 0; i < argc; ++i) {
		if (!argv[i][0])
			continue;

		handled = false;

		if (argv[i][0] == '-' && argv[i][1] == '-') {
			v = &argv[i][2];
			for (j = 0; args[j].name_s || args[j].name_l; ++j) {
				arg = &args[j];
				if (!arg->name_l)
					continue;
				if (strcmp(v, arg->name_l))
					continue;

				handled = true;
				if (!arg->need_arg) {
					arg->arg = no_value;
					break;
				}
				++i;
				if (i >= argc)
					log_warn("app: missing arg for %s\n",
									v);
				else
					arg->arg = argv[i];
			}
		} else if (argv[i][0] == '-') {
			v = &argv[i][1];
			for (j = 0; args[j].name_s || args[j].name_l; ++j) {
				arg = &args[j];
				if (arg->name_s != *v)
					continue;

				handled = true;
				if (!arg->need_arg) {
					arg->arg = no_value;
					break;
				}
				++i;
				if (i >= argc)
					log_warn("app: missing arg for %s\n",
									v);
				else
					arg->arg = argv[i];
			}
		}

		if (!handled)
			log_warn("app: unhandled argument '%s'\n", argv[i]);
	}

	if (args[0].arg)
		mapping[XWII_KEY_LEFT] = atoi(args[0].arg);
	if (args[1].arg)
		mapping[XWII_KEY_RIGHT] = atoi(args[1].arg);
	if (args[2].arg)
		mapping[XWII_KEY_UP] = atoi(args[2].arg);
	if (args[3].arg)
		mapping[XWII_KEY_DOWN] = atoi(args[3].arg);
	if (args[4].arg)
		mapping[XWII_KEY_A] = atoi(args[4].arg);
	if (args[5].arg)
		mapping[XWII_KEY_B] = atoi(args[5].arg);
	if (args[6].arg)
		mapping[XWII_KEY_PLUS] = atoi(args[6].arg);
	if (args[7].arg)
		mapping[XWII_KEY_MINUS] = atoi(args[7].arg);
	if (args[8].arg)
		mapping[XWII_KEY_HOME] = atoi(args[8].arg);
	if (args[9].arg)
		mapping[XWII_KEY_ONE] = atoi(args[9].arg);
	if (args[10].arg)
		mapping[XWII_KEY_TWO] = atoi(args[10].arg);
	if (args[11].arg)
		uinput_path = args[11].arg;
	if (args[12].arg)
		uinput_name = args[12].arg;
	if (args[13].arg)
		app->daemon = true;
}

int main(int argc, char **argv)
{
	int ret;
	struct app app;

	log_info("app: initializing\n");

	memset(&app, 0, sizeof(app));
	if (argc > 1)
		parse_args(&app, argc - 1, &argv[1]);

	if (app.daemon) {
		log_info("app: forking into background\n");
		daemon(0, 0);
	}

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
