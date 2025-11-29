/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2013 Jan Arne Petersen
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

#if !defined (__GDKWAYLAND_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/wayland/gdkwayland.h> can be included directly."
#endif

#include <gdk/gdk.h>

#include <wayland-client.h>

G_BEGIN_DECLS

#ifdef GTK_COMPILATION
typedef struct _GdkWaylandSurface GdkWaylandSurface;
#else
typedef GdkSurface GdkWaylandSurface;
#endif

#define GDK_TYPE_WAYLAND_SURFACE              (gdk_wayland_surface_get_type())
#define GDK_WAYLAND_SURFACE(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_SURFACE, GdkWaylandSurface))
#define GDK_IS_WAYLAND_SURFACE(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_SURFACE))

GDK_AVAILABLE_IN_ALL
GType                    gdk_wayland_surface_get_type             (void);

GDK_AVAILABLE_IN_ALL
struct wl_surface       *gdk_wayland_surface_get_wl_surface       (GdkSurface *surface);

G_END_DECLS

