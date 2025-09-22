/*
 * Copyright (C) 2012 Colin Watson.
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
 * USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "xalloc.h"

#include "common.h"

char *temp_dir;

void temp_dir_setup (void)
{
	temp_dir = xstrdup ("libpipeline.XXXXXX");
	if (mkdtemp (temp_dir) == NULL)
		ck_abort_msg ("mkdtemp failed: %s", strerror (errno));
}

void temp_dir_teardown (void)
{
	pipeline_run (
	        pipeline_new_command_args ("rm", "-rf", temp_dir, (void *) 0));
	free (temp_dir);
	temp_dir = NULL;
}
