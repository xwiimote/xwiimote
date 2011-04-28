/*
 * XWiimote - tools - xwiiconn.c
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * XWiimote Connect
 * Connecting to the wiimote can be done with any bluez-compatible
 * bluetooth applet. However, if you don't have one, you can use this
 * tool which uses the bluez-dbus API to connect to the input device
 * on the wiimote.
 * This tool takes as argument the address of the remote device and
 * initiates pairing. If used with "-p" flag for permanent pairing, you
 * must press the red-sync button on the back of the wiimote for pairing.
 * If you don't specify the -p flag, then you must press 1+2 buttons on
 * the front of the wiimote.
 *
 * This tool uses the bluetooth address of the remote device as PIN if
 * -p is not specified, otherwise it uses the local bluetooth address
 * as PIN.
 * This works with all devices that need such PINs and thus is not
 * limited to wiimotes.
 *
 * If autoconnect is configured on the wiimote, then this tool is not
 * needed and you should either use bluez-simple-agent or your favour
 * bluetooth applet to allow incoming connections.
 *
 * Run this tool without arguments to see usage information.
 */

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <bluetooth/bluetooth.h>

enum {
	REQ_DEFADAPTER,
	REQ_PAIRING,
	REQ_INPUT,
	REQ_APROPS,
};

static GMainLoop *mainloop;
static DBusConnection *sysbus;
static dbus_uint32_t last_serial;
static int last_req;

static char *bt_opath;
static char bt_addr[18];
static const char *agent_path = "/our/agent";

static struct appconfig {
	bool is_bonded;
	bool debug;
	bool show_help;
	bool perm_sync;
	const char *addr;
} config = { .is_bonded = false, .debug = false, .show_help = true, .perm_sync = false, .addr = NULL };

static void stop_mainloop()
{
	g_main_loop_quit(mainloop);
}

static void request_input()
{
	char buf[100];
	DBusMessage *msg;
	dbus_bool_t ret;
	char addr[18];

	strncpy(addr, config.addr, sizeof(addr));
	addr[2] = addr[5] = addr[8] = addr[11] = addr[14] = '_';
	addr[17] = 0;
	snprintf(buf, sizeof(buf) - 1, "%s/dev_%s", bt_opath, addr);
	buf[sizeof(buf) - 1] = 0;

	msg = dbus_message_new_method_call("org.bluez", buf, "org.bluez.Input", "Connect");
	if (!msg)
		goto failure;

	ret = dbus_connection_send(sysbus, msg, &last_serial);
	dbus_message_unref(msg);
	if (!ret)
		goto failure;
	last_req = REQ_INPUT;
	return;

failure:
	fprintf(stderr, "Error: Cannot send message to system bus\n");
	stop_mainloop();
}

static void handle_input(DBusMessage *msg)
{
	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_RETURN)
		goto failure;
	fprintf(stderr, "Input connection established\n");
	stop_mainloop();
	return;

failure:
	fprintf(stderr, "Error: Couldn't establish input connection\n");
	stop_mainloop();
}

static void request_pairing()
{
	static const char *caps = "KeyboardOnly";
	DBusMessage *msg;
	dbus_bool_t ret;

	msg = dbus_message_new_method_call("org.bluez", bt_opath, "org.bluez.Adapter", "CreatePairedDevice");
	if (!msg)
		goto failure;
	dbus_message_append_args(msg, DBUS_TYPE_STRING, &config.addr,
					DBUS_TYPE_OBJECT_PATH, &agent_path,
					DBUS_TYPE_STRING, &caps,
					DBUS_TYPE_INVALID);

	ret = dbus_connection_send(sysbus, msg, &last_serial);
	dbus_message_unref(msg);
	if (!ret)
		goto failure;
	last_req = REQ_PAIRING;
	return;

failure:
	fprintf(stderr, "Error: Cannot send message to system bus\n");
	stop_mainloop();
}

