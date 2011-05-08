/*
 *  HID driver for Nintendo Wiimote devices
 *
 *  Copyright (c) 2011 David Herrmann
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/hid.h>
#include <linux/module.h>

static int wiimote_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	printk("Probing Wiimote HID driver\n");
	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		return ret;
	}

	ret = hid_hw_start(hdev, HID_CONNECT_DEFAULT);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		return ret;
	}

	return 0;
}

static void wiimote_remove(struct hid_device *hdev)
{
	printk("Removing Wiimote HID driver\n");
	hid_hw_stop(hdev);
}

static const struct hid_device_id wiimote_devices[] = {
	{ HID_BLUETOOTH_DEVICE(0x057e, 0x0306) },
	{ }
};
MODULE_DEVICE_TABLE(hid, wiimote_devices);

static struct hid_driver wiimote_driver = {
	.name = "wiimote",
	.id_table = wiimote_devices,
	.probe = wiimote_probe,
	.remove = wiimote_remove,
};

static int __init wiimote_init(void)
{
	return hid_register_driver(&wiimote_driver);
}

static void __exit wiimote_exit(void)
{
	hid_unregister_driver(&wiimote_driver);
}

module_init(wiimote_init);
module_exit(wiimote_exit);
MODULE_LICENSE("GPL");
