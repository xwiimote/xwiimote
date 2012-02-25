/*
 * XWiimote - lib
 * Written 2010, 2011, 2012 by David Herrmann
 * Dedicated to the Public Domain
 */

#ifndef XWII_INTERNAL_H
#define XWII_INTERNAL_H

#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>

/*
 * Sysfs attributes
 * Files in sysfs are called attributes. They often contain only a single value
 * for a single kernel parameter/event.
 */

extern int sfs_attr_read(const char *path, size_t *len, char **out);

/*
 * Generic directory handling
 * Helper functions for directory traversal and similar.
 */

typedef int (*sfs_dir_callback) (const char *path, const struct dirent *ent,
								void *extra);
extern int sfs_dir_foreach(const char *path, sfs_dir_callback callback,
								void *extra);

/*
 * Input devices
 * Each input device has a directory inside /sys/class/input. This helps reading
 * input device attributes to identify the device.
 * This also helps finding all input devices a parent has registered.
 */

struct sfs_input_dev {
	size_t ref;
	struct sfs_input_dev *next;
	char *path;
	char *event;
	char *name;
};

extern int sfs_input_read(const char *path, struct sfs_input_dev **dev);
extern struct sfs_input_dev *sfs_input_ref(struct sfs_input_dev *dev);
extern void sfs_input_unref(struct sfs_input_dev *dev);

typedef int (*sfs_input_callback) (struct sfs_input_dev *dev, void *extra);
extern int sfs_input_foreach(const char *path, sfs_input_callback callback,
								void *extra);
extern int sfs_input_list(const char *path, struct sfs_input_dev **first);

#endif /* XWII_INTERNAL_H */
