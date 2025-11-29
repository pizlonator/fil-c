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
#include "gdkclipdrop-win32.h"

G_BEGIN_DECLS

#define GDK_TYPE_WIN32_HDATA_OUTPUT_STREAM         (gdk_win32_hdata_output_stream_get_type ())
#define GDK_WIN32_HDATA_OUTPUT_STREAM(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WIN32_HDATA_OUTPUT_STREAM, GdkWin32HDataOutputStream))
#define GDK_WIN32_HDATA_OUTPUT_STREAM_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDK_TYPE_WIN32_HDATA_OUTPUT_STREAM, GdkWin32HDataOutputStreamClass))
#define GDK_IS_WIN32_HDATA_OUTPUT_STREAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_WIN32_HDATA_OUTPUT_STREAM))
#define GDK_IS_WIN32_HDATA_OUTPUT_STREAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDK_TYPE_WIN32_HDATA_OUTPUT_STREAM))
#define GDK_WIN32_HDATA_OUTPUT_STREAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WIN32_HDATA_OUTPUT_STREAM, GdkWin32HDataOutputStreamClass))

typedef struct GdkWin32HDataOutputStream         GdkWin32HDataOutputStream;
typedef struct GdkWin32HDataOutputStreamClass    GdkWin32HDataOutputStreamClass;

typedef void (* GdkWin32HDataOutputHandler) (GOutputStream *stream, GdkWin32ContentFormatPair pair, gpointer user_data);

struct GdkWin32HDataOutputStream
{
  GOutputStream parent_instance;
};

struct GdkWin32HDataOutputStreamClass
{
  GOutputStreamClass parent_class;
};

GType gdk_win32_hdata_output_stream_get_type            (void) G_GNUC_CONST;

GOutputStream *gdk_win32_hdata_output_stream_new        (GdkWin32ContentFormatPair  *pair,
                                                         GError                    **error);

HANDLE         gdk_win32_hdata_output_stream_get_handle (GdkWin32HDataOutputStream  *stream,
                                                         gboolean                   *is_hdata);

G_END_DECLS

