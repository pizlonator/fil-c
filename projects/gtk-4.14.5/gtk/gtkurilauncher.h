/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

#define GTK_TYPE_URI_LAUNCHER (gtk_uri_launcher_get_type ())

GDK_AVAILABLE_IN_4_10
G_DECLARE_FINAL_TYPE (GtkUriLauncher, gtk_uri_launcher, GTK, URI_LAUNCHER, GObject)

GDK_AVAILABLE_IN_4_10
GtkUriLauncher * gtk_uri_launcher_new                         (const char          *uri);

GDK_AVAILABLE_IN_4_10
const char     * gtk_uri_launcher_get_uri                     (GtkUriLauncher      *self);
GDK_AVAILABLE_IN_4_10
void             gtk_uri_launcher_set_uri                     (GtkUriLauncher      *self,
                                                               const char          *uri);

GDK_AVAILABLE_IN_4_10
void             gtk_uri_launcher_launch                      (GtkUriLauncher      *self,
                                                               GtkWindow           *parent,
                                                               GCancellable        *cancellable,
                                                               GAsyncReadyCallback  callback,
                                                               gpointer             user_data);

GDK_AVAILABLE_IN_4_10
gboolean         gtk_uri_launcher_launch_finish               (GtkUriLauncher     *self,
                                                               GAsyncResult        *result,
                                                               GError             **error);


G_END_DECLS
