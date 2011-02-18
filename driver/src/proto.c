/*
 * XWiimote - driver - proto.c
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "proto.h"

void wii_proto_init(struct wii_proto_dev *dev)
{
	memset(dev, 0, sizeof(*dev));
	dev->units |= WII_PROTO_CU_STATUS;
}

void wii_proto_deinit(struct wii_proto_dev *dev)
{
	struct wii_proto_buf *tmp;

	while (dev->buf_list) {
		tmp = dev->buf_list;
		dev->buf_list = tmp->next;
		free(tmp);
	}
	while (dev->buf_free) {
		tmp = dev->buf_free;
		dev->buf_free = tmp->next;
		free(tmp);
	}
}

bool wii_proto_encode(struct wii_proto_dev *dev, struct wii_proto_buf *buf)
{
	struct wii_proto_buf *ele;

	if (dev->buf_list) {
		ele = dev->buf_list;
		dev->buf_list = ele->next;
		memcpy(buf, ele, sizeof(*buf));

		/* set rumble flag on all outputs if enabled */
		if (buf->size >= 3 && dev->cache.rumble.on)
			buf->buf[2] |= 0x1;

		ele->next = dev->buf_free;
		dev->buf_free = ele;
		return true;
	}
	else {
		return false;
	}
}

static struct wii_proto_buf *wii__push(struct wii_proto_dev *dev)
{
	struct wii_proto_buf *req, *iter;
	static struct wii_proto_buf null;

	if (dev->buf_free) {
		req = dev->buf_free;
		dev->buf_free = req->next;
	}
	else {
		req = malloc(sizeof(*req));
		if (!req) {
			/*
			 * If memory allocation fails, return pointer to
			 * static structure that is discarded and
			 * not actually pushed to the request list.
			 */
			memset(&null, 0, sizeof(null));
			return &null;
		}
	}

	memset(req, 0, sizeof(*req));
	if (dev->buf_list) {
		iter = dev->buf_list;
		while (iter->next)
			iter = iter->next;
		iter->next = req;
	}
	else {
		dev->buf_list = req;
	}

	return req;
}

static inline void wii__mkcmd(struct wii_proto_buf *out, unsigned int rep, void *payload, size_t size)
{
	out->size = 2 + size;
	out->buf[0] = WII_PROTO_SH_CMD_OUT;
	out->buf[1] = rep;
	if (size)
		memcpy(&out->buf[2], payload, size);
}

void wii_proto_enable(struct wii_proto_dev *dev, wii_proto_mask_t units)
{
	if (units & WII_PROTO_CU_INPUT)
		dev->units |= WII_PROTO_CU_INPUT;
	if (units & WII_PROTO_CU_ACCEL)
		dev->units |= WII_PROTO_CU_ACCEL;

	wii_proto_do_format(dev);
}

void wii_proto_disable(struct wii_proto_dev *dev, wii_proto_mask_t units)
{
	if (units & WII_PROTO_CU_INPUT)
		dev->units &= ~WII_PROTO_CU_INPUT;
	if (units & WII_PROTO_CU_ACCEL)
		dev->units &= ~WII_PROTO_CU_ACCEL;

	wii_proto_do_format(dev);
}

void wii_proto_do_led(struct wii_proto_dev *dev, const struct wii_proto_cc_led *pl)
{
	struct wii_proto_buf *cmd;
	struct wii_proto_sr_led raw;

	cmd = wii__push(dev);
	raw.common.flags = 0;
	if (pl->one)
		raw.common.flags |= WII_PROTO_SR_COMMON_LED1;
	if (pl->two)
		raw.common.flags |= WII_PROTO_SR_COMMON_LED2;
	if (pl->three)
		raw.common.flags |= WII_PROTO_SR_COMMON_LED3;
	if (pl->four)
		raw.common.flags |= WII_PROTO_SR_COMMON_LED4;
	wii__mkcmd(cmd, WII_PROTO_SR_LED, &raw, sizeof(raw));
	memcpy(&dev->cache.led, pl, sizeof(dev->cache.led));
}

