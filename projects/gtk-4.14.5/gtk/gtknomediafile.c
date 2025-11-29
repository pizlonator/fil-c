/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtknomediafileprivate.h"

#include <glib/gi18n-lib.h>

struct _GtkNoMediaFile
{
  GtkMediaFile parent_instance;
};

struct _GtkNoMediaFileClass
{
  GtkMediaFileClass parent_class;
};

G_DEFINE_TYPE_WITH_CODE (GtkNoMediaFile, gtk_no_media_file, GTK_TYPE_MEDIA_FILE,
                         g_io_extension_point_implement (GTK_MEDIA_FILE_EXTENSION_POINT_NAME,
                                                         g_define_type_id,
                                                         "none",
                                                         G_MININT);)

static void
gtk_no_media_file_open (GtkMediaFile *file)
{
  gtk_media_stream_error (GTK_MEDIA_STREAM (file),
                          G_IO_ERROR,
                          G_IO_ERROR_NOT_SUPPORTED,
                          _("GTK could not find a media module. Check your installation."));
}

static void
gtk_no_media_file_class_init (GtkNoMediaFileClass *klass)
{
  GtkMediaFileClass *file_class = GTK_MEDIA_FILE_CLASS (klass);

  file_class->open = gtk_no_media_file_open;
}

static void
gtk_no_media_file_init (GtkNoMediaFile *video)
{
}

