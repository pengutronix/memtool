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

#include <libgen.h>
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

#define DISP_LINE_LEN	16

/*
 * Like simple_strtoull() but handles an optional G, M, K or k
 * suffix for Gigabyte, Megabyte or Kilobyte
 */
unsigned long long strtoull_suffix(const char *str, char **endp, int base)
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
int parse_area_spec(const char *str, unsigned long long *start, unsigned long long *size)
{
	char *endp;
	loff_t end;

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
			printf("end < start\n");
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
#define swab32(x) ((uint32_t)(						\
	(((uint32_t)(x) & (uint32_t)0x000000ffUL) << 24) |		\
	(((uint32_t)(x) & (uint32_t)0x0000ff00UL) <<  8) |		\
	(((uint32_t)(x) & (uint32_t)0x00ff0000UL) >>  8) |		\
	(((uint32_t)(x) & (uint32_t)0xff000000UL) >> 24)))

#define swab16(x) ((uint16_t)(						\
	(((uint16_t)(x) & (uint16_t)0x00ffU) << 8) |			\
	(((uint16_t)(x) & (uint16_t)0xff00U) >> 8)))

int memory_display(const void *addr, unsigned long long offs, unsigned nbytes, int size, int swab)
{
	ulong linebytes, i;
	u_char	*cp;

	/* Print the lines.
	 *
	 * We buffer all read data, so we can make sure data is read only
	 * once, and all accesses are with the specified bus width.
	 */
	do {
		char linebuf[DISP_LINE_LEN];
		uint32_t *uip = (uint   *)linebuf;
		uint16_t *usp = (ushort *)linebuf;
		uint8_t *ucp = (u_char *)linebuf;
		unsigned count = 52;

		printf("%08llx:", offs);
		linebytes = (nbytes > DISP_LINE_LEN) ? DISP_LINE_LEN : nbytes;

		for (i = 0; i < linebytes; i += size) {
			if (size == 4) {
				uint32_t res;
				res = (*uip++ = *((uint *)addr));
				if (swab)
					res = swab32(res);
				count -= printf(" %08x", res);
			} else if (size == 2) {
				uint16_t res;
				res = (*usp++ = *((ushort *)addr));
				if (swab)
					res = swab16(res);
				count -= printf(" %04x", res);
			} else {
				count -= printf(" %02x", (*ucp++ = *((u_char *)addr)));
			}
			addr += size;
			offs += size;
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
int memfd;

static void *memmap(const char *file, unsigned long addr, unsigned long size)
{
	unsigned long mmap_start, ofs;
	void *mem;

	memfd = open(file, O_RDWR);
	if (memfd < 0) {
		perror("open");
		exit(1);
	}

	mmap_start = addr & ~(4095);
	ofs = addr - mmap_start;

	mem = mmap(0, size + ofs, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, mmap_start);
	if (mem == MAP_FAILED) {
		perror("mmap");
		goto out;
	}

	return mem + ofs;
out:
	close(memfd);

	return NULL;
}

static int md(int argc, char **argv)
{
	int opt;
	int width = 4;
	unsigned long long size = 0x100, start = 0x0;
	void *mem;
	char *file = "/dev/mem";
	int swap = 0;

	while ((opt = getopt(argc, argv, "bwls:x")) != -1) {
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
		case 's':
			file = optarg;
			break;
		case 'x':
			swap = 1;
			break;
		}
	}

	if (optind < argc) {
		if (parse_area_spec(argv[optind], &start, &size)) {
			printf("could not parse: %s\n", argv[optind]);
			return 1;
		}
		if (size == ~0)
			size = 0x100;
	}

	mem = memmap(file, start, size);

	memory_display(mem, start, size, width, swap);

	close(memfd);

	exit(1);
}

static int mm(int argc, char *argv[])
{
	unsigned long long adr;
	int width = 4;
	int opt;
	void *mem;
	char *file = "/dev/mem";

	while ((opt = getopt(argc, argv, "bwld:")) != -1) {
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
		case 'd':
			file = optarg;
			break;
		}
	}

	if (optind + 1 >= argc)
		return 1;

	adr = strtoull_suffix(argv[optind++], NULL, 0);

	mem = memmap(file, adr, argc * sizeof(unsigned long));
	if (!mem)
		return 1;

	while (optind < argc) {
		uint8_t val8;
		uint16_t val16;
		uint32_t val32;

		switch (width) {
		case 1:
			val8 = strtoul(argv[optind], NULL, 0);
			*(volatile uint8_t *)mem = val8;
			mem += 1;
			break;
		case 2:
			val16 = strtoul(argv[optind], NULL, 0);
			*(volatile uint16_t *)mem = val16;
			mem += 2;
			break;
		case 4:
			val32 = strtoul(argv[optind], NULL, 0);
			*(volatile uint32_t *)mem = val32;
			mem += 4;
			break;
		}
		optind++;
	}

	close(memfd);

	return 0;
}

struct cmd {
	int (*cmd)(int argc, char **argv);
	const char *name;
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

struct cmd cmds[] = {
	{
		.cmd = md,
		.name = "md",
	}, {
		.cmd = md,
		.name = "d",
	}, {
		.cmd = mm,
		.name = "mm",
	}, {
		.cmd = mm,
		.name = "mw",
	},

};

int main(int argc, char **argv)
{
	int i;
	struct cmd *cmd;

	if (!strcmp(basename(argv[0]), "memtool")) {
		argv++;
		argc--;
	}

	if (argc < 1) {
		fprintf(stderr, "No command given\n");
		return EXIT_FAILURE;
	}

	for (i = 0; i < ARRAY_SIZE(cmds); i++) {
		cmd = &cmds[i];
		if (!strcmp(argv[0], cmd->name))
			return cmd->cmd(argc, argv);
	}

	fprintf(stderr, "No such command: %s\n", argv[0]);

	exit(1);
}
