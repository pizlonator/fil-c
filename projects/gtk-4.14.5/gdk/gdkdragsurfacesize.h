/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2023 Red Hat
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
 *
 */

#pragma once

#if !defined(__GDK_H_INSIDE__) && !defined(GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdktypes.h>

G_BEGIN_DECLS

typedef struct _GdkDragSurfaceSize GdkDragSurfaceSize;

#define GDK_TYPE_DRAG_SURFACE_SIZE (gdk_drag_surface_size_get_type ())

GDK_AVAILABLE_IN_4_12
GType                   gdk_drag_surface_size_get_type  (void);

GDK_AVAILABLE_IN_4_12
void                    gdk_drag_surface_size_set_size  (GdkDragSurfaceSize *size,
                                                         int                 width,
                                                         int                 height);

G_END_DECLS