void wii_proto_do_rumble(struct wii_proto_dev *dev, const struct wii_proto_cc_rumble *pl)
{
	/*
	 * Rumble is automatically set on every request, however, if
	 * there is no request in the list, we need to create one.
	 * We simply resend the led-state here.
	 */
	if (!dev->buf_list)
		wii_proto_do_led(dev, &dev->cache.led);
	memcpy(&dev->cache.rumble, pl, sizeof(dev->cache.rumble));
}

void wii_proto_do_query(struct wii_proto_dev *dev)
{
	struct wii_proto_buf *cmd;
	struct wii_proto_sr_query raw;

	cmd = wii__push(dev);
	raw.common.flags = 0;
	wii__mkcmd(cmd, WII_PROTO_SR_QUERY, &raw, sizeof(raw));
}

void wii_proto_do_format(struct wii_proto_dev *dev)
{
	struct wii_proto_buf *cmd;
	struct wii_proto_sr_format raw;

	cmd = wii__push(dev);
	raw.common.flags = 0;
	if (wii_proto_enabled(dev, (WII_PROTO_CU_INPUT | WII_PROTO_CU_ACCEL)))
		raw.mode = WII_PROTO_SR_KA;
	else
		raw.mode = WII_PROTO_SR_K;
	wii__mkcmd(cmd, WII_PROTO_SR_FORMAT, &raw, sizeof(raw));
}

void wii_proto_do_acalib(struct wii_proto_dev *dev, const struct wii_proto_cc_acalib *pl)
{
	if (!wii_proto_enabled(dev, WII_PROTO_CU_ACCEL))
		return;

	memcpy(&dev->cache.acalib, pl, sizeof(dev->cache.acalib));
}

void wii_proto_do(struct wii_proto_dev *dev, const struct wii_proto_res *res)
{
	if (wii_proto_enabled(dev, WII_PROTO_CU_ACCEL)) {
		if (res->modified & WII_PROTO_CC_ACALIB)
			wii_proto_do_acalib(dev, &res->acalib);
	}

	/* WII_PROTO_CU_STATUS is always enabled */
	{
		if (res->modified & WII_PROTO_CC_LED)
			wii_proto_do_led(dev, &res->led);
		if (res->modified & WII_PROTO_CC_QUERY)
			wii_proto_do_query(dev);
		if (res->modified & WII_PROTO_CC_FORMAT)
			wii_proto_do_format(dev);
		if (res->modified & WII_PROTO_CC_RUMBLE)
			wii_proto_do_rumble(dev, &res->rumble);
	}

	/*
	 * Rumble should be the last command sent, so rumble-overhead
	 * is kept small.
	 */
}

static void wii__key(struct wii_proto_dev *dev, const struct wii_proto_sr_key *key, struct wii_proto_res *res)
{
	if (!wii_proto_enabled(dev, WII_PROTO_CU_INPUT))
		return;

	res->modified |= WII_PROTO_CR_KEY;
	res->key.left = !!(key->k1 & WII_PROTO_SR_KEY1_LEFT);
	res->key.right = !!(key->k1 & WII_PROTO_SR_KEY1_RIGHT);
	res->key.down = !!(key->k1 & WII_PROTO_SR_KEY1_DOWN);
	res->key.up = !!(key->k1 & WII_PROTO_SR_KEY1_UP);
	res->key.plus = !!(key->k1 & WII_PROTO_SR_KEY1_PLUS);
	res->key.two = !!(key->k2 & WII_PROTO_SR_KEY2_TWO);
	res->key.one = !!(key->k2 & WII_PROTO_SR_KEY2_ONE);
	res->key.b = !!(key->k2 & WII_PROTO_SR_KEY2_B);
	res->key.a = !!(key->k2 & WII_PROTO_SR_KEY2_A);
	res->key.minus = !!(key->k2 & WII_PROTO_SR_KEY2_MINUS);
	res->key.home = !!(key->k2 & WII_PROTO_SR_KEY2_HOME);
}

