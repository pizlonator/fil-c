/*
 * Copyright © 2021 Benjamin Otte
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
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtksymbolicpaintable.h"

#include "gtkenums.h"

/**
 * GtkSymbolicPaintable:
 *
 * `GtkSymbolicPaintable` is an interface that support symbolic colors in
 * paintables.
 *
 * `GdkPaintable`s implementing the interface will have the
 * [vfunc@Gtk.SymbolicPaintable.snapshot_symbolic] function called and
 * have the colors for drawing symbolic icons passed. At least 4 colors are guaranteed
 * to be passed every time.
 *
 * These 4 colors are the foreground color, and the colors to use for errors, warnings
 * and success information in that order.
 *
 * More colors may be added in the future.
 *
 * Since: 4.6
 */

G_DEFINE_INTERFACE (GtkSymbolicPaintable, gtk_symbolic_paintable, GDK_TYPE_PAINTABLE)

static void
gtk_symbolic_paintable_default_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                                  GdkSnapshot          *snapshot,
                                                  double                width,
                                                  double                height,
                                                  const GdkRGBA        *colors,
                                                  gsize                 n_colors)
{
  gdk_paintable_snapshot (GDK_PAINTABLE (paintable), snapshot, width, height);
}

static void
gtk_symbolic_paintable_default_init (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = gtk_symbolic_paintable_default_snapshot_symbolic;
}

/**
 * gtk_symbolic_paintable_snapshot_symbolic
 * @paintable: a `GtkSymbolicPaintable`
 * @snapshot: a `GdkSnapshot` to snapshot to
 * @width: width to snapshot in
 * @height: height to snapshot in
 * @colors: (array length=n_colors): a pointer to an array of colors
 * @n_colors: The number of colors
 *
 * Snapshots the paintable with the given colors.
 *
 * If less than 4 colors are provided, GTK will pad the array with default
 * colors.
 *
 * Since: 4.6
 */
void
gtk_symbolic_paintable_snapshot_symbolic (GtkSymbolicPaintable   *paintable,
                                          GdkSnapshot            *snapshot,
                                          double                  width,
                                          double                  height,
                                          const GdkRGBA          *colors,
                                          gsize                   n_colors)
{
  GtkSymbolicPaintableInterface *iface;

  g_return_if_fail (GTK_IS_SYMBOLIC_PAINTABLE (paintable));
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (colors != NULL || n_colors == 0);

  if (width <= 0.0 || height <= 0.0)
    return;

  iface = GTK_SYMBOLIC_PAINTABLE_GET_IFACE (paintable);

  if (n_colors >= 4)
    {
      iface->snapshot_symbolic (paintable, snapshot, width, height, colors, n_colors);
    }
  else
    {
      /* Taken from GTK3, no idea where it got those from */
      GdkRGBA real_colors[4] = {
        [GTK_SYMBOLIC_COLOR_FOREGROUND] = { 0.7450980392156863, 0.7450980392156863, 0.7450980392156863, 1.0 },
        [GTK_SYMBOLIC_COLOR_ERROR] = { 0.796887159533074, 0, 0, 1.0 },
        [GTK_SYMBOLIC_COLOR_WARNING] = { 0.9570458533607996, 0.47266346227206835, 0.2421911955443656, 1.0 },
        [GTK_SYMBOLIC_COLOR_SUCCESS] = { 0.3046921492332342,0.6015716792553597, 0.023437857633325704, 1.0 }
      };

      if (n_colors != 0)
        memcpy (real_colors, colors, sizeof (GdkRGBA) * n_colors);

      iface->snapshot_symbolic (paintable, snapshot, width, height, real_colors, 4);
    }
}
