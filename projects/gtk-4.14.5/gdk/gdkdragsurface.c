/*
 * Copyright © 2020 Red Hat, Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gdkprivate.h"
#include "gdkdragsurfaceprivate.h"
#include <glib/gi18n-lib.h>

/**
 * GdkDragSurface:
 *
 * A `GdkDragSurface` is an interface for surfaces used during DND.
 */

/**
 * GdkDragSurfaceInterface:
 *
 * The `GdkDragSurfaceInterface` implementation is private to GDK.
 */

G_DEFINE_INTERFACE (GdkDragSurface, gdk_drag_surface, GDK_TYPE_SURFACE)

enum
{
  COMPUTE_SIZE,

  N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

void
gdk_drag_surface_notify_compute_size (GdkDragSurface     *surface,
                                      GdkDragSurfaceSize *size)
{
  g_signal_emit (surface, signals[COMPUTE_SIZE], 0, size);
}

static gboolean
gdk_drag_surface_default_present (GdkDragSurface *drag_surface,
                                  int          width,
                                  int          height)
{
  return FALSE;
}

static void
gdk_drag_surface_default_init (GdkDragSurfaceInterface *iface)
{
  iface->present = gdk_drag_surface_default_present;

  /**
   * GdkDragSurface::compute-size:
   * @surface: a `GdkDragSurface`
   * @size: (type Gdk.DragSurfaceSize): the size of the drag surface
   *
   * Emitted when the size for the surface needs to be computed, when it is
   * present.
   *
   * This signal will normally be emitted during the native surface layout
   * cycle when the surface size needs to be recomputed.
   *
   * It is the responsibility of the drag surface user to handle this signal
   * and compute the desired size of the surface, storing the computed size
   * in the [struct@Gdk.DragSurfaceSize] object that is passed to the signal
   * handler, using [method@Gdk.DragSurfaceSize.set_size].
   *
   * Failing to set a size so will result in an arbitrary size being used as
   * a result.
   *
   * Since: 4.12
   */
  signals[COMPUTE_SIZE] =
    g_signal_new (I_("compute-size"),
                  GDK_TYPE_DRAG_SURFACE,
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DRAG_SURFACE_SIZE);
}

/**
 * gdk_drag_surface_present:
 * @drag_surface: the `GdkDragSurface` to show
 * @width: the unconstrained drag_surface width to layout
 * @height: the unconstrained drag_surface height to layout
 *
 * Present @drag_surface.
 *
 * Returns: %FALSE if it failed to be presented, otherwise %TRUE.
 */
gboolean
gdk_drag_surface_present (GdkDragSurface *drag_surface,
                          int             width,
                          int             height)
{
  g_return_val_if_fail (GDK_IS_DRAG_SURFACE (drag_surface), FALSE);
  g_return_val_if_fail (width > 0, FALSE);
  g_return_val_if_fail (height > 0, FALSE);

  return GDK_DRAG_SURFACE_GET_IFACE (drag_surface)->present (drag_surface, width, height);
}
