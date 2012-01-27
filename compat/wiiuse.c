/*
 * XWiimote - wiiuse Compatibility
 * Written 2010, 2011, 2012 by David Herrmann
 * Dedicated to the Public Domain
 */

#include <stdlib.h>
#include <string.h>
#include "xwiimote.h"
#include "xwiimote_wiiuse.h"

const char *wiiuse_version()
{
	/* this is the most recent version of wiiuse */
	return "0.12";
}

struct wiimote_t **wiiuse_init(int wiimotes)
{
	struct wiimote_t **wm;
	int i;

	if (!wiimotes)
		return NULL;

	wm = malloc(sizeof(struct wiimote_t*) * wiimotes);
	if (!wm)
		return NULL;

	for (i = 0; i < wiimotes; ++i) {
		wm[i] = malloc(sizeof(*wm[i]));
		if (!wm[i])
			goto err_loop;

		memset(wm[i], 0, sizeof(*wm[i]));
		wm[i]->unid = i + 1;
		wm[i]->bdaddr = *BDADDR_ANY;
		wm[i]->out_sock = -1;
		wm[i]->in_sock = -1;

		wm[i]->event = WIIUSE_NONE;
		wm[i]->exp.type = EXP_NONE;
	}

	return wm;

err_loop:
	while (i > 0) {
		--i;
		free(wm[i]);
	}

	free(wm);
	return NULL;
}

void wiiuse_disconnected(struct wiimote_t *wm)
{
	if (!wm)
		return;
}

void wiiuse_cleanup(struct wiimote_t **wm, int wiimotes)
{
	if (!wm)
		return;

	while (wiimotes > 0) {
		--wiimotes;
		free(wm[wiimotes]);
	}

	free(wm);
}

void wiiuse_rumble(struct wiimote_t *wm, int status)
{
	if (!wm)
		return;
}

void wiiuse_toggle_rumble(struct wiimote_t *wm)
{
	if (!wm)
		return;
}

void wiiuse_set_leds(struct wiimote_t *wm, int leds)
{
	if (!wm)
		return;
}

void wiiuse_motion_sensing(struct wiimote_t *wm, int status)
{
	if (!wm)
		return;
}

/* return 0 on failure and 1 on success */
int wiiuse_read_data(struct wiimote_t *wm, byte *buffer, unsigned int offset,
							unsigned short len)
{
	if (!wm || !buffer || !len)
		return 0;

	return 0;
}

/* return 0 on failure and 1 on success */
int wiiuse_write_data(struct wiimote_t *wm, unsigned int addr, byte *data,
								byte len)
{
	if (!wm || !data || !len)
		return 0;

	return 0;
}

void wiiuse_status(struct wiimote_t *wm)
{
	if (!wm)
		return;
}

struct wiimote_t *wiiuse_get_by_id(struct wiimote_t **wm, int wiimotes,
								int unid)
{
	int i = 0;

	if (!wm)
		return NULL;

	for (i = 0; i < wiimotes; ++i) {
		if (wm[i]->unid == unid)
			return wm[i];
	}

	return NULL;
}

int wiiuse_set_flags(struct wiimote_t *wm, int enable, int disable)
{
	if (!wm)
		return 0;

	return 0;
}

float wiiuse_set_smooth_alpha(struct wiimote_t *wm, float alpha)
{
	if (!wm)
		return 0;

	return 0;
}

void wiiuse_set_bluetooth_stack(struct wiimote_t **wm, int wiimotes,
						enum win_bt_stack_t type)
{
	/* this is not available under linux */
}

void wiiuse_set_orient_threshold(struct wiimote_t *wm, float threshold)
{
	if (!wm)
		return;
}

void wiiuse_resync(struct wiimote_t *wm)
{
	if (!wm)
		return;
}

void wiiuse_set_timeout(struct wiimote_t **wm, int wiimotes,
					byte normal_timeout, byte exp_timeout)
{
	/* this is not available under linux */
}

void wiiuse_set_accel_threshold(struct wiimote_t *wm, int threshold)
{
	if (!wm)
		return;
}

int wiiuse_find(struct wiimote_t **wm, int max_wiimotes, int timeout)
{
	if (!wm)
		return 0;

	return 0;
}

int wiiuse_connect(struct wiimote_t **wm, int wiimotes)
{
	if (!wm || !wiimotes)
		return 0;

	return 0;
}

void wiiuse_disconnect(struct wiimote_t *wm)
{
	if (!wm)
		return;
}

int wiiuse_poll(struct wiimote_t **wm, int wiimotes)
{
	if (!wm || !wiimotes)
		return 0;

	return 0;
}

void wiiuse_set_ir(struct wiimote_t *wm, int status)
{
	if (!wm)
		return;
}

void wiiuse_set_ir_vres(struct wiimote_t *wm, unsigned int x, unsigned int y)
{
	if (!wm)
		return;
}

void wiiuse_set_ir_position(struct wiimote_t *wm, enum ir_position_t pos)
{
	if (!wm)
		return;
}

void wiiuse_set_aspect_ratio(struct wiimote_t *wm, enum aspect_t aspect)
{
	if (!wm)
		return;
}

void wiiuse_set_ir_sensitivity(struct wiimote_t *wm, int level)
{
	if (!wm)
		return;
}

void wiiuse_set_nunchuk_orient_threshold(struct wiimote_t *wm, float threshold)
{
	if (!wm)
		return;
}

void wiiuse_set_nunchuk_accel_threshold(struct wiimote_t *wm, int threshold)
{
	if (!wm)
		return;
}
