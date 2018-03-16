/*
 * Copyright (C) 2015 Pengutronix, Sascha Hauer <entwicklung@pengutronix.de>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <inttypes.h>

#include "fileaccess.h"

#define DISP_LINE_LEN	16

/*
 * Like strtoull() but handles an optional G, M, K or k
 * suffix for Gibibyte, Mibibyte or Kibibyte.
 */
static unsigned long long strtoull_suffix(const char *str, char **endp, int base)
{
	unsigned long long val;
	char *end;

	val = strtoull(str, &end, base);

	switch (*end) {
	case 'G':
		val *= 1024;
	case 'M':
		val *= 1024;
	case 'k':
	case 'K':
		val *= 1024;
		end++;
	default:
		break;
	}

	if (endp)
		*endp = (char *)end;

	return val;
}

/*
 * This function parses strings in the form <startadr>[-endaddr]
 * or <startadr>[+size] and fills in start and size accordingly.
 * <startadr> and <endadr> can be given in decimal or hex (with 0x prefix)
 * and can have an optional G, M, K or k suffix.
 *
 * examples:
 * 0x1000-0x2000 -> start = 0x1000, size = 0x1001
 * 0x1000+0x1000 -> start = 0x1000, size = 0x1000
 * 0x1000        -> start = 0x1000, size = ~0
 * 1M+1k         -> start = 0x100000, size = 0x400
 */
static int parse_area_spec(const char *str, off_t *start, size_t *size)
{
	char *endp;
	off_t end;

	if (!isdigit(*str))
		return -1;

	*start = strtoull_suffix(str, &endp, 0);

	str = endp;

	if (!*str) {
		/* beginning given, but no size, assume maximum size */
		*size = ~0;
		return 0;
	}

	if (*str == '-') {
		/* beginning and end given */
		end = strtoull_suffix(str + 1, NULL, 0);
		if (end < *start) {
			fprintf(stderr, "end < start\n");
			return -1;
		}
		*size = end - *start + 1;
		return 0;
	}

	if (*str == '+') {
		/* beginning and size given */
		*size = strtoull_suffix(str + 1, NULL, 0);
		return 0;
	}

	return -1;
}
#define swab64(x) ((uint64_t)(						\
	(((uint64_t)(x) & (uint64_t)0x00000000000000ffUL) << 56) |	\
	(((uint64_t)(x) & (uint64_t)0x000000000000ff00UL) << 40) |	\
	(((uint64_t)(x) & (uint64_t)0x0000000000ff0000UL) << 24) |	\
	(((uint64_t)(x) & (uint64_t)0x00000000ff000000UL) <<  8) |	\
	(((uint64_t)(x) & (uint64_t)0x000000ff00000000UL) >>  8) |	\
	(((uint64_t)(x) & (uint64_t)0x0000ff0000000000UL) >> 24) |	\
	(((uint64_t)(x) & (uint64_t)0x00ff000000000000UL) >> 40) |	\
	(((uint64_t)(x) & (uint64_t)0xff00000000000000UL) >> 56)))

