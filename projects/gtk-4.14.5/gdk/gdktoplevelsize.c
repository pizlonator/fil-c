/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 Red Hat
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

#include "gdktoplevelsizeprivate.h"

/**
 * GdkToplevelSize:
 *
 * The `GdkToplevelSize` struct contains information that is useful
 * to compute the size of a toplevel.
 */

G_DEFINE_POINTER_TYPE (GdkToplevelSize, gdk_toplevel_size)

#define UNCONFIGURED_WIDTH 400
#define UNCONFIGURED_HEIGHT 300

#define DEFAULT_BOUNDS_WIDTH INT_MAX
#define DEFAULT_BOUNDS_HEIGHT INT_MAX

void
gdk_toplevel_size_init (GdkToplevelSize *size,
                        int              bounds_width,
                        int              bounds_height)
{
  *size = (GdkToplevelSize) { 0 };

  size->bounds_width = bounds_width;
  size->bounds_height = bounds_height;

  size->width = UNCONFIGURED_WIDTH;
  size->height = UNCONFIGURED_HEIGHT;
}

/**
 * gdk_toplevel_size_get_bounds:
 * @size: a `GdkToplevelSize`
 * @bounds_width: (out): return location for width
 * @bounds_height: (out): return location for height
 *
 * Retrieves the bounds the toplevel is placed within.
 *
 * The bounds represent the largest size a toplevel may have while still being
 * able to fit within some type of boundary. Depending on the backend, this may
 * be equivalent to the dimensions of the work area or the monitor on which the
 * window is being presented on, or something else that limits the way a
 * toplevel can be presented.
 */
void
gdk_toplevel_size_get_bounds (GdkToplevelSize *size,
                              int             *bounds_width,
                              int             *bounds_height)
{
  g_return_if_fail (bounds_width);
  g_return_if_fail (bounds_height);

  if (size->bounds_width > 0)
    *bounds_width = size->bounds_width;
  else
    *bounds_width = DEFAULT_BOUNDS_WIDTH;

  if (size->bounds_height > 0)
    *bounds_height = size->bounds_height;
  else
    *bounds_height = DEFAULT_BOUNDS_HEIGHT;
}

/**
 * gdk_toplevel_size_set_size:
 * @size: a `GdkToplevelSize`
 * @width: the width
 * @height: the height
 *
 * Sets the size the toplevel prefers to be resized to.
 *
 * The size should be within the bounds (see
 * [method@Gdk.ToplevelSize.get_bounds]). The set size should
 * be considered as a hint, and should not be assumed to be
 * respected by the windowing system, or backend.
 */
void
gdk_toplevel_size_set_size (GdkToplevelSize *size,
                            int              width,
                            int              height)
{
  size->width = width;
  size->height = height;
}

/**
 * gdk_toplevel_size_set_min_size:
 * @size: a `GdkToplevelSize`
 * @min_width: the minimum width
 * @min_height: the minimum height
 *
 * Sets the minimum size of the toplevel.
 *
 * The minimum size corresponds to the limitations the toplevel can be shrunk
 * to, without resulting in incorrect painting. A user of a `GdkToplevel` should
 * calculate these given both the existing size, and the bounds retrieved from
 * the `GdkToplevelSize` object.
 *
 * The minimum size should be within the bounds (see
 * [method@Gdk.ToplevelSize.get_bounds]).
 */
void
gdk_toplevel_size_set_min_size (GdkToplevelSize *size,
                                int              min_width,
                                int              min_height)
{
  size->min_width = min_width;
  size->min_height = min_height;
}

/**
 * gdk_toplevel_size_set_shadow_width:
 * @size: a `GdkToplevelSize`
 * @left: width of the left part of the shadow
 * @right: width of the right part of the shadow
 * @top: height of the top part of the shadow
 * @bottom: height of the bottom part of the shadow
 *
 * Sets the shadows size of the toplevel.
 *
 * The shadow width corresponds to the part of the computed surface size
 * that would consist of the shadow margin surrounding the window, would
 * there be any.
 * 
 * Shadow width should only be set if
 * [method@Gtk.Display.supports_shadow_width] is %TRUE.
 */
void
gdk_toplevel_size_set_shadow_width (GdkToplevelSize *size,
                                    int              left,
                                    int              right,
                                    int              top,
                                    int              bottom)
{
  size->shadow.is_valid = TRUE;
  size->shadow.left = left;
  size->shadow.right = right;
  size->shadow.top = top;
  size->shadow.bottom = bottom;
}

void
gdk_toplevel_size_validate (GdkToplevelSize *size)
{
#if 0
  int geometry_width, geometry_height;

  geometry_width = size->width;
  geometry_height = size->height;
  if (size->shadow.is_valid)
    {
      geometry_width -= size->shadow.left + size->shadow.right;
      geometry_height -= size->shadow.top + size->shadow.bottom;
    }
#endif
}