static void handle_pairing(DBusMessage *msg)
{
	int type;
	char *obj;
	const char *member;
	char pin[14];
	char *pinptr = pin;
	bdaddr_t addr;
	char *err;
	DBusMessage *retmsg;
	dbus_bool_t ret;

	type = dbus_message_get_type(msg);
	member = dbus_message_get_member(msg);
	if (member && 0 == strcmp(member, "RequestPinCode")) {
		if (!(retmsg = dbus_message_new_method_return(msg)))
			goto failure;

		if (config.perm_sync)
			str2ba(bt_addr, &addr);
		else
			str2ba(config.addr, &addr);
		sprintf(pin, "$%2.2X%2.2X%2.2X%2.2X%2.2X%2.2X", addr.b[0], addr.b[1], addr.b[2], addr.b[3], addr.b[4], addr.b[5]);
		dbus_message_append_args(retmsg, DBUS_TYPE_STRING, &pinptr, DBUS_TYPE_INVALID);

		ret = dbus_connection_send(sysbus, retmsg, NULL);
		dbus_message_unref(retmsg);
		if (!ret)
			goto failure;
		fprintf(stderr, "Sending PIN code to device\n");
	}
	else if (member && 0 == strcmp(member, "Release")) {
		if (!(retmsg = dbus_message_new_method_return(msg)))
			goto failure;
		ret = dbus_connection_send(sysbus, retmsg, NULL);
		dbus_message_unref(retmsg);
		if (!ret)
			goto failure;
		fprintf(stderr, "Releasing agent\n");
	}
	else if (type == DBUS_MESSAGE_TYPE_ERROR) {
		if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &err, DBUS_TYPE_INVALID)) {
			if (0 == strcmp(err, "Already Exists")) {
				fprintf(stderr, "Warning: Pairing with that device already exists (use -b flag)\n");
				request_input();
			}
			else {
				fprintf(stderr, "BlueZ-Error: %s\n", err);
				goto failure;
			}
		}
		else {
			fprintf(stderr, "Error: Pairing failed\n");
			goto failure;
		}
	}
	else {
		if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &obj, DBUS_TYPE_INVALID))
			goto failure;
		if (!obj || !*obj)
			goto failure;
		fprintf(stderr, "Device pairing established to %s\n", obj);
		request_input();
	}
	return;

failure:
	fprintf(stderr, "Error: Received invalid return on DBus\n");
	stop_mainloop();
}

static void request_aprops()
{
	DBusMessage *msg;
	dbus_bool_t ret;

	msg = dbus_message_new_method_call("org.bluez", bt_opath, "org.bluez.Adapter", "GetProperties");
	if (!msg)
		goto failure;

	ret = dbus_connection_send(sysbus, msg, &last_serial);
	dbus_message_unref(msg);
	if (!ret)
		goto failure;
	last_req = REQ_APROPS;
	return;

failure:
	fprintf(stderr, "Error: Cannot send message to system bus\n");
	stop_mainloop();
}

static void handle_aprops(DBusMessage *msg)
{
	DBusMessageIter iter, subiter, dictiter, argiter;
	int type;
	char *str;

	if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_RETURN)
		goto failure;

	dbus_message_iter_init(msg, &iter);
	while ((type = dbus_message_iter_get_arg_type (&iter)) != DBUS_TYPE_INVALID) {
		if (type == DBUS_TYPE_ARRAY) {
			dbus_message_iter_recurse(&iter, &subiter);
			while ((type = dbus_message_iter_get_arg_type (&subiter)) != DBUS_TYPE_INVALID) {
				if (type == DBUS_TYPE_DICT_ENTRY) {
					dbus_message_iter_recurse(&subiter, &dictiter);
					dbus_message_iter_get_basic(&dictiter, &str);
					if (0 == strcmp(str, "Address") && dbus_message_iter_next(&dictiter)) {
						dbus_message_iter_recurse(&dictiter, &argiter);
						dbus_message_iter_get_basic(&argiter, &str);
						strncpy(bt_addr, str, sizeof(bt_addr) - 1);
					}
				}
				dbus_message_iter_next(&subiter);
			}
		}
		dbus_message_iter_next(&iter);
	}

	if (!*bt_addr)
		goto failure;
	fprintf(stderr, "Local addr: %s\n", bt_addr);
	request_pairing();
	return;

failure:
	fprintf(stderr, "Error: Couldn't get local bluetooth address\n");
	stop_mainloop();
}

static void request_defadapter()
{
	DBusMessage *msg;
	dbus_bool_t ret;

	msg = dbus_message_new_method_call("org.bluez", "/", "org.bluez.Manager", "DefaultAdapter");
	if (!msg)
		goto failure;
	ret = dbus_connection_send(sysbus, msg, &last_serial);
	dbus_message_unref(msg);
	if (!ret)
		goto failure;
	last_req = REQ_DEFADAPTER;
	return;

failure:
	fprintf(stderr, "Error: Cannot send message to system bus\n");
	stop_mainloop();
}

