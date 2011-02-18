/*
 * XWiimote - driver - main.c
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include "bluetooth.h"
#include "log.h"
#include "main.h"

static struct {
	unsigned int cmd_help : 1;
	unsigned int cmd_connect : 1;
	unsigned int cmd_listen : 1;

	unsigned int arg_longi : 1;
	unsigned int arg_detach : 1;
	unsigned int arg_sync : 1;
	unsigned int arg_addr : 1;
	const char *addr;
} ui_params;

static bool wii_terminate = false;

bool wii_terminating(bool print)
{
	if (wii_terminate && print)
		printf("Error: Interrupted\n");
	return wii_terminate;
}

static void ui_signal(signed int sig)
{
	wii_terminate = true;
}

bool wii_fork(void (*func)(void *arg), void *arg)
{
	signed int ret;

	ret = fork();
	if (ret < 0) {
		return false;
	}
	else if (ret == 0) {
		ret = fork();
		if (ret == 0)
			func(arg);
		exit(0);
	}
	else {
		if (-1 == waitpid(ret, NULL, 0))
			return false;
	}
	return true;
}

static signed int ui_help()
{
	printf("Open Source Nintendo Wii Remote Linux Device Driver\n");
	printf("Usage:\n");
	printf("  %s -hcl -lsda [ADDR]\n", "xwiimote");
	printf("\n");
	printf("Commands:\n");
	printf("  h: Show this help\n");
	printf("  c: Connect to remote xwiimote\n");
	printf("  l: Listen for new xwiimote connections\n");
	printf("\n");
	printf("Connect Arguments:\n");
	printf("  l: Perform long inquiry (30 instead of 10 seconds)\n");
	printf("  d: Detach driver into background\n");
	printf("  s: Perform sync with remote\n");
	printf("  a: Use given address instead of performing an inquiry\n");
	printf("Listen Arguments:\n");
	printf("  d: Detach driver into background\n");
	printf("\n");
	return EXIT_FAILURE;
}

static bool ui_parse(signed int argc, char **argv)
{
	size_t i;

	if (argc > 3) {
		if (argv[3][0])
			ui_params.addr = argv[3];
	}

	if (argc > 2) {
		for (i = 0; argv[2][i]; ++i) {
			switch (argv[2][i]) {
			case 'l':
				ui_params.arg_longi = 1;
				break;
			case 's':
				ui_params.arg_sync = 1;
				break;
			case 'd':
				ui_params.arg_detach = 1;
				break;
			case 'a':
				ui_params.arg_addr = 1;
				break;
			case '-':
				break;
			default:
				printf("Warning: Invalid argument '%c'\n", argv[2][i]);
				break;
			}
		}
	}

	if (argc > 1) {
		for (i = 0; argv[1][i]; ++i) {
			switch (argv[1][i]) {
			case 'h':
				ui_params.cmd_help = 1;
				break;
			case 'c':
				ui_params.cmd_connect = 1;
				break;
			case 'l':
				ui_params.cmd_listen = 1;
				break;
			case '-':
				break;
			default:
				printf("Warning: Invalid command '%c'\n", argv[1][i]);
				break;
			}
		}
	}

	if (ui_params.arg_addr && !ui_params.addr) {
		ui_params.cmd_help = 1;
		printf("Error: Missing address argument\n");
	}

	if (ui_params.cmd_connect == ui_params.cmd_listen) {
		ui_help();
		return false;
	}
	if (ui_params.cmd_help) {
		ui_help();
		return false;
	}

	return true;
}

static signed int ui_listen()
{
	return EXIT_SUCCESS;
}

static signed int ui_connect()
{
	const char *dev;
	unsigned int i;
	unsigned int arg;

	if (ui_params.arg_addr && ui_params.addr) {
		dev = ui_params.addr;
	} else {
		arg = WII_BT_NAMES | WII_BT_CACHE | WII_BT_UI;
		if (ui_params.arg_longi)
			arg |= WII_BT_LONG;
		dev = wii_bt_inquiry(arg);
	}

	if (dev) {
		i = 0;
		while (1) {
			/* TODO: connect to remote device */
			if (wii_terminating(true))
				break;
			if (i++ == 3)
				break;
			printf("Trying again in 3seconds...\n");
			sleep(3);
			if (wii_terminating(true))
				break;
		}
		return EXIT_SUCCESS;
	}
	else {
		printf("Error: No device to connect to\n");
		return EXIT_FAILURE;
	}
}

signed int main(signed int argc, char **argv)
{
	signal(SIGINT, ui_signal);
	signal(SIGHUP, ui_signal);
	signal(SIGQUIT, ui_signal);
	signal(SIGTERM, ui_signal);

	if (!ui_parse(argc, argv))
		return EXIT_FAILURE;

	if (ui_params.cmd_listen)
		return ui_listen();
	else if (ui_params.cmd_connect)
		return ui_connect();
	else
		return ui_help();
}