static void wii__accel(struct wii_proto_dev *dev, const struct wii_proto_sr_key *key, const uint8_t accel[3], struct wii_proto_res *res)
{
	uint16_t x, y, z;

	if (!wii_proto_enabled(dev, WII_PROTO_CU_ACCEL))
		return;

	res->modified |= WII_PROTO_CR_MOVE;

	/* assemble the 10 bit unsigned integers */
	x = accel[0];
	x <<= 2;
	x |= (key->k1 & (WII_PROTO_SR_KEY1_X6 | WII_PROTO_SR_KEY1_X7)) >> 5;
	y = accel[1];
	y <<= 2;
	y |= (key->k2 & WII_PROTO_SR_KEY2_X6) >> 4;
	z = accel[2];
	z <<= 2;
	z |= (key->k2 & WII_PROTO_SR_KEY2_X7) >> 5;

	/*
	 * Apply calibration data and convert to signed integer. The raw input has
	 * 10 bits or precision and is unsigned, so subtract a five bit integer (0x200)
	 * to convert to signed integer and apply calibration data.
	 */
	res->move.x = x - 0x200 + dev->cache.acalib.x;
	res->move.y = y - 0x200 + dev->cache.acalib.y;
	res->move.z = z - 0x200 + dev->cache.acalib.z;
}

static void wii__status_handler(struct wii_proto_dev *dev, const void *buf, struct wii_proto_res *res)
{
	const struct wii_proto_sr_status *pl = buf;

	wii__keys(dev, &pl->key, res);

	res->modified |= WII_PROTO_CR_BATTERY;
	res->battery.low = pl->flags & WII_PROTO_SR_STATUS_EMPTY;
	res->battery.level = pl->battery;

	wii_proto_do_format(dev);
}

static void wii__K_handler(struct wii_proto_dev *dev, const void *buf, struct wii_proto_res *res)
{
	const struct wii_proto_sr_K *pl = buf;

	wii__keys(dev, &pl->key, res);
}

static void wii__KA_handler(struct wii_proto_dev *dev, const void *buf, struct wii_proto_res *res)
{
	const struct wii_proto_sr_KA *pl = buf;

	wii__keys(dev, &pl->key, res);
	wii__accel(dev, &pl->key, pl->accel, res);
}

struct {
	unsigned int id;
	size_t size;
	void (*func)(struct wii_proto_dev *dev, const void *buf, struct wii_proto_res *res);
} wii__handlers[] = {
	{ WII_PROTO_SR_STATUS, 6, wii__status_handler },
	{ WII_PROTO_SR_K, 2, wii__K_handler },
	{ WII_PROTO_SR_KA, 5, wii__KA_handler },
	{ 0, 0, NULL },
};

void wii_proto_decode(struct wii_proto_dev *dev, const void *buf, size_t size, struct wii_proto_res *res)
{
	unsigned int i;
	const uint8_t *raw;

	res->modified = 0;
	res->error = WII_PROTO_E_NONE;
	raw = buf;

	if (!size) {
		res->error = WII_PROTO_E_EMPTY;
		return;
	}
	if (*raw != WII_PROTO_SH_CMD_IN) {
		res->error = WII_PROTO_E_BADHID;
		return;
	}
	if (size < 2) {
		res->error = WII_PROTO_E_BADREP;
		return;
	}

	for (i = 0; wii__handlers[i].id; ++i) {
		if (wii__handlers[i].id == raw[1]) {
			if (wii__handlers[i].size > (size - 2)) {
				res->error = WII_PROTO_E_BADARG;
				return;
			}
			if (wii__handlers[i].func)
				wii__handlers[i].func(dev, buf + 2, res);
			return;
		}
	}
	/* no handler for this report => ignore */
	return;
}
