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
 */

#pragma once

#include <gio/gio.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS

#define GDK_TYPE_X11_TEXT_LIST_CONVERTER         (gdk_x11_text_list_converter_get_type ())
#define GDK_X11_TEXT_LIST_CONVERTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_X11_TEXT_LIST_CONVERTER, GdkX11TextListConverter))
#define GDK_IS_X11_TEXT_LIST_CONVERTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_X11_TEXT_LIST_CONVERTER))

typedef struct _GdkX11TextListConverter   GdkX11TextListConverter;

GType              gdk_x11_text_list_converter_get_type         (void) G_GNUC_CONST;

GConverter *       gdk_x11_text_list_converter_to_utf8_new      (GdkDisplay     *display,
                                                                 const char     *encoding,
                                                                 int             format);
GConverter *       gdk_x11_text_list_converter_from_utf8_new    (GdkDisplay     *display,
                                                                 const char     *encoding,
                                                                 int             format);


G_END_DECLS

