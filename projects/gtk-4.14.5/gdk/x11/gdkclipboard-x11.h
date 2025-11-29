/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2017 Red Hat, Inc.
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

#include "gdk/gdkclipboard.h"

#include <X11/Xlib.h>


G_BEGIN_DECLS

#define GDK_TYPE_X11_CLIPBOARD    (gdk_x11_clipboard_get_type ())
#define GDK_X11_CLIPBOARD(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_X11_CLIPBOARD, GdkX11Clipboard))
#define GDK_IS_X11_CLIPBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_X11_CLIPBOARD))

typedef struct _GdkX11Clipboard GdkX11Clipboard;

GType                   gdk_x11_clipboard_get_type              (void) G_GNUC_CONST;

GdkClipboard *          gdk_x11_clipboard_new                   (GdkDisplay             *display,
                                                                 const char             *selection);

GSList *                gdk_x11_clipboard_formats_to_targets    (GdkContentFormats      *formats);
Atom *                  gdk_x11_clipboard_formats_to_atoms      (GdkDisplay             *display,
                                                                 gboolean                include_special,
                                                                 GdkContentFormats      *formats,
                                                                 gsize                  *n_atoms);

G_END_DECLS

