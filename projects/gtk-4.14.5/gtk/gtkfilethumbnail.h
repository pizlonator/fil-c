/* gtkfilethumbnail.h
 *
 * Copyright 2022 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */


#pragma once

#include <gio/gio.h>

#include "gtkfilesystemmodelprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_FILE_THUMBNAIL    (_gtk_file_thumbnail_get_type ())
#define GTK_FILE_THUMBNAIL(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FILE_THUMBNAIL, GtkFileThumbnail))
#define GTK_IS_FILE_THUMBNAIL(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_FILE_THUMBNAIL))

typedef struct _GtkFileThumbnail      GtkFileThumbnail;

GType _gtk_file_thumbnail_get_type (void) G_GNUC_CONST;

GFileInfo *_gtk_file_thumbnail_get_info (GtkFileThumbnail *self);
void _gtk_file_thumbnail_set_info (GtkFileThumbnail *self,
                                   GFileInfo        *info);

int _gtk_file_thumbnail_get_icon_size (GtkFileThumbnail *self);
void _gtk_file_thumbnail_set_icon_size (GtkFileThumbnail *self,
                                        int               icon_size);

G_END_DECLS


