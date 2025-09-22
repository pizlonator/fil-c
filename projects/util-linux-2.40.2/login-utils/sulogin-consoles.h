/*
 * consoles.h	    Header file for routines to detect the system consoles
 *
 * Copyright (c) 2011 SuSE LINUX Products GmbH, All rights reserved.
 * Copyright (c) 2012 Werner Fink <werner@suse.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program (see the file COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Author: Werner Fink <werner@suse.de>
 */
#ifndef UTIL_LINUX_SULOGIN_CONSOLES_H
#define UTIL_LINUX_SULOGIN_CONSOLES_H

#include <sys/types.h>
#include <stdint.h>
#include <stdio.h>
#include <termios.h>

#include "list.h"
#include "ttyutils.h"

struct console {
	struct list_head entry;
	char *tty;
	FILE *file;
	uint32_t flags;
	int fd, id;
#define	CON_SERIAL	0x0001
#define	CON_NOTTY	0x0002
#define	CON_EIO		0x0004
	pid_t pid;
	struct chardata cp;
	struct termios tio;
#ifdef HAVE_LIBSELINUX
	char *reset_tty_context;
	char *user_tty_context;
#endif
};

extern int detect_consoles(const char *device, int fallback,
			   struct list_head *consoles);

extern void emergency_do_umounts(void);
extern void emergency_do_mounts(void);

#endif /* UTIL_LINUX_SULOGIN_CONSOLES_H */
