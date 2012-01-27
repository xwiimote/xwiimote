/*
 * XWiimote - cwiid Compatibility
 * Written 2010, 2011, 2012 by David Herrmann
 * Dedicated to the Public Domain
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xwiimote.h"
#include "xwiimote_cwiid.h"

static cwiid_err_t *cwiid_err_func = cwiid_err_default;

/*
 * This is not thread safe. If the caller cares for thread-safety he must ensure
 * that this is not called synchronously from two threads. The error function
 * itself must be threadsafe, too.
 */
int cwiid_set_err(cwiid_err_t *err)
{
	if (!err)
		cwiid_err_func = cwiid_err_default;
	else
		cwiid_err_func = err;

	return 0;
}

void cwiid_err_default(struct wiimote *wiimote, const char *str, va_list ap)
{
	if (!str)
		str = "xwiimote-cwiid: empty error message";

	vfprintf(stderr, str, ap);
	fprintf(stderr, "\n");
}

cwiid_wiimote_t *cwiid_open(bdaddr_t *bdaddr, int flags)
{
	return NULL;
}

cwiid_wiimote_t *cwiid_open_timeout(bdaddr_t *bdaddr, int flags, int timeout)
{
	return NULL;
}

cwiid_wiimote_t *cwiid_listen(int flags)
{
	return NULL;
}

int cwiid_close(cwiid_wiimote_t *wiimote)
{
	return 0;
}

int cwiid_get_id(cwiid_wiimote_t *wiimote)
{
	return -1;
}

int cwiid_set_data(cwiid_wiimote_t *wiimote, const void *data)
{
	return 0;
}

const void *cwiid_get_data(cwiid_wiimote_t *wiimote)
{
	return NULL;
}

int cwiid_enable(cwiid_wiimote_t *wiimote, int flags)
{
	return 0;
}

int cwiid_disable(cwiid_wiimote_t *wiimote, int flags)
{
	return 0;
}

int cwiid_set_mesg_callback(cwiid_wiimote_t *wiimote,
					cwiid_mesg_callback_t *callback)
{
	return 0;
}

int cwiid_get_mesg(cwiid_wiimote_t *wiimote, int *mesg_count,
			union cwiid_mesg *mesg[], struct timespec *timestamp)
{
	return 0;
}

int cwiid_get_state(cwiid_wiimote_t *wiimote, struct cwiid_state *state)
{
	return 0;
}

int cwiid_get_acc_cal(struct wiimote *wiimote, enum cwiid_ext_type ext_type,
						struct acc_cal *acc_cal)
{
	return 0;
}

int cwiid_get_balance_cal(struct wiimote *wiimote,
					struct balance_cal *balance_cal)
{
	return 0;
}

int cwiid_command(cwiid_wiimote_t *wiimote, enum cwiid_command command,
								int flags)
{
	return 0;
}

int cwiid_send_rpt(cwiid_wiimote_t *wiimote, uint8_t flags, uint8_t report,
						size_t len, const void *data)
{
	return 0;
}

int cwiid_request_status(cwiid_wiimote_t *wiimote)
{
	return 0;
}

int cwiid_set_led(cwiid_wiimote_t *wiimote, uint8_t led)
{
	return 0;
}

int cwiid_set_rumble(cwiid_wiimote_t *wiimote, uint8_t rumble)
{
	return 0;
}

int cwiid_set_rpt_mode(cwiid_wiimote_t *wiimote, uint8_t rpt_mode)
{
	return 0;
}

int cwiid_read(cwiid_wiimote_t *wiimote, uint8_t flags, uint32_t offset,
						uint16_t len, void *data)
{
	return 0;
}

int cwiid_write(cwiid_wiimote_t *wiimote, uint8_t flags, uint32_t offset,
					uint16_t len, const void *data)
{
	return 0;
}

int cwiid_get_bdinfo_array(int dev_id, unsigned int timeout, int max_bdinfo,
				struct cwiid_bdinfo **bdinfo, uint8_t flags)
{
	return 0;
}

int cwiid_find_wiimote(bdaddr_t *bdaddr, int timeout)
{
	return 0;
}
