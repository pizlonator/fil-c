/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * dhcpcd - DHCP client daemon
 * Copyright (c) 2009-2023 Roy Marples <roy@marples.name>
 * All rights reserved

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

/*
 * THIS IS A NASTY HACK THAT SHOULD NEVER HAVE HAPPENED
 * Basically we cannot include linux/if.h and net/if.h because
 * they have conflicting structures.
 * Sadly, linux/wireless.h includes linux/if.h all the time.
 * Some kernel-header installs fix this and some do not.
 * This file solely exists for those who do not.
 *
 * We *could* include wireless.h as that is designed for userspace,
 * but that then depends on the correct version of wireless-tools being
 * installed which isn't always the case.
 */

#include <sys/ioctl.h>
#include <sys/socket.h>

#include <linux/types.h>
#include <linux/rtnetlink.h>
/* Support older kernels */
#ifdef IFLA_WIRELESS
# include <linux/if.h>
# include <linux/wireless.h>
#else
# define IFLA_WIRELESS (IFLA_MASTER + 1)
#endif

#include <string.h>
#include <unistd.h>

#include "config.h"

/* We can't include if.h or dhcpcd.h because
 * they would pull in net/if.h, which defeats the purpose of this hack. */
#define IF_SSIDLEN 32
int if_getssid_wext(const char *ifname, uint8_t *ssid);

int
if_getssid_wext(const char *ifname, uint8_t *ssid)
{
#ifdef SIOCGIWESSID
	int s, retval;
	struct iwreq iwr;

	if ((s = xsocket(PF_INET, SOCK_DGRAM, 0)) == -1)
		return -1;
	memset(&iwr, 0, sizeof(iwr));
	strlcpy(iwr.ifr_name, ifname, sizeof(iwr.ifr_name));
	iwr.u.essid.pointer = ssid;
	iwr.u.essid.length = IF_SSIDLEN;

	if (ioctl(s, SIOCGIWESSID, &iwr) == 0)
		retval = iwr.u.essid.length;
	else
		retval = -1;
	close(s);
	return retval;
#else
	/* Stop gcc warning about unused parameters */
	ifname = ssid;
	return -1;
#endif
}
