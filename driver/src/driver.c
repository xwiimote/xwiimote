/*
 * XWiimote - driver - driver.c
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

#include <stdlib.h>
#include <unistd.h>
#include "log.h"
#include "main.h"

void wii_start_driver(void *drv_arg)
{
	struct wii_drv_io *drv = drv_arg;

	close(drv->in);
	close(drv->out);
}
