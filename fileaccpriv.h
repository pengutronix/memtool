/*
 * Copyright (C) 2018 Pengutronix, Uwe Kleine-König <oss-tools@pengutronix.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <sys/types.h>

struct memtool_fd {
	ssize_t (*read)(struct memtool_fd *handle, off_t offset,
			void *buf, size_t nbytes, int width);
	ssize_t (*write)(struct memtool_fd *handle, off_t offset,
			 const void *buf, size_t nbytes, int width);
	int (*close)(struct memtool_fd *handle);
};

struct memtool_fd *mdio_open(const char *spec, int flags);
struct memtool_fd *mmap_open(const char *spec, int flags);