static void handle_defadapter(DBusMessage *msg)
{
	char *obj;

	if (!dbus_message_get_args(msg, NULL, DBUS_TYPE_OBJECT_PATH, &obj, DBUS_TYPE_INVALID))
		goto failure;
	if (!obj || !*obj)
		goto failure;

	free(bt_opath);
	bt_opath = strdup(obj);
	if (!bt_opath)
		goto failure;
	fprintf(stderr, "Received path to default adapter %s\n", bt_opath);

	/* if device is already bonded, skip pairing and start input request right away */
	if (config.is_bonded) {
		request_input();
	}
	else {
		if (config.perm_sync)
			request_aprops();
		else
			request_pairing();
	}
	return;

failure:
	fprintf(stderr, "Error: Received invalid return on DBus\n");
	stop_mainloop();
}

/* this prints a DBus message to stderr for debug purposes */
static void print_message(DBusMessage *msg)
{
	int type;
	char *str;
	const char *sig;

	type = dbus_message_get_type(msg);
	fprintf(stderr, "Got message: ");
	switch(type) {
		case DBUS_MESSAGE_TYPE_METHOD_CALL:
			fprintf(stderr, "method call\n");
			break;
		case DBUS_MESSAGE_TYPE_METHOD_RETURN:
			fprintf(stderr, "method return\n");
			break;
		case DBUS_MESSAGE_TYPE_ERROR:
			fprintf(stderr, "error\n");
			break;
		case DBUS_MESSAGE_TYPE_SIGNAL:
			fprintf(stderr, "signal\n");
			break;
		default:
			fprintf(stderr, "unknown\n");
			break;
	}

	fprintf(stderr, "path: %s\n", dbus_message_get_path(msg));
	fprintf(stderr, "interface: %s\n", dbus_message_get_interface(msg));
	fprintf(stderr, "member: %s\n", dbus_message_get_member(msg));
	fprintf(stderr, "destination: %s\n", dbus_message_get_destination(msg));
	fprintf(stderr, "sender: %s\n", dbus_message_get_sender(msg));
	sig = dbus_message_get_signature(msg);
	fprintf(stderr, "signature: %s\n", sig);
	if (sig && *sig == 's') {
		dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &str, DBUS_TYPE_INVALID);
		fprintf(stderr, "str-arg: %s\n", str);
	}
	fprintf(stderr, "\n");
}

