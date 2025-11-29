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
#include <gdk/wayland/gdkwaylandsurface.h>

#include <wayland-client.h>

G_BEGIN_DECLS

#ifdef GTK_COMPILATION
typedef struct _GdkWaylandToplevel GdkWaylandToplevel;
#else
typedef GdkToplevel GdkWaylandToplevel;
#endif

#define GDK_TYPE_WAYLAND_TOPLEVEL             (gdk_wayland_toplevel_get_type())
#define GDK_WAYLAND_TOPLEVEL(object)          (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_TOPLEVEL, GdkWaylandToplevel))
#define GDK_IS_WAYLAND_TOPLEVEL(object)       (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_TOPLEVEL))

GDK_AVAILABLE_IN_ALL
GType                    gdk_wayland_toplevel_get_type            (void);


typedef void (*GdkWaylandToplevelExported) (GdkToplevel *toplevel,
                                            const char  *handle,
                                            gpointer     user_data);

GDK_AVAILABLE_IN_ALL
gboolean                 gdk_wayland_toplevel_export_handle (GdkToplevel              *toplevel,
                                                             GdkWaylandToplevelExported callback,
                                                             gpointer                 user_data,
                                                             GDestroyNotify           destroy_func);

GDK_DEPRECATED_IN_4_12_FOR(gdk_wayland_toplevel_drop_exported_handle)
void                     gdk_wayland_toplevel_unexport_handle (GdkToplevel *toplevel);

GDK_AVAILABLE_IN_4_12
void                     gdk_wayland_toplevel_drop_exported_handle (GdkToplevel *toplevel,
                                                                    const char  *handle);

GDK_AVAILABLE_IN_ALL
gboolean                 gdk_wayland_toplevel_set_transient_for_exported (GdkToplevel *toplevel,
                                                                         const char   *parent_handle_str);

GDK_AVAILABLE_IN_ALL
void                     gdk_wayland_toplevel_set_application_id (GdkToplevel *toplevel,
                                                                  const char  *application_id);

G_END_DECLS
