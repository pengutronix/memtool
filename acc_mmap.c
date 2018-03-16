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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "fileaccpriv.h"

#define container_of(ptr, type, member) \
	(type *)((char *)(ptr) - (char *) &((type *)0)->member)

static off_t mmap_pagesize(void) __attribute__((const));
static off_t mmap_pagesize(void)
{
	static off_t pagesize;

	if (pagesize == 0)
		pagesize = sysconf(_SC_PAGE_SIZE);

	if (pagesize == 0)
		pagesize = 4096;

	return pagesize;
}

struct memtool_mmap_fd {
	struct memtool_fd mfd;
	struct stat s;
	int fd;
};

static ssize_t mmap_read(struct memtool_fd *handle, off_t offset,
			 void *buf, size_t nbytes, int width)
{
	struct memtool_mmap_fd *mmap_fd =
		container_of(handle, struct memtool_mmap_fd, mfd);
	struct stat *s = &mmap_fd->s;
	off_t map_start, map_off;
	void *map;
	size_t i = 0;
	int ret;

	if (S_ISREG(s->st_mode)) {
		if (s->st_size <= offset) {
			errno = EINVAL;
			perror("File to small");
			return -1;
		}

		if (s->st_size < offset + nbytes)
			/* truncating */
			nbytes = s->st_size - offset;
	}

	map_start = offset & ~(mmap_pagesize() - 1);
	map_off = offset - map_start;

	map = mmap(NULL, nbytes + map_off, PROT_READ,
		   MAP_SHARED, mmap_fd->fd, map_start);
	if (map == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	while (i * width + width <= nbytes) {
		switch (width) {
		case 1:
			((uint8_t *)buf)[i] = ((uint8_t *)(map + map_off))[i];
			break;
		case 2:
			((uint16_t *)buf)[i] = ((uint16_t *)(map + map_off))[i];
			break;
		case 4:
			((uint32_t *)buf)[i] = ((uint32_t *)(map + map_off))[i];
			break;
		case 8:
			((uint64_t *)buf)[i] = ((uint64_t *)(map + map_off))[i];
			break;
		}
		++i;
	}

	ret = munmap(map, nbytes + map_off);
	if (ret < 0) {
		perror("munmap");
		return -1;
	}

	return i * width;
}

static ssize_t mmap_write(struct memtool_fd *handle, off_t offset,
			  const void *buf, size_t nbytes, int width)
{
	struct memtool_mmap_fd *mmap_fd =
		container_of(handle, struct memtool_mmap_fd, mfd);
	struct stat *s = &mmap_fd->s;
	off_t map_start, map_off;
	void *map;
	size_t i = 0;
	int ret;

	if (S_ISREG(s->st_mode) && s->st_size < offset + nbytes) {
		ret = posix_fallocate(mmap_fd->fd, offset, nbytes);
		if (ret) {
			errno = ret;
			perror("fallocate");
			return -1;
		}
		s->st_size = offset + nbytes;
	}

	map_start = offset & ~(mmap_pagesize() - 1);
	map_off = offset - map_start;

	map = mmap(NULL, nbytes + map_off, PROT_WRITE,
		   MAP_SHARED, mmap_fd->fd, map_start);
	if (map == MAP_FAILED) {
		perror("mmap");
		return -1;
	}

	while (i * width + width <= nbytes) {
		switch (width) {
		case 1:
			((uint8_t *)(map + map_off))[i] = ((uint8_t *)buf)[i];
			break;
		case 2:
			((uint16_t *)(map + map_off))[i] = ((uint16_t *)buf)[i];
			break;
		case 4:
			((uint32_t *)(map + map_off))[i] = ((uint32_t *)buf)[i];
			break;
		case 8:
			((uint64_t *)(map + map_off))[i] = ((uint64_t *)buf)[i];
			break;
		}
		++i;
	}

	ret = munmap(map, nbytes + map_off);
	if (ret < 0) {
		perror("munmap");
		return -1;
	}

	return i * width;
}

static int mmap_close(struct memtool_fd *handle)
{
	struct memtool_mmap_fd *mmap_fd =
		container_of(handle, struct memtool_mmap_fd, mfd);
	int ret;

	ret = close(mmap_fd->fd);

	free(mmap_fd);

	return ret;
}

struct memtool_fd *mmap_open(const char *spec, int flags)
{
	struct memtool_mmap_fd *mmap_fd;
	int ret;

	mmap_fd = malloc(sizeof(*mmap_fd));
	if (!mmap_fd) {
		fprintf(stderr, "Failure to allocate mmap_fd\n");
		return NULL;
	}

	mmap_fd->mfd.read = mmap_read;
	mmap_fd->mfd.write = mmap_write;
	mmap_fd->mfd.close = mmap_close;

	mmap_fd->fd = open(spec, flags, S_IRUSR | S_IWUSR);
	if (mmap_fd->fd < 0) {
		perror("open");
		free(mmap_fd);
		return NULL;
	}

	ret = fstat(mmap_fd->fd, &mmap_fd->s);
	if (ret) {
		perror("fstat");
		close(mmap_fd->fd);
		free(mmap_fd);
		return NULL;
	}

	return &mmap_fd->mfd;
}
