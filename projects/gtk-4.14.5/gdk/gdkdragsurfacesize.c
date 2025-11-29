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

#include "config.h"

#include "gdkdragsurfacesizeprivate.h"

/**
 * GdkDragSurfaceSize:
 *
 * The `GdkDragSurfaceSize` struct contains information that is useful
 * to compute the size of a drag surface.
 *
 * Since: 4.12
 */

G_DEFINE_POINTER_TYPE (GdkDragSurfaceSize, gdk_drag_surface_size)

void
gdk_drag_surface_size_init (GdkDragSurfaceSize *size)
{
  *size = (GdkDragSurfaceSize) { 0 };
}

/**
 * gdk_drag_surface_size_set_size:
 * @size: a `GdkDragSurfaceSize`
 * @width: the width
 * @height: the height
 *
 * Sets the size the drag surface prefers to be resized to.
 *
 * Since: 4.12
 */
void
gdk_drag_surface_size_set_size (GdkDragSurfaceSize *size,
                                int                 width,
                                int                 height)
{
  size->width = width;
  size->height = height;
}
