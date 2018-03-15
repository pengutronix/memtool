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
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include <linux/if.h>
#include <linux/mii.h>
#include <linux/sockios.h>

#include "fileaccpriv.h"

#define container_of(ptr, type, member) \
	(type *)((char *)(ptr) - (char *) &((type *)0)->member)

struct memtool_mdio_fd {
	struct memtool_fd mfd;
	int fd;
	uint16_t phy_id;
	char ifrn_name[IFNAMSIZ];
};

static ssize_t mdio_read(struct memtool_fd *handle, off_t offset,
			 void *buf, size_t nbytes, int width)
{
	struct memtool_mdio_fd *mdio_fd =
		container_of(handle, struct memtool_mdio_fd, mfd);
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (void *)&ifr.ifr_data;
	size_t i = 0;
	int ret;

	if (width != 2) {
		fprintf(stderr,
			"mdio can only be accessed with memory width 2\n");
		return -EINVAL;
	}

	assert((nbytes & 1) == 0);

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, mdio_fd->ifrn_name);

	mii->phy_id = mdio_fd->phy_id;

	while (2 * i < nbytes) {
		mii->reg_num = offset / 2 + i;

		ret = ioctl(mdio_fd->fd, SIOCGMIIREG, &ifr);
		if (ret < 0) {
			perror("Failure to read register");
			return -1;
		}

		((uint16_t *)buf)[i] = mii->val_out;
		++i;
	}

	return 2 * i;
}

static ssize_t mdio_write(struct memtool_fd *handle, off_t offset,
			  const void *buf, size_t nbytes, int width)
{
	struct memtool_mdio_fd *mdio_fd =
		container_of(handle, struct memtool_mdio_fd, mfd);
	struct ifreq ifr;
	struct mii_ioctl_data *mii = (void *)&ifr.ifr_data;
	size_t i = 0;
	int ret;

	if (width != 2) {
		fprintf(stderr,
			"mdio can only be accessed with memory width 2\n");
		return -EINVAL;
	}

	assert((nbytes & 1) == 0);

	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, mdio_fd->ifrn_name);

	mii->phy_id = mdio_fd->phy_id;

	while (2 * i < nbytes) {
		mii->reg_num = offset / 2 + i;
		mii->val_in = ((uint16_t *)buf)[i];

		ret = ioctl(mdio_fd->fd, SIOCSMIIREG, &ifr);
		if (ret < 0) {
			perror("Failure to write register");
			return -1;
		}

		++i;
	}

	return 2 * i;
}

static int mdio_close(struct memtool_fd *handle)
{
	struct memtool_mdio_fd *mdio_fd =
		container_of(handle, struct memtool_mdio_fd, mfd);
	int ret;

	ret = close(mdio_fd->fd);

	free(mdio_fd);

	return ret;
}

struct memtool_fd *mdio_open(const char *spec, int flags)
{
	struct memtool_mdio_fd *mdio_fd;
	char *delim;
	char *endp;
	long int val;

	mdio_fd = malloc(sizeof(*mdio_fd));
	if (!mdio_fd) {
		fprintf(stderr, "Failure to allocate mdio_fd\n");
		return NULL;
	}

	mdio_fd->mfd.read = mdio_read;
	mdio_fd->mfd.write = mdio_write;
	mdio_fd->mfd.close = mdio_close;

	mdio_fd->fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (mdio_fd->fd < 0) {
		perror("socket");
		goto err_socket;
	}

	delim = strrchr(spec, '.');
	if (!delim) {
		fprintf(stderr, "Failed to parse phy specifier, no \".\"\n");
		goto err_parse;
	}

	if (delim - spec >= sizeof(mdio_fd->ifrn_name)) {
		fprintf(stderr, "device string too long\n");
		goto err_parse;
	}

	memcpy(mdio_fd->ifrn_name, spec, delim - spec);
	mdio_fd->ifrn_name[delim - spec] = '\0';

	val = strtol(delim + 1, &endp, 0);
	if (*endp != '\0') {
		fprintf(stderr, "failed to parse phy_id\n");
		goto err_parse;
	}

	if (val < 0 || val >= (1 << 16)) {
		fprintf(stderr, "phy_id out of range\n");
err_parse:
		close(mdio_fd->fd);
err_socket:
		free(mdio_fd);
		return NULL;
	}
	mdio_fd->phy_id = val;

	return &mdio_fd->mfd;
}
