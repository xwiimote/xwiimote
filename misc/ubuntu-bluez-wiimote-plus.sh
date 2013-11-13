#!/bin/bash

#set -x

echo "Please make sure to have run
$ sudo apt-get build-dep bluez
before attempting to run this script!
(Hit enter to continue)"
read

set -euf

TMPDIR=$(mktemp -d)
pushd $TMPDIR >/dev/null

echo '***** Getting original sources *****'
apt-get source bluez
cd bluez-4.101

echo '***** Patching *****'
echo 'Backported (altered) patch for Wiimote-Plus support

Cherry-Pickend and Backported patch for plugins/wiimote.c from bluez-git.
- Changes detection approach.
- Adds support for Wiimote-Plus id and names.

From e9fc6bf4a3ffad3a40096836aff7858a57ce0288 Mon Sep 17 00:00:00 2001
From: David Herrmann <dh.herrmann@googlemail.com>
Date: Mon, 22 Oct 2012 08:31:20 +0000
Subject: wiimote: add Wii-Remote-Plus ID and name detection

The Nintendo Wii Remote Plus uses a new product ID and name. To detect
them properly, we need to add them to the wiimote-module.

To avoid an overlong "if" statement, this converts the match-function to
walk over an array and check all VID/PID pairs and device-names. This
makes adding new devices much easier.

diff --git a/plugins/wiimote.c b/plugins/wiimote.c
--- a/plugins/wiimote.c	2013-11-06 12:28:57.904858899 +0100
+++ b/plugins/wiimote.c	2013-11-06 12:29:02.136858801 +0100
@@ -2,7 +2,7 @@
  *
  *  BlueZ - Bluetooth protocol stack for Linux
  *
- *  Copyright (C) 2011  David Herrmann <dh.herrmann@googlemail.com>
+ *  Copyright (C) 2011-2012 David Herrmann <dh.herrmann@googlemail.com>
  *
  *
  *  This program is free software; you can redistribute it and/or modify
@@ -56,12 +56,24 @@
  * is pressed.
  */
 
+static uint16_t wii_ids[][2] = {
+	{ 0x057e, 0x0306 },
+	{ 0x057e, 0x0330 },
+};
+
+static const char *wii_names[] = {
+	"Nintendo RVL-CNT-01",
+	"Nintendo RVL-CNT-01-TR",
+	"Nintendo RVL-WBC-01",
+};
+
 static ssize_t wii_pincb(struct btd_adapter *adapter, struct btd_device *device,
 						char *pinbuf, gboolean *display)
 {
 	uint16_t vendor, product;
 	bdaddr_t sba, dba;
 	char addr[18], name[25];
+	unsigned int i;
 
 	adapter_get_address(adapter, &sba);
 	device_get_address(device, &dba, NULL);
@@ -73,14 +85,22 @@
 	device_get_name(device, name, sizeof(name));
 	name[sizeof(name) - 1] = 0;
 
-	if (g_str_equal(name, "Nintendo RVL-CNT-01") ||
-				(vendor == 0x057e && product == 0x0306)) {
-		DBG("Forcing fixed pin on detected wiimote %s", addr);
-		memcpy(pinbuf, &sba, 6);
-		return 6;
+	for (i = 0; i < G_N_ELEMENTS(wii_ids); ++i) {
+		if (vendor == wii_ids[i][0] && product == wii_ids[i][1])
+			goto found;
+	}
+
+	for (i = 0; i < G_N_ELEMENTS(wii_names); ++i) {
+		if (g_str_equal(name, wii_names[i]))
+			goto found;
 	}
 
 	return 0;
+
+found:
+	DBG("Forcing fixed pin on detected wiimote %s", addr);
+	memcpy(pinbuf, &sba, 6);
+	return 6;
 }
 
 static int wii_probe(struct btd_adapter *adapter)' >> debian/patches/git-e9fc6bf-wiimote-plus-backported.patch
 
echo 'git-e9fc6bf-wiimote-plus-backported.patch' >> debian/patches/series

debchange --nmu "Backport support for wiimote plus"
#debchange --check-dirname-level=0 --local=+wiimote-plus "Backport support for wiimote plus"

echo '***** Building (this may take some time) *****'
debuild -uc -us

#sudo dpkg -i ../bluez_4.101-0ubuntu?.?_*.deb

cd ..

set +f
FILENAME=$(echo bluez_4.101-?ubuntu*.1_*.deb)
set -f

popd >/dev/null

mv "$TMPDIR/$FILENAME" ./

rm -rf $TMPDIR

echo "You can now install the patched bluez package via
$ sudo dpkg -i $FILENAME
Good Luck and have fun."
