/*
 * Sysfs Helper Library
 * Written 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "internal.h"

/*
 * Concatenate two strings
 * This allocates a new buffer and writes \path and \cat into it. \len is the
 * length of \path or -1 to use strlen(path). \clen is the length of \cat or -1
 * to use strlen(cat).
 * The resulting size is written to \outlen if it is non-NULL. The resulting
 * string pointer is stored in \out which must be non-NULL.
 * On failure, a negative error code is returned and no output variable is
 * touched. On success 0 is returned.
 */
static int path_cat(ssize_t len, const char *path, ssize_t clen,
				const char *cat, size_t *outlen, char **out)
{
	size_t size;
	char *res;

	assert(path);
	assert(cat);
	assert(out);

	if (len < 0)
		len = strlen(path);
	if (clen < 0)
		clen = strlen(cat);

	size = len + clen;
	res = malloc(size + 1);
	if (!res)
		return -ENOMEM;

	memcpy(res, path, len);
	memcpy(&res[len], cat, clen);
	res[size] = 0;

	if (outlen)
		*outlen = size;
	*out = res;

	return 0;
}

/*
 * Read attribute
 * This reads the whole attribute located at \path into a freshly allocated
 * buffer and stores a pointer to the buffer into \out.
 * On failure, a negative error code is returned and \out is not touched. On
 * success 0 is returned and the pointer stored into \out and the length in
 * \len. If \len is NULL, the length is not stored.
 * You need to free the pointer on success if you don't need it anymore with
 * free().
 * If the attribute is empty -ENODATA is returned.
 * A trailing newline character in \out is removed. This always adds a
 * terminating zero character to the result so it is safe to be printed with
 * printf() etc.
 */
int sfs_attr_read(const char *path, size_t *len, char **out)
{
	FILE *f;
	int ret = 0;
	long size;
	char *tmp;

	assert(path);
	assert(out);

	f = fopen(path, "rb");
	if (!f)
		return -errno;

	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (size < 0) {
		ret = -EINVAL;
		goto err_file;
	}

	if (size == 0) {
		ret = -ENODATA;
		goto err_file;
	}

	tmp = malloc(size + 1);
	if (!tmp) {
		ret = -ENOMEM;
		goto err_file;
	}

	ret = fread(tmp, 1, size, f);
	if (ret < 1) {
		ret = -EINVAL;
		free(tmp);
		goto err_file;
	}

	if (tmp[ret - 1] == '\n')
		--ret;

	if (ret <= 0) {
		free(tmp);
		ret = -ENODATA;
		goto err_file;
	}

	tmp[ret] = 0;
	if (len)
		*len = ret;
	*out = tmp;
	ret = 0;

err_file:
	fclose(f);
	return ret;
}

/*
 * readdir_r() requires us to allocate the "struct dirent" manually. However, we
 * cannot allocate it on the stack as we don't know the size of the structure.
 * See manpage of readdir_r(3) for information on this. This is simply copied
 * from there.
 */
static int alloc_dirent(const char *path, struct dirent **ent)
{
	size_t len;
	struct dirent *tmp;

	len = offsetof(struct dirent, d_name) +
					pathconf(path, _PC_NAME_MAX) + 1;
	tmp = malloc(len);
	if (!tmp)
		return -ENOMEM;

	*ent = tmp;
	return 0;
}

/*
 * Go through directory
 * This calls \callback for every entry in the directory \path. The \extra
 * argument is passed untouched to each callback.
 * If a callback returns non-zero, then the function returns the error code
 * unchanged. If the directory stream fails or memory allocation fails, then a
 * negative error code is returned.
 * Returns 0 if everything went fine.
 */
int sfs_dir_foreach(const char *path, sfs_dir_callback callback, void *extra)
{
	int ret = 0;
	DIR *dir;
	struct dirent *ent, *res;

	dir = opendir(path);
	if (!dir)
		return -errno;

	ret = alloc_dirent(path, &ent);
	if (ret)
		goto err_dir;

	while (true) {
		ret = readdir_r(dir, ent, &res);
		if (ret) {
			ret = -abs(ret);
			goto err_ent;
		}
		if (!res)
			break;
		ret = callback(path, ent, extra);
		if (ret)
			goto err_ent;
	}

err_ent:
	free(ent);
err_dir:
	closedir(dir);
	return ret;
}

