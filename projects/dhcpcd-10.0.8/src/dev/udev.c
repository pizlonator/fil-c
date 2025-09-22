/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2006-2023 Roy Marples <roy@marples.name>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef LIBUDEV_NOINIT
#  define LIBUDEV_I_KNOW_THE_API_IS_SUBJECT_TO_CHANGE
#  warning This version of udev is too old does not support
#  warning per device initialization checks.
#  warning As such, dhcpcd will need to depend on the
#  warning udev-settle service or similar if starting
#  warning in master mode.
#endif

#include <libudev.h>
#include <string.h>

#include "../common.h"
#include "../dev.h"
#include "../if.h"
#include "../logerr.h"

static const char udev_name[] = "udev";
static struct udev *udev;
static struct udev_monitor *monitor;

static struct dev_dhcpcd dhcpcd;

static int
udev_listening(void)
{

	return monitor ? 1 : 0;
}

static int
udev_initialised(const char *ifname)
{
	struct udev_device *device;
	int r;

	device = udev_device_new_from_subsystem_sysname(udev, "net", ifname);
	if (device) {
#ifndef LIBUDEV_NOINIT
		r = udev_device_get_is_initialized(device);
#else
		r = 1;
#endif
		udev_device_unref(device);
	} else
		r = 0;
	return r;
}

static int
udev_handle_device(void *ctx)
{
	struct udev_device *device;
	const char *subsystem, *ifname, *action;

	device = udev_monitor_receive_device(monitor);
	if (device == NULL) {
		logerrx("libudev: received NULL device");
		return -1;
	}

	subsystem = udev_device_get_subsystem(device);
	ifname = udev_device_get_sysname(device);
	action = udev_device_get_action(device);

	/* udev filter documentation says "usually" so double check */
	if (strcmp(subsystem, "net") == 0) {
		logdebugx("%s: libudev: %s", ifname, action);
		if (strcmp(action, "add") == 0 || strcmp(action, "move") == 0)
			dhcpcd.handle_interface(ctx, 1, ifname);
		else if (strcmp(action, "remove") == 0)
			dhcpcd.handle_interface(ctx, -1, ifname);
	}

	udev_device_unref(device);
	return 1;
}

static void
udev_stop(void)
{

	if (monitor) {
		udev_monitor_unref(monitor);
		monitor = NULL;
	}

	if (udev) {
		udev_unref(udev);
		udev = NULL;
	}
}

static int
udev_start(void)
{
	char netns[PATH_MAX];
	int fd;

	if (if_getnetworknamespace(netns, sizeof(netns)) != NULL) {
		logdebugx("udev does not work in a network namespace");
		return -1;
	}

	if (udev) {
		logerrx("udev: already started");
		return -1;
	}

	logdebugx("udev: starting");
	udev = udev_new();
	if (udev == NULL) {
		logerr("udev_new");
		return -1;
	}
	monitor = udev_monitor_new_from_netlink(udev, "udev");
	if (monitor == NULL) {
		logerr("udev_monitor_new_from_netlink");
		goto bad;
	}
#ifndef LIBUDEV_NOFILTER
	if (udev_monitor_filter_add_match_subsystem_devtype(monitor,
	    "net", NULL) != 0)
	{
		logerr("udev_monitor_filter_add_match_subsystem_devtype");
		goto bad;
	}
#endif
	if (udev_monitor_enable_receiving(monitor) != 0) {
		logerr("udev_monitor_enable_receiving");
		goto bad;
	}
	fd = udev_monitor_get_fd(monitor);
	if (fd == -1) {
		logerr("udev_monitor_get_fd");
		goto bad;
	}
	return fd;

bad:
	udev_stop();
	return -1;
}

int
dev_init(struct dev *dev, const struct dev_dhcpcd *dev_dhcpcd)
{

	dev->name = udev_name;
	dev->initialised = udev_initialised;
	dev->listening = udev_listening;
	dev->handle_device = udev_handle_device;
	dev->stop = udev_stop;
	dev->start = udev_start;

	dhcpcd = *dev_dhcpcd;

	return 0;
}
