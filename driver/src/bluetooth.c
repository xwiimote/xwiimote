/*
 * XWiimote - driver - bluetooth.c
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <unistd.h>
#include "bluetooth.h"
#include "log.h"
#include "main.h"

/* constants */
#define MAX_RES 100 /* max inquiry results */
#define BA_LEN 18 /* BD address length */
#define NAME_LEN 250 /* BT devname length */

/* global inquiry results list */
static struct {
	char addr[BA_LEN];
	char name[NAME_LEN];
} devlist[MAX_RES];
static size_t devllen;

const char *wii_bt_inquiry(unsigned int flags)
{
	inquiry_info res[MAX_RES], *ptr;
	signed int num, i, sock = -1;
	size_t l, id;
	char *addr = NULL;

	printf("Performing inquiry, please wait...\n");

	ptr = res;
	sock = hci_open_dev(hci_get_route(NULL));

	/* perform inquiry */
	num = hci_inquiry(-1, (flags & WII_BT_LONG)?24:8, MAX_RES, NULL, &ptr, (flags & WII_BT_CACHE)?0:IREQ_CACHE_FLUSH);
	if (num < 0 && errno != EINTR) {
		printf("Error: Cannot access HCI socket to perform inquiry\n");
		goto failure;
	}

	devllen = 0;
	if (num < 1) {
		printf("Error: No devices found\n");
		goto failure;
	}

	if (flags & WII_BT_NAMES)
		printf("Resolving device names, please wait...\n");

	for (i = 0; i < num && devllen < MAX_RES; ++i) {
		ba2str(&(res + i)->bdaddr, devlist[devllen].addr);
		if (!(flags & WII_BT_NAMES) || hci_read_remote_name(sock, &(res + i)->bdaddr, NAME_LEN, devlist[devllen].name, 5) < 0)
			strncpy(devlist[devllen].name, "<unknown>", NAME_LEN);
		devlist[devllen].name[NAME_LEN - 1] = 0;
		++devllen;
		if (wii_terminating(true))
			goto failure;
	}

	/* print results */
	printf("Following devices were found during inquiry:\n");
	for (l = 0; l < devllen; ++l) {
		printf("  (id %lu) %s (%s)\n", l + 1, devlist[l].addr, devlist[l].name);
	}
	if (!devllen)
		printf("  none\n");
	printf("End of List\n");

	if (devllen && (flags & WII_BT_UI)) {
		do {
			printf("Select device (by id): ");
			fflush(stdout);
			scanf("%lu", &id);
			if (id-- && id < devllen)
				addr = devlist[id].addr;
			else
				printf("Invalid id\n");
			if (wii_terminating(true))
				goto failure;
		} while(!addr);
	}

failure:
	if (sock >= 0)
		close(sock);
	return addr;
}
