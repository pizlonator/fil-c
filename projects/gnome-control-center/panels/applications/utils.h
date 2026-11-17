/* utils.h
 *
 * Copyright 2018 Matthias Clasen <matthias.clasen@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gio/gio.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void      file_remove_async    (GFile               *file,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             data);

gboolean  file_remove_finish   (GFile               *file,
                                GAsyncResult        *result,
                                GError             **error);

void      file_size_async      (GFile               *file,
                                GCancellable        *cancellable,
                                GAsyncReadyCallback  callback,
                                gpointer             data);

gboolean  file_size_finish     (GFile               *file,
                                GAsyncResult        *result,
                                guint64             *size,
                                GError             **error);

GKeyFile* get_flatpak_metadata (const gchar         *app_id);

guint64   get_flatpak_app_size (const gchar         *app_id);

guint64   get_snap_app_size    (const gchar         *snap_name);

gchar*    get_app_id           (GAppInfo            *info);

G_END_DECLS
