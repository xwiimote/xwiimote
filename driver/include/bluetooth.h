/*
 * XWiimote - driver - bluetooth.h
 * Written by David Herrmann, 2010, 2011
 * Dedicated to the Public Domain
 */

/*
 * Bluetooth Interface
 * Provides access to the low-level bluetooth functions. This performs
 * the device inquiry, sets up l2cap listeners or connects to remote
 * devices.
 *
 * Namespace: WII_BT_*
 *            wii_bt_*
 *
 * Device Inquiry:
 *   wii_bt_inquiry() performs a bluetooth device inquiry. It supports
 *   an UI interface which lets the user choose one of the found devices.
 *   This device is returned or NULL if no device was chosen. The returned
 *   string is a pointer to a static variable inside this module so this
 *   address becomes overridden when this function is called again.
 *   This function is not reentrant.
 */

#ifndef WII_BT_H
#define WII_BT_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#define WII_BT_LONG 0x01 /* perform 30s inquiry instead of 10s */
#define WII_BT_UI 0x02 /* provide UI to select one of the results */
#define WII_BT_CACHE 0x04 /* use the kernel cache of inquiry results */
#define WII_BT_NAMES 0x08 /* resolve names */

extern const char *wii_bt_inquiry(unsigned int flags);

#endif /* WII_BT_H */
