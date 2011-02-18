/*
 * XWiimote - driver - bluetooth.c
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/l2cap.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
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

bool wii_bt_connect(const char *addr, unsigned int sync)
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

struct qele;
struct qele {
	struct qele *next;
	bdaddr_t addr;
	signed int in;
	signed int out;
	time_t last;
};

#define TYPE_IN 0
#define TYPE_OUT 1

static struct wii_log logger;
static struct qele *queue;

/* remove incomplete queue elements older than 5 seconds */
static void clean_queue()
{
	time_t now;
	struct qele *iter;
	char addr[20];

	now = time(NULL);
	iter = queue;
	while (iter) {
		if (iter->in != -1 || iter->out != -1) {
			if (iter->last < now - 5) {
				/* Last 5s no update, clear! */
				ba2str(&iter->addr, addr);
				wii_log_do(&logger, "Warning: Listener for %s timed out", addr);
				if (iter->in != -1)
					close(iter->in);
				if (iter->out != -1)
					close(iter->out);
				iter->in = -1;
				iter->out = -1;
			}
		}
		iter = iter->next;
	}
}

static void accept_conn(signed int fd, unsigned int type)
{
	signed int newfd;
	struct sockaddr_l2 l2addr;
	socklen_t size;
	struct qele *iter;
	char addr[20];
	struct wii_drv_io drv;

	size = sizeof(l2addr);
	newfd = accept(fd, (struct sockaddr*)&l2addr, &size);
	if (newfd < 0)
		return;

	/* look whether there is already an entry with this address */
	iter = queue;
	while (iter) {
		if (0 == bacmp(&iter->addr, &l2addr.l2_bdaddr))
			goto complete;
		iter = iter->next;
	}

	/*
	 * No entry available, yet. Find an empty one or create a new
	 * one and push it to the queue.
	 */
	iter = queue;
	while (iter) {
		if (iter->in == -1 && iter->out == -1)
			break;
		iter = iter->next;
	}
	if (!iter) {
		iter = malloc(sizeof(*iter));
		if (!iter) {
			/* simply drop this connection on memory failure */
			wii_log_do(&logger, "Warning: Memory allocation failed, dropping new fd %d", newfd);
			close(newfd);
			return;
		}
		iter->in = -1;
		iter->out = -1;
		iter->next = queue;
		queue = iter;
	}
	bacpy(&iter->addr, &l2addr.l2_bdaddr);

	/*
	 * There is (already) an entry with this address, complete it. If we got
	 * one descriptor twice, drop the old one and replace it with the new one.
	 * If the set is complete, start off a new driver on both descriptors,
	 * clear the queue entry and leave it empty.
	 */
complete:
	ba2str(&iter->addr, addr);
	iter->last = time(NULL);
	if (type == TYPE_IN) {
		if (iter->in != -1)
			close(iter->in);
		iter->in = newfd;
		wii_log_do(&logger, "New socket (in-fd %d) for %s", newfd, addr);
	}
	else {
		if (iter->out != -1)
			close(iter->out);
		iter->out = newfd;
		wii_log_do(&logger, "New socket (out-fd %d) for %s", newfd, addr);
	}
	if (iter->in != -1 && iter->out != -1) {
		/* this forks a new process */
		drv.in = iter->in;
		drv.out = iter->out;
		if (wii_fork(wii_start_driver, &drv))
			wii_log_do(&logger, "Spawning device driver for %s", addr);
		else
			wii_log_do(&logger, "Warning: Failed forking device driver for %s", addr);
		close(iter->in);
		close(iter->out);
		iter->in = -1;
		iter->out = -1;
	}
}

/*
 * Takes two listener sockets and listens on both for incoming
 * connections. If a remote device connects on both sockets,
 * then the driver is spawned.
 * If a remote device takes longer than 5 seconds between
 * connecting to both sockets, then the connection is dropped.
 */
static void run_listener(void *arg)
{
	struct wii_drv_io *drv = arg;
	struct pollfd fds[2];
	signed int num;
	size_t errcnt;
	time_t lasterr, now;

	fds[0].fd = drv->in;
	fds[0].events = POLLIN;
	fds[1].fd = drv->out;
	fds[1].events = POLLIN;

	errcnt = 0;
	lasterr = time(NULL);
	wii_log_open(&logger, "listener", false);
	wii_log_do(&logger, "Listener ready");

	do {
		if (wii_terminating(false)) {
			wii_log_do(&logger, "Error: Interrupted");
			goto cleanup;
		}
		num = poll(fds, 2, 5000);
		now = time(NULL);
		if (num < 0) {
			if (errno == EAGAIN || errno == EINTR) {
				if (lasterr < now - 5) {
					errcnt = 0;
					lasterr = now;
				}
				if (++errcnt > 50) {
					wii_log_do(&logger, "Error: poll() error threshold reached, errno %d", errno);
					goto cleanup;
				}
				continue;
			}
			wii_log_do(&logger, "Error: poll() failed, errno %d", errno);
			goto cleanup;
		}
		if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL) || fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
			wii_log_do(&logger, "Error: Socket hangup on listener");
			goto cleanup;
		}
		if (fds[0].revents & POLLIN)
			accept_conn(fds[0].fd, TYPE_IN);
		if (fds[1].revents & POLLIN)
			accept_conn(fds[1].fd, TYPE_OUT);
		clean_queue();
	} while(1);

cleanup:
	wii_log_do(&logger, "Listener closed");
	wii_log_close(&logger);
	close(drv->in);
	close(drv->out);
}

bool wii_bt_listen()
{
	signed int c1, c2;
	struct sockaddr_l2 l2;
	struct wii_drv_io drv;
	bool ret = false;

	printf("Setup listener for incoming connections...\n");
	c1 = -1;
	c2 = -1;

	c1 = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	c2 = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
	if (c1 < 0 || c2 < 0) {
		printf("Error: Cannot create l2cap sockets\n");
		goto cleanup;
	}

	memset(&l2, 0, sizeof(l2));
	l2.l2_family = AF_BLUETOOTH;

	l2.l2_psm = 0x11;
	if (bind(c1, (struct sockaddr*)&l2, sizeof(l2)) != 0) {
		printf("Error: Cannot bind l2cap socket\n");
		goto cleanup;
	}

	l2.l2_psm = 0x13;
	if (bind(c2, (struct sockaddr*)&l2, sizeof(l2)) != 0) {
		printf("Error: Cannot bind l2cap socket\n");
		goto cleanup;
	}

	if (listen(c1, 10) != 0 || listen(c2, 10) != 0) {
		printf("Error: Cannot listen on l2cap socket\n");
		goto cleanup;
	}

	drv.in = c2;
	drv.out = c1;
	ret = wii_fork(run_listener, &drv);
	if (ret)
		printf("Listener forked into background\n");
	else
		printf("Error: Cannot fork listener process\n");

cleanup:
	if (c1 >= 0)
		close(c1);
	if (c2 >= 0)
		close(c2);
	return ret;
}
