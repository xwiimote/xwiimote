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
#include <bluetooth/l2cap.h>
#include <sys/ioctl.h>
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

bool wii_bt_connect(const char *addr, unsigned int detach, unsigned int sync)
{
	struct sockaddr_l2 l2addr;
	signed int sock, hci, devid, c1, c2;
	struct hci_conn_info_req *req_ci;
	auth_requested_cp req_auth;
	bdaddr_t req_neg, local_addr;
	pin_code_reply_cp req_pin;
	struct wii_drv_io drv;
	unsigned int ret = false;

	hci = -1;
	sock = -1;
	c1 = -1;
	c2 = -1;

	printf("Connecting to %s...\n", addr);

	if (-1 == (devid = hci_get_route(NULL))) {
		printf("Error: Cannot access local bluetooth socket\n");
		goto failure;
	}
	if (0 != hci_devba(devid, &local_addr)) {
		printf("Error: Cannot get local bluetooth address\n");
		goto failure;
	}
	if(0 > (hci = hci_open_dev(devid))) {
		printf("Error: Cannot open HCI socket\n");
		goto failure;
	}

	sock = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_L2CAP);
	if (sock < 0) {
		printf("Error: Cannot create bluetooth socket\n");
		goto failure;
	}

	memset(&l2addr, 0, sizeof(l2addr));
	l2addr.l2_family = AF_BLUETOOTH;
	l2addr.l2_psm = 0;
	bacpy(&l2addr.l2_bdaddr, &local_addr);
	if (0 != bind(sock, (struct sockaddr*)&l2addr, sizeof(l2addr))) {
		printf("Error: Cannot bind bluetooth socket\n");
		goto failure;
	}

	memset(&l2addr, 0, sizeof(l2addr));
	l2addr.l2_family = AF_BLUETOOTH;
	l2addr.l2_psm = 0;
	str2ba(addr, &l2addr.l2_bdaddr);
	if (0 != connect(sock, (struct sockaddr*)&l2addr, sizeof(l2addr))) {
		printf("Error: Couldn't connect to target device\n");
		goto failure;
	}

	req_ci = malloc(sizeof(*req_ci) + sizeof(struct hci_conn_info));
	str2ba(addr, &req_ci->bdaddr);
	req_ci->type = ACL_LINK;
	if (0 != ioctl(hci, HCIGETCONNINFO, (unsigned long)req_ci)) {
		printf("Error: Couldn't get connection info\n");
		goto failure;
	}

	memset(&req_auth, 0, sizeof(req_auth));
	req_auth.handle = req_ci->conn_info->handle;
	if (0 != hci_send_cmd(hci, OGF_LINK_CTL, OCF_AUTH_REQUESTED, AUTH_REQUESTED_CP_SIZE, &req_auth)) {
		printf("Error: Auth request failed\n");
		goto failure;
	}

	memset(&req_neg, 0, sizeof(req_neg));
	str2ba(addr, &req_neg);
	if (0 != hci_send_cmd(hci, OGF_LINK_CTL, OCF_LINK_KEY_NEG_REPLY, sizeof(req_neg), &req_neg)) {
		printf("Error: Link key negation failed\n");
		goto failure;
	}

	memset(&req_pin, 0, sizeof(req_pin));
	str2ba(addr, &req_pin.bdaddr);
	req_pin.pin_len = 6;
	if (sync)
		memcpy(req_pin.pin_code, &local_addr, 6);
	else
		memcpy(req_pin.pin_code, &req_pin.bdaddr, 6);
	if (0 != hci_send_cmd(hci, OGF_LINK_CTL, OCF_PIN_CODE_REPLY, PIN_CODE_REPLY_CP_SIZE, &req_pin)) {
		printf("Error: Pin code transmission failed\n");
		goto failure;
	}

	c1 = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	c2 = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if(c1 < 0 || c2 < 0) {
		printf("Error: Cannot create l2cap sockets\n");
		goto failure;
	}
	memset(&l2addr, 0, sizeof(l2addr));
	l2addr.l2_family = AF_BLUETOOTH;
	str2ba(addr, &l2addr.l2_bdaddr);

	l2addr.l2_psm = 0x11;
	if (0 != connect(c1, (struct sockaddr*)&l2addr, sizeof(l2addr))) {
		printf("Error: Cannot open l2cap channel on psm 0x11\n");
		goto failure;
	}
	l2addr.l2_psm = 0x13;
	if (0 != connect(c2, (struct sockaddr*)&l2addr, sizeof(l2addr))) {
		printf("Error: Cannot open l2cap channel on psm 0x13\n");
		goto failure;
	}

	printf("Connection established\n");

	drv.in = c2;
	drv.out = c1;
	ret = wii_fork(wii_start_driver, &drv);
	if (!ret)
		printf("Error: Cannot fork driver process\n");

failure:
	if (c2 >= 0)
		close(c2);
	if (c1 >= 0)
		close(c1);
	if (sock >= 0)
		close(sock);
	if (hci >= 0)
		close(hci);
	return ret;
}
