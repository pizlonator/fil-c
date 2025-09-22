/*
 * debug.c: debugging messages
 * Copyright (C) 2007, 2010 Colin Watson.
 *
 * This file is part of libpipeline.
 *
 * libpipeline is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * libpipeline is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libpipeline; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pipeline-private.h"

int debug_level = 0;

void init_debug (void)
{
	static int inited = 0;
	const char *pipeline_debug;

	if (inited)
		return;
	inited = 1;

	pipeline_debug = getenv ("PIPELINE_DEBUG");
	if (pipeline_debug && !strcmp (pipeline_debug, "1"))
		debug_level = 1;
}

static void PIPELINE_ATTR_FORMAT_PRINTF (1, 0)
        vdebug (const char *message, va_list args)
{
	if (debug_level)
		vfprintf (stderr, message, args);
}

void debug (const char *message, ...)
{
	init_debug ();

	if (debug_level) {
		va_list args;

		va_start (args, message);
		vdebug (message, args);
		va_end (args);
	}
}