#define swab32(x) ((uint32_t)(						\
	(((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) |		\
	(((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) |		\
	(((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) |		\
	(((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24)))

#define swab16(x) ((uint16_t)(						\
	(((uint16_t)(x) & (uint16_t)0x00ffU) << 8) |			\
	(((uint16_t)(x) & (uint16_t)0xff00U) >> 8)))

static int memory_display(const void *addr, off_t offs,
			  size_t nbytes, int width, int swab)
{
	size_t linebytes, i;
	u_char	*cp;

	/* Print the lines.
	 *
	 * We buffer all read data, so we can make sure data is read only
	 * once, and all accesses are with the specified bus width.
	 */
	do {
		char linebuf[DISP_LINE_LEN];
		uint64_t *uqp = (uint64_t *)linebuf;
		uint32_t *uip = (uint32_t *)linebuf;
		uint16_t *usp = (uint16_t *)linebuf;
		uint8_t *ucp = (uint8_t *)linebuf;
		unsigned count = 52;

		printf("%08llx:", (unsigned long long)offs);
		linebytes = (nbytes > DISP_LINE_LEN) ? DISP_LINE_LEN : nbytes;

		for (i = 0; i < linebytes; i += width) {
			if (width == 8) {
				uint64_t res;
				res = (*uqp++ = *((uint64_t *)addr));
				if (swab)
					res = swab64(res);
				count -= printf(" %016" PRIx64, res);
			} else if (width == 4) {
				uint32_t res;
				res = (*uip++ = *((uint *)addr));
				if (swab)
					res = swab32(res);
				count -= printf(" %08" PRIx32, res);
			} else if (width == 2) {
				uint16_t res;
				res = (*usp++ = *((ushort *)addr));
				if (swab)
					res = swab16(res);
				count -= printf(" %04" PRIx16, res);
			} else {
				count -= printf(" %02x", (*ucp++ = *((u_char *)addr)));
			}
			addr += width;
			offs += width;
		}

		while (count--)
			putchar(' ');

		cp = (uint8_t *)linebuf;
		for (i = 0; i < linebytes; i++) {
			if ((*cp < 0x20) || (*cp > 0x7e))
				putchar('.');
			else
				printf("%c", *cp);
			cp++;
		}

		putchar('\n');
		nbytes -= linebytes;
	} while (nbytes > 0);

	return 0;
}

static void usage_md(void)
{
	printf(
"md - memory display\n"
"\n"
"Usage: md [-bwlqsx] REGION\n"
"\n"
"Display (hex dump) a memory region.\n"
"\n"
"Options:\n"
"  -b        byte access\n"
"  -w        word access (16 bit)\n"
"  -l        long access (32 bit)\n"
"  -q        quad access (64 bit)\n"
"  -s <FILE> display file (default /dev/mem)\n"
"  -x        swap bytes at output\n"
"\n"
"Memory regions can be specified in two different forms: START+SIZE\n"
"or START-END, If START is omitted it defaults to 0x100\n"
"Sizes can be specified as decimal, or if prefixed with 0x as hexadecimal.\n"
"An optional suffix of k, M or G is for kbytes, Megabytes or Gigabytes.\n"
	);

}

static int cmd_memory_display(int argc, char **argv)
{
	int opt;
	int width = 4;
	size_t bufsize, size = 0x100;
	char *buf;
	void *handle;
	off_t start = 0x0;
	char *file = "/dev/mem";
	int swap = 0;

	while ((opt = getopt(argc, argv, "bwlqs:xh")) != -1) {
		switch (opt) {
		case 'b':
			width = 1;
			break;
		case 'w':
			width = 2;
			break;
		case 'l':
			width = 4;
			break;
		case 'q':
			width = 8;
			break;
		case 's':
			file = optarg;
			break;
		case 'x':
			swap = 1;
			break;
		case 'h':
			usage_md();
			return 0;
		}
	}

	if (optind < argc) {
		if (parse_area_spec(argv[optind], &start, &size)) {
			fprintf(stderr, "could not parse: %s\n", argv[optind]);
			return EXIT_FAILURE;
		}
		if (size == ~0)
			size = 0x100;
	}

	if (size & (width - 1)) {
		size &= ~(width - 1);
		fprintf(stderr, "warning: skipping truncated read, size=%zu\n",
			size);
	}

	if (!size)
		return EXIT_SUCCESS;

	bufsize = size;
	if (bufsize > 4096)
		bufsize = 4096;

	buf = malloc(bufsize);
	if (!buf) {
		fprintf(stderr, "could not allocate memory\n");
		return EXIT_FAILURE;
	}

	handle = memtool_open(file, O_RDONLY);
	if (!handle)
		return EXIT_FAILURE;

	while (size) {
		int ret;

		if (size < bufsize)
			bufsize = size;

		ret = memtool_read(handle, start, buf, bufsize, width);
		if (ret < 0)
			return EXIT_FAILURE;

		assert(ret == bufsize);
		memory_display(buf, start, bufsize, width, swap);

		start += bufsize;
		size -= bufsize;
	}

	memtool_close(handle);

	return EXIT_SUCCESS;
}

static void usage_mw(void)
{
	printf(
"mw - memory write\n"
"\n"
"Usage: mw [-bwlqd] OFFSET DATA...\n"
"\n"
"Write DATA value(s) to the specified REGION.\n"
"\n"
"Options:\n"
"  -b        byte access\n"
"  -w        word access (16 bit)\n"
"  -l        long access (32 bit)\n"
"  -q        quad access (64 bit)\n"
"  -d <FILE> write file (default /dev/mem)\n"
	);
}

static int cmd_memory_write(int argc, char *argv[])
{
	off_t adr;
	size_t bufsize, size;
	char *buf;
	void *handle;
	int width = 4;
	int opt;
	int i, ret;
	char *file = "/dev/mem";

	while ((opt = getopt(argc, argv, "bwlqd:h")) != -1) {
		switch (opt) {
		case 'b':
			width = 1;
			break;
		case 'w':
			width = 2;
			break;
		case 'l':
			width = 4;
			break;
		case 'q':
			width = 8;
			break;
		case 'd':
			file = optarg;
			break;
		case 'h':
			usage_mw();
			return 0;
		}
	}

	if (optind + 1 >= argc) {
		fprintf(stderr, "Too few parameters for mw\n");
		return EXIT_FAILURE;
	}

	adr = strtoull_suffix(argv[optind++], NULL, 0);

	size = (argc - optind) * width;
	if (!size)
		return EXIT_SUCCESS;

	bufsize = size;
	if (bufsize > 4096)
		bufsize = 4096;

	buf = malloc(bufsize);
	if (!buf) {
		fprintf(stderr, "could not allocate memory\n");
		return EXIT_FAILURE;
	}

	handle = memtool_open(file, O_RDWR | O_CREAT);
	if (!handle)
		return EXIT_FAILURE;

	while (optind < argc) {
		i = 0;

		while (optind < argc && i * width < bufsize) {
			switch (width) {
			case 1:
				((uint8_t *)buf)[i] =
					strtoull(argv[optind], NULL, 0);
				break;
			case 2:
				((uint16_t *)buf)[i] =
					strtoull(argv[optind], NULL, 0);
				break;
			case 4:
				((uint32_t *)buf)[i] =
					strtoull(argv[optind], NULL, 0);
				break;
			case 8:
				((uint64_t *)buf)[i] =
					strtoull(argv[optind], NULL, 0);
				break;
			}
			++i;
			++optind;
		}

		ret = memtool_write(handle, adr, buf, i * width, width);
		if (ret < 0)
			break;

		assert(ret == i * width);
		adr += i * width;
	}


	memtool_close(handle);
	free(buf);

	return ret < 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

struct cmd {
	int (*cmd)(int argc, char **argv);
	const char *name;
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static struct cmd cmds[] = {
	{
		.cmd = cmd_memory_display,
		.name = "md",
	}, {
		.cmd = cmd_memory_write,
		.name = "mw",
	},
};

static void usage(void)
{
	printf(
"memtool - display and modify memory\n"
"\n"
"Usage: memtool <cmd> [OPTIONS]\n"
"\n"
"memtool is divided into subcommands. Supported commands are:\n"
"md: memory display, Show regions of memory\n"
"mw: memory write, write values to memory\n"
"\n"
"To show help for a subcommand do 'memtool <cmd> -h'\n"
"\n"
"memtool is a collection of tools to show (hexdump) and modify arbitrary files.\n"
"By default /dev/mem is used to allow access to physical memory.\n"
	);
}

int main(int argc, char **argv)
{
	int i;
	struct cmd *cmd;

	if (!strcmp(basename(argv[0]), "memtool")) {
		argv++;
		argc--;

		if (argc > 0 && !strcmp(argv[0], "-V")) {
			printf("%s\n", PACKAGE_STRING);
			return EXIT_SUCCESS;
		}
	}

	if (argc < 1) {
		fprintf(stderr, "No command given\n");
		usage();
		return EXIT_FAILURE;
	}

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		cmd = &cmds[i];
		if (!strcmp(argv[0], cmd->name))
			return cmd->cmd(argc, argv);
	}

	fprintf(stderr, "No such command: %s\n", argv[0]);

	usage();

	return EXIT_FAILURE;
}
