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

#include "gdkseat-wayland.h"
#include <wayland-client.h>

G_BEGIN_DECLS

#define GDK_TYPE_WAYLAND_PRIMARY    (gdk_wayland_primary_get_type ())
#define GDK_WAYLAND_PRIMARY(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GDK_TYPE_WAYLAND_PRIMARY, GdkWaylandPrimary))
#define GDK_IS_WAYLAND_PRIMARY(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GDK_TYPE_WAYLAND_PRIMARY))

typedef struct _GdkWaylandPrimary GdkWaylandPrimary;

GType                   gdk_wayland_primary_get_type            (void) G_GNUC_CONST;

GdkClipboard *          gdk_wayland_primary_new                 (GdkWaylandSeat         *seat);

G_END_DECLS