/* foreach through /sys/<device>/input/inputX and find eventY */
static int event_foreach(const char *path, const struct dirent *ent,
								void *extra)
{
	struct sfs_input_dev *dev = extra;
	char *npath, *tmp;
	int ret;

	if (!strncmp("event", ent->d_name, 5)) {
		if (dev->event)
			return 0;

		dev->event = strdup(ent->d_name);
		if (!dev->event)
			return -ENOMEM;
	} else if (!strcmp("name", ent->d_name)) {
		if (dev->name)
			return 0;

		ret = path_cat(-1, path, -1, "/", NULL, &tmp);
		if (ret)
			return ret;

		ret = path_cat(-1, tmp, -1, ent->d_name, NULL, &npath);
		free(tmp);
		if (ret)
			return ret;

		ret = sfs_attr_read(npath, NULL, &tmp);
		free(npath);
		if (ret)
			return ret;

		dev->name = tmp;
	}

	return 0;
}

/*
 * Read information about input device
 * \path must point to an input device from /sys/class/input/<device>
 * The path \path is copied into a new buffer in \dev->path. The name of the
 * device is read into \dev->name and the event file name is read into
 * \dev->event. On failure, \dev is not touched.
 * On success, it returns 0 and you must free \dev with sfs_input_destroy().
 */
int sfs_input_read(const char *path, struct sfs_input_dev **dev)
{
	int ret = 0;
	struct sfs_input_dev *d;

	assert(path);
	assert(dev);

	d = malloc(sizeof(*d));
	if (!d)
		return -ENOMEM;

	memset(d, 0, sizeof(*d));
	sfs_input_ref(d);

	d->path = strdup(path);
	if (!d->path) {
		ret = -ENOMEM;
		goto err_dev;
	}

	ret = sfs_dir_foreach(path, event_foreach, d);
	if (ret)
		goto err_dev;

	*dev = d;
	return 0;

err_dev:
	sfs_input_unref(d);
	return ret;
}

struct sfs_input_dev *sfs_input_ref(struct sfs_input_dev *dev)
{
	dev->ref++;
	assert(dev->ref);
	return dev;
}

/* frees the result of sfs_input_read() */
void sfs_input_unref(struct sfs_input_dev *dev)
{
	if (!dev)
		return;

	assert(dev->ref);

	if (--dev->ref)
		return;

	sfs_input_unref(dev->next);
	free(dev->path);
	free(dev->event);
	free(dev->name);
	free(dev);
}

struct input_foreach_extra {
	void *extra;
	sfs_input_callback callback;
};

/* foreach through /sys/<device>/input */
static int input_foreach(const char *path, const struct dirent *ent,
								void *extra)
{
	struct input_foreach_extra *e = extra;
	int ret = 0;
	char *npath;
	size_t len;
	struct sfs_input_dev *dev;

	if (ent->d_type != DT_DIR && ent->d_type != DT_LNK)
		return 0;

	if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
		return 0;

	len = strlen(ent->d_name);
	if (len <= 0)
		return 0;

	ret = path_cat(-1, path, len, ent->d_name, NULL, &npath);
	if (ret)
		return ret;

	ret = sfs_input_read(npath, &dev);
	free(npath);
	if (ret)
		return ret;

	ret = e->callback(dev, e->extra);
	sfs_input_unref(dev);

	return ret;
}

/*
 * Foreach over all input devices of a single device
 * Many devices register input devices to report events to userspace. Input
 * devices are accessible via /dev/input/eventX and similar. However, you need
 * to find the correct event device first.
 * This function takes as \path argument the path to the base directory of a
 * device that has registered input devices. If this device has registered input
 * devices, then there is an "./input/" subdirectory with several
 * "./input/input12/" subdirectories and similar.
 * This iterates over each "./input/inputX" input device and calls the callback
 * with each device. It also opens "./input/inputX/name" and reads
 * "./input/inputX/eventX" and passes both information to the callback so you
 * can immediately check the device name and event file.
 * Returns 0 on success and negative error code on failure.
 */
int sfs_input_foreach(const char *path, sfs_input_callback callback,
								void *extra)
{
	char *npath;
	int ret;
	struct input_foreach_extra e;

	assert(path);
	assert(callback);

	ret = path_cat(-1, path, -1, "/input/", NULL, &npath);
	if (ret)
		return ret;

	e.extra = extra;
	e.callback = callback;
	ret = sfs_dir_foreach(npath, input_foreach, &e);
	free(npath);

	return ret;
}

/* stuff all input devices into a single linked list */
static int input_list_foreach(struct sfs_input_dev *dev, void *extra)
{
	struct sfs_input_dev **first = extra;

	assert(!dev->next);

	dev = sfs_input_ref(dev);
	dev->next = *first;
	*first = dev;

	return 0;
}

/*
 * Same as sfs_input_foreach but this creates a single linked list of all input
 * devices instead of calling a callback on each device.
 * Calling sfs_input_destroy() on the result frees the whole list recursively.
 */
int sfs_input_list(const char *path, struct sfs_input_dev **out)
{
	struct sfs_input_dev *first = NULL;
	int ret;

	ret = sfs_input_foreach(path, input_list_foreach, &first);
	if (ret)
		return ret;

	*out = first;
	return 0;
}
