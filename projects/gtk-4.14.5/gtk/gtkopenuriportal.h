/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright 2017 Red Hat, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkwindow.h"
#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gboolean gtk_openuri_portal_is_available (void);

typedef enum
{
 GTK_OPENURI_FLAGS_ASK      = 1 << 0,
 GTK_OPENURI_FLAGS_WRITABLE = 1 << 1,
} GtkOpenuriFlags;

void     gtk_openuri_portal_open_async   (GFile               *file,
                                          gboolean             open_folder,
                                          GtkOpenuriFlags      flags,
                                          GtkWindow           *window,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data);

gboolean gtk_openuri_portal_open_finish  (GAsyncResult        *result,
                                          GError             **error);

void     gtk_openuri_portal_open_uri_async  (const char          *uri,
                                             GtkWindow           *window,
                                             GCancellable        *cancellable,
                                             GAsyncReadyCallback  callback,
                                             gpointer             user_data);

gboolean gtk_openuri_portal_open_uri_finish (GAsyncResult        *result,
                                             GError             **error);

G_END_DECLS
