/*
 * XWiimote - tools
 * Written 2010, 2011 by David Herrmann
 * Dedicated to the Public Domain
 */

/*
 * XWiimote EEPROM Dump
 * This tool reads the whole eeprom of a wiimote and dumps the output to
 * stdout. This requires debugfs support and euid 0/root.
 * Pass as argument the eeprom input file. This requires that debugfs is
 * mounted and compiled into the kernel.
 *
 * Debugfs compiled:
 *   zgrep DEBUG_FS /proc/config.gz
 * Mount debugfs:
 *   mount -t debugfs debugfs /sys/kernel/debug
 * Path to eeprom file:
 *   /sys/kernel/debug/hid/<dev>/eeprom
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void show(const char *buf, size_t len)
{
	size_t i;

	for (i = 0; i < len; ++i)
		printf(" 0x%02hhx", buf[i]);
}

static void dump(int fd)
{
	char buf[1];
	int ret;
	size_t off, i;

	off = 0;
	while (1) {
		printf("0x%08zu:", off);

		for (i = 0; i < 8; ++i) {
			ret = read(fd, buf, sizeof(buf));
			if (ret > 0) {
				show(buf, ret);
			} else if (ret < 0) {
				printf(" (read error %d)", errno);
				if (lseek(fd, 1, SEEK_CUR) < 0) {
					printf(" (Seek failed %d)", errno);
					goto out;
				}
			} else {
				printf(" (eof)");
				goto out;
			}
			++off;
		}
		printf("\n");
	}

out:
	return;
}

static int open_eeprom(const char *file)
{
	int fd;

	fd = open(file, O_RDONLY);
	if (fd < 0)
		fprintf(stderr, "Cannot open eeprom file %d\n", errno);

	return fd;
}

int main(int argc, char **argv)
{
	int fd;

	if (argc < 2 || !*argv[1]) {
		fprintf(stderr, "Please give path to eeprom file as first argument\n");
		return EXIT_FAILURE;
	}

	fd = open_eeprom(argv[1]);
	if (fd >= 0) {
		dump(fd);
		close(fd);
	} else {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
