/* GIO - GLib Input, Output and Streaming Library
 *
 * Copyright (C) 2017 Red Hat, Inc.
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
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Benjamin Otte <otte@gnome.org>
 *         Christian Kellner <gicmo@gnome.org>
 */

#pragma once

#include <gio/gio.h>
#include "gdktypes.h"

G_BEGIN_DECLS

#define GDK_TYPE_X11_SELECTION_INPUT_STREAM         (gdk_x11_selection_input_stream_get_type ())
#define GDK_X11_SELECTION_INPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_X11_SELECTION_INPUT_STREAM, GdkX11SelectionInputStream))
#define GDK_X11_SELECTION_INPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDK_TYPE_X11_SELECTION_INPUT_STREAM, GdkX11SelectionInputStreamClass))
#define GDK_IS_X11_SELECTION_INPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_X11_SELECTION_INPUT_STREAM))
#define GDK_IS_X11_SELECTION_INPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDK_TYPE_X11_SELECTION_INPUT_STREAM))
#define GDK_X11_SELECTION_INPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_X11_SELECTION_INPUT_STREAM, GdkX11SelectionInputStreamClass))

typedef struct GdkX11SelectionInputStream         GdkX11SelectionInputStream;
typedef struct GdkX11SelectionInputStreamClass    GdkX11SelectionInputStreamClass;

struct GdkX11SelectionInputStream
{
  GInputStream parent_instance;
};

struct GdkX11SelectionInputStreamClass
{
  GInputStreamClass parent_class;
};


GType          gdk_x11_selection_input_stream_get_type      (void) G_GNUC_CONST;

void           gdk_x11_selection_input_stream_new_async     (GdkDisplay                 *display,
                                                             const char                 *selection,
                                                             const char                 *target,
                                                             guint32                     timestamp,
                                                             int                         io_priority,
                                                             GCancellable               *cancellable,
                                                             GAsyncReadyCallback         callback,
                                                             gpointer                    user_data);
GInputStream * gdk_x11_selection_input_stream_new_finish    (GAsyncResult               *result,
                                                             const char                **type,
                                                             int                        *format,
                                                             GError                    **error);


G_END_DECLS

