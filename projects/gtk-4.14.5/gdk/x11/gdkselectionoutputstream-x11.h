/* GIO - GLib Output, Output and Streaming Library
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

#include <X11/Xlib.h>

G_BEGIN_DECLS

#define GDK_TYPE_X11_SELECTION_OUTPUT_STREAM         (gdk_x11_selection_output_stream_get_type ())
#define GDK_X11_SELECTION_OUTPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM, GdkX11SelectionOutputStream))
#define GDK_X11_SELECTION_OUTPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM, GdkX11SelectionOutputStreamClass))
#define GDK_IS_X11_SELECTION_OUTPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM))
#define GDK_IS_X11_SELECTION_OUTPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM))
#define GDK_X11_SELECTION_OUTPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_X11_SELECTION_OUTPUT_STREAM, GdkX11SelectionOutputStreamClass))

typedef struct GdkX11SelectionOutputStream         GdkX11SelectionOutputStream;
typedef struct GdkX11SelectionOutputStreamClass    GdkX11SelectionOutputStreamClass;

typedef void (* GdkX11SelectionOutputHandler) (GOutputStream *stream, const char *mime_type, gpointer user_data);

struct GdkX11SelectionOutputStream
{
  GOutputStream parent_instance;
};

struct GdkX11SelectionOutputStreamClass
{
  GOutputStreamClass parent_class;
};


GType           gdk_x11_selection_output_stream_get_type        (void) G_GNUC_CONST;

void            gdk_x11_selection_output_streams_create         (GdkDisplay             *display,
                                                                 GdkContentFormats      *formats,
                                                                 Window                  requestor,
                                                                 Atom                    selection,
                                                                 Atom                    target,
                                                                 Atom                    property,
                                                                 gulong                  timestamp,
                                                                 GdkX11SelectionOutputHandler handler,
                                                                 gpointer                user_data);

G_END_DECLS

