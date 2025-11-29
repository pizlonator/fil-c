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

#include <wayland-client.h>

G_BEGIN_DECLS

#define GDK_TYPE_WAYLAND_CLIPBOARD    (gdk_wayland_clipboard_get_type ())
#define GDK_WAYLAND_CLIPBOARD(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WAYLAND_CLIPBOARD, GdkWaylandClipboard))
#define GDK_IS_WAYLAND_CLIPBOARD(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WAYLAND_CLIPBOARD))

typedef struct _GdkWaylandClipboard GdkWaylandClipboard;

GType                   gdk_wayland_clipboard_get_type              (void) G_GNUC_CONST;

GdkClipboard *          gdk_wayland_clipboard_new                   (GdkDisplay             *display);

void                    gdk_wayland_clipboard_claim_remote          (GdkWaylandClipboard    *cb,
                                                                     struct wl_data_offer   *offer,
                                                                     GdkContentFormats      *formats);

G_END_DECLS

