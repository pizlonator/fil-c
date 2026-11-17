/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Copyright (C) 2004 Red Hat, Inc.
 *
 *  Nautilus is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Nautilus is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#include "file-share-properties.h"

#include <string.h>
#include <stdio.h>

#include <glib.h>


#define REALM "Please log in as the user guest"
#define USER "guest"

void
file_share_write_out_password (const char *password)
{
    g_autofree gchar *to_hash = NULL;
    g_autofree gchar *ascii_digest = NULL;
    g_autofree gchar *line = NULL;
    g_autofree gchar *filename = NULL;
    FILE *file;

    to_hash = g_strdup_printf ("%s:%s:%s", USER, REALM, password);
    ascii_digest = g_compute_checksum_for_string (G_CHECKSUM_MD5, to_hash, strlen (to_hash));
    line = g_strdup_printf ("%s:%s:%s\n", USER, REALM, ascii_digest);

    filename = g_build_filename (g_get_user_config_dir (), "user-share", "passwd", NULL);

    file = fopen (filename, "w");
    if (file != NULL) {
	fwrite (line, strlen (line), 1, file);
	fclose (file);
    }
}