static DBusHandlerResult sysbus_filter(DBusConnection *conn, DBusMessage *msg, void *data)
{
	int type;
	const char *path;
	const char *member;
	const char *interface;
	dbus_uint32_t serial;

	if (config.debug)
		print_message(msg);

	type = dbus_message_get_type(msg);
	path = dbus_message_get_path(msg);
	member = dbus_message_get_member(msg);
	interface = dbus_message_get_interface(msg);
	serial = dbus_message_get_reply_serial(msg);

	if (type == DBUS_MESSAGE_TYPE_SIGNAL &&
		path && 0 == strcmp(path, "/org/freedesktop/DBus") &&
		member && 0 == strcmp(member, "NameAcquired")) {
		request_defadapter();
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	else if (type == DBUS_MESSAGE_TYPE_METHOD_CALL &&
		interface && 0 == strcmp(interface, "org.bluez.Agent") &&
		path && 0 == strcmp(path, agent_path)) {
		handle_pairing(msg);
		return DBUS_HANDLER_RESULT_HANDLED;
	}
	else if ((type == DBUS_MESSAGE_TYPE_METHOD_RETURN ||
			type == DBUS_MESSAGE_TYPE_ERROR) &&
			last_serial && serial == last_serial) {
		switch (last_req) {
			case REQ_DEFADAPTER:
				handle_defadapter(msg);
				break;
			case REQ_PAIRING:
				handle_pairing(msg);
				break;
			case REQ_INPUT:
				handle_input(msg);
				break;
			case REQ_APROPS:
				handle_aprops(msg);
				break;
			default:
				fprintf(stderr, "Warning: Invalid return received on DBus\n");
		}
		return DBUS_HANDLER_RESULT_HANDLED;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

/*
 * Prepares the mainloop and connects to system dbus. All bluez messages are
 * requested and filtered in \sysbus_filter.
 */
static int prepare_mainloop()
{
	DBusError err;

	if (!dbus_threads_init_default()) {
		fprintf(stderr, "Error: Cannot initialize threaded dbus library\n");
		return EXIT_FAILURE;
	}

	mainloop = g_main_loop_new(NULL, FALSE);
	if (!mainloop) {
		fprintf(stderr, "Error: Cannot create glib mainloop\n");
		return EXIT_FAILURE;
	}

	dbus_error_init(&err);
	sysbus = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (dbus_error_is_set(&err)) {
		dbus_error_free(&err);
		g_main_loop_unref(mainloop);
		mainloop = NULL;
		fprintf(stderr, "Error: Cannot connect to DBus system bus\n");
		return EXIT_FAILURE;
	}

	dbus_connection_set_exit_on_disconnect(sysbus, TRUE);
	dbus_connection_setup_with_g_main(sysbus, NULL);
	dbus_connection_add_filter(sysbus, sysbus_filter, NULL, NULL);

	dbus_bus_add_match(sysbus, "sender='org.bluez'", &err);
	if (dbus_error_is_set(&err)) {
		dbus_error_free(&err);
		g_main_loop_unref(mainloop);
		mainloop = NULL;
		fprintf(stderr, "Error: Cannot add DBus filter\n");
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

static void free_mainloop()
{
	g_main_loop_unref(mainloop);
	mainloop = NULL;
	dbus_shutdown();
}

/* initializes the mainloop and runs it */
static int run()
{
	int ret;

	ret = prepare_mainloop();
	if (ret)
		return ret;

	g_main_loop_run(mainloop);

	free_mainloop();
	return EXIT_SUCCESS;
}

/*
 * Main
 * Parses the argument list and prints the usage information if no argument
 * is given or and invalid combination was passed.
 * Parsed arguments are stored in the global variable \config and then
 * the \run function is called.
 */
int main(int argc, char **argv)
{
	int i, j;
	int ret;
	size_t need_addr;

	need_addr = 0;
	for (i = 1; i < argc; ++i) {
		if (*argv[i] == '-') {
			for (j = 1; argv[i][j]; ++j) {
				if (argv[i][j] == 'd')
					config.debug = true;
				else if (argv[i][j] == 'b')
					config.is_bonded = true;
				else if (argv[i][j] == 'r')
					config.show_help = false;
				else if (argv[i][j] == 'a')
					++need_addr;
				else if (argv[i][j] == 'p')
					config.perm_sync = true;
				/* TODO: Remove this! This is my wiimote address. */
				else if (argv[i][j] == 'D')
					config.addr = "00:1E:35:3B:7E:6D";
				else
					fprintf(stderr, "Warning: Invalid argument %c\n", argv[i][j]);
			}
		}
		else {
			if (*argv[i] && need_addr) {
				--need_addr;
				config.addr = argv[i];
			}
			else {
				fprintf(stderr, "Warning: Invalid parameter %s\n", argv[i]);
			}
		}
	}

	if (!config.addr) {
		fprintf(stderr, "Error: No target address specified\n");
		config.show_help = true;
	}

	if (config.show_help) {
		fprintf(stderr, "Usage: %s [-dbrap] [addr]\n", argv[0]);
		fprintf(stderr, "Connects to a nearby wiimote\n");
		fprintf(stderr, "During pairing you must press the 1+2 buttons on the wiimote\n");
		fprintf(stderr, "Without arguments, this help text is shown\n");
		fprintf(stderr, "-r argument must always be present (run)\n");
		fprintf(stderr, "-a [addr] specifies the remote device bluetooth address\n");
		fprintf(stderr, "-b skips pairing (device must already be paired)\n");
		fprintf(stderr, "-d enables debug mode (very verbose!)\n");
		fprintf(stderr, "-p enables permanent pairing, that is, you must press the\n");
		fprintf(stderr, "   red-sync button instead of 1+2 buttons on the wiimote for pairing\n");
		fprintf(stderr, "   and then auto-reconnect on the wiimote will be enabled\n");
		ret = EXIT_FAILURE;
	}
	else {
		ret = run();
	}

	return ret;
}
