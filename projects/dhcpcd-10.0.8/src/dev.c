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

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _INDEV
#include "common.h"
#include "dev.h"
#include "eloop.h"
#include "dhcpcd.h"
#include "logerr.h"

int
dev_initialised(struct dhcpcd_ctx *ctx, const char *ifname)
{

#ifdef PRIVSEP
	if (ctx->options & DHCPCD_PRIVSEP &&
	    !(ctx->options & DHCPCD_PRIVSEPROOT))
		return ps_root_dev_initialised(ctx, ifname);
#endif

	if (ctx->dev == NULL)
		return 1;
	return ctx->dev->initialised(ifname);
}

int
dev_listening(struct dhcpcd_ctx *ctx)
{

#ifdef PRIVSEP
	if (ctx->options & DHCPCD_PRIVSEP &&
	    !(ctx->options & DHCPCD_PRIVSEPROOT))
		return ps_root_dev_listening(ctx);
#endif

	if (ctx->dev == NULL)
		return 0;
	return ctx->dev->listening();
}

static void
dev_stop1(struct dhcpcd_ctx *ctx, int stop)
{

	if (ctx->dev) {
		if (stop)
			logdebugx("dev: unloaded %s", ctx->dev->name);
		eloop_event_delete(ctx->eloop, ctx->dev_fd);
		ctx->dev->stop();
		free(ctx->dev);
		ctx->dev = NULL;
		ctx->dev_fd = -1;
	}
	if (ctx->dev_handle) {
		dlclose(ctx->dev_handle);
		ctx->dev_handle = NULL;
	}
}

void
dev_stop(struct dhcpcd_ctx *ctx)
{

	dev_stop1(ctx, !(ctx->options & DHCPCD_FORKED));
}

static int
dev_start2(struct dhcpcd_ctx *ctx, const struct dev_dhcpcd *dev_dhcpcd,
    const char *name)
{
	char file[PATH_MAX];
	void *h;
	void (*fptr)(struct dev *, const struct dev_dhcpcd *);
	int r;

	snprintf(file, sizeof(file), DEVDIR "/%s", name);
	h = dlopen(file, RTLD_LAZY);
	if (h == NULL) {
		logerrx("dlopen: %s", dlerror());
		return -1;
	}
	fptr = dlsym(h, "dev_init");
	if (fptr == NULL) {
		logerrx("dlsym: %s", dlerror());
		dlclose(h);
		return -1;
	}
	if ((ctx->dev = calloc(1, sizeof(*ctx->dev))) == NULL) {
		logerr("%s: calloc", __func__);
		dlclose(h);
		return -1;
	}
	fptr(ctx->dev, dev_dhcpcd);
	if (ctx->dev->start  == NULL || (r = ctx->dev->start()) == -1) {
		free(ctx->dev);
		ctx->dev = NULL;
		dlclose(h);
		return -1;
	}
	loginfox("dev: loaded %s", ctx->dev->name);
	ctx->dev_handle = h;
	return r;
}

static int
dev_start1(struct dhcpcd_ctx *ctx, const struct dev_dhcpcd *dev_dhcpcd)
{
	DIR *dp;
	struct dirent *d;
	int r;

	if (ctx->dev) {
		logerrx("dev: already started %s", ctx->dev->name);
		return -1;
	}

	if (ctx->dev_load)
		return dev_start2(ctx, dev_dhcpcd, ctx->dev_load);

	dp = opendir(DEVDIR);
	if (dp == NULL) {
		logdebug("dev: %s", DEVDIR);
		return -1;
	}

	r = 0;
	while ((d = readdir(dp))) {
		if (d->d_name[0] == '.')
			continue;

		r = dev_start2(ctx, dev_dhcpcd, d->d_name);
		if (r != -1)
			break;
	}
	closedir(dp);
	return r;
}

static void
dev_handle_data(void *arg, unsigned short events)
{
	struct dhcpcd_ctx *ctx;

	if (events != ELE_READ)
		logerrx("%s: unexpected event 0x%04x", __func__, events);

	ctx = arg;
	if (ctx->dev->handle_device(arg) == -1) {
		/* XXX: an error occured. should we restart dev? */
	}
}

int
dev_start(struct dhcpcd_ctx *ctx, int (*handler)(void *, int, const char *))
{
	struct dev_dhcpcd dev_dhcpcd = {
		.handle_interface = handler,
	};

	if (ctx->dev_fd != -1) {
		logerrx("%s: already started on fd %d", __func__, ctx->dev_fd);
		return ctx->dev_fd;
	}

	ctx->dev_fd = dev_start1(ctx, &dev_dhcpcd);
	if (ctx->dev_fd != -1) {
		if (eloop_event_add(ctx->eloop, ctx->dev_fd, ELE_READ,
		    dev_handle_data, ctx) == -1)
		{
			logerr(__func__);
			dev_stop1(ctx, 1);
			return -1;
		}
	}

	return ctx->dev_fd;
}
