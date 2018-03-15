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

#include <string.h>
#include <stdio.h>

#include "fileaccess.h"
#include "fileaccpriv.h"

void *memtool_open(const char *spec, int flags)
{
	if (!strncmp(spec, "mmap:", 5)) {
		return mmap_open(spec + 5, flags);
	} else if (!strncmp(spec, "mdio:", 5)) {
#ifdef USE_MDIO
		return mdio_open(spec + 5, flags);
#else
		fprintf(stderr, "mdio support not compiled in\n");
		return NULL;
#endif
	} else {
		return mmap_open(spec, flags);
	}
}

ssize_t memtool_read(void *handle,
		     off_t offset, void *buf, size_t nbytes, int width)
{
	struct memtool_fd *mfd = handle;

	return mfd->read(mfd, offset, buf, nbytes, width);
}

ssize_t memtool_write(void *handle,
		      off_t offset, const void *buf, size_t nbytes, int width)
{
	struct memtool_fd *mfd = handle;

	return mfd->write(mfd, offset, buf, nbytes, width);
}

int memtool_close(void *handle)
{
	struct memtool_fd *mfd = handle;

	return mfd->close(mfd);
}
