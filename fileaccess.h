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

void *memtool_open(const char *spec, int flags);
ssize_t memtool_read(void *handle, off_t offset,
		     void *buf, size_t nbytes, int width);
ssize_t memtool_write(void *handle, off_t offset,
		      const void *buf, size_t nbytes, int width);
int memtool_close(void *handle);
