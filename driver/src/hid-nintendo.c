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

static int nintendo_probe(struct hid_device *hdev, const struct hid_device_id *id)
{
	int ret;

	printk("Probing Nintendo HID driver\n");
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

static void nintendo_remove(struct hid_device *hdev)
{
	hid_hw_stop(hdev);
}

static const struct hid_device_id nintendo_devices[] = {
	{ HID_BLUETOOTH_DEVICE(0x057e, 0x0306) },
	{ }
};
MODULE_DEVICE_TABLE(hid, nintendo_devices);

static struct hid_driver nintendo_driver = {
	.name = "nintendo",
	.id_table = nintendo_devices,
	.probe = nintendo_probe,
	.remove = nintendo_remove,
};

static int __init nintendo_init(void)
{
	printk("Registering Nintendo HID Driver\n");
	return hid_register_driver(&nintendo_driver);
}

static void __exit nintendo_exit(void)
{
	hid_unregister_driver(&nintendo_driver);
}

module_init(nintendo_init);
module_exit(nintendo_exit);
MODULE_LICENSE("GPL");
