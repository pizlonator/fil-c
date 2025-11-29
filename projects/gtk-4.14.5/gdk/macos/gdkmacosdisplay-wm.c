/*
 * Copyright © 2022 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacosmonitor-private.h"
#include "gdkmacossurface-private.h"
#include "gdkmacostoplevelsurface-private.h"

#define WARP_OFFSET_X 15
#define WARP_OFFSET_Y 15

static void
_gdk_macos_display_position_toplevel_with_parent (GdkMacosDisplay *self,
                                                  GdkMacosSurface *surface,
                                                  GdkMacosSurface *parent,
                                                  int             *x,
                                                  int             *y)
{
  GdkRectangle surface_rect;
  GdkRectangle parent_rect;
  GdkMonitor *monitor;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (surface));
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (parent));

  monitor = _gdk_macos_surface_get_best_monitor (parent);

  /* Try to center on top of the parent but also try to make the whole thing
   * visible in case that lands us under the topbar/panel/etc.
   */
  parent_rect.x = parent->root_x;
  parent_rect.y = parent->root_y;
  parent_rect.width = GDK_SURFACE (parent)->width;
  parent_rect.height = GDK_SURFACE (parent)->height;

  surface_rect.width = GDK_SURFACE (surface)->width;
  surface_rect.height = GDK_SURFACE (surface)->height;
  surface_rect.x = parent_rect.x + ((parent_rect.width - surface_rect.width) / 2);
  surface_rect.y = parent_rect.y + ((parent_rect.height - surface_rect.height) / 2);

  _gdk_macos_monitor_clamp (GDK_MACOS_MONITOR (monitor), &surface_rect);

  *x = surface_rect.x;
  *y = surface_rect.y;
}

static inline gboolean
has_surface_at_origin (const GList *surfaces,
                       int          x,
                       int          y)
{
  for (const GList *iter = surfaces; iter; iter = iter->next)
    {
      GdkMacosSurface *surface = iter->data;

      if (surface->root_x == x && surface->root_y == y)
        return TRUE;
    }

  return FALSE;
}

static void
_gdk_macos_display_position_toplevel (GdkMacosDisplay *self,
                                      GdkMacosSurface *surface,
                                      GdkMonitor      *selected_monitor,
                                      int             *x,
                                      int             *y)
{
  cairo_rectangle_int_t surface_rect;
  GdkRectangle workarea;
  const GList *surfaces;
  GdkMonitor *monitor;
  CGPoint mouse;

  g_assert (GDK_IS_MACOS_DISPLAY (self));
  g_assert (GDK_IS_MACOS_TOPLEVEL_SURFACE (surface));

  mouse = [NSEvent mouseLocation];
  if (!selected_monitor)
    monitor = _gdk_macos_display_get_monitor_at_display_coords (self, mouse.x, mouse.y);
  else
    monitor = selected_monitor;

  gdk_macos_monitor_get_workarea (monitor, &workarea);

  /* First place at top-left of current monitor */
  surface_rect.width = GDK_SURFACE (surface)->width;
  surface_rect.height = GDK_SURFACE (surface)->height;
  surface_rect.x = workarea.x + ((workarea.width - surface_rect.width) / 2);
  surface_rect.y = workarea.y + ((workarea.height - surface_rect.height) / 2);

  _gdk_macos_monitor_clamp (GDK_MACOS_MONITOR (selected_monitor ? selected_monitor : surface->best_monitor), &surface_rect);

  *x = surface_rect.x;
  *y = surface_rect.y;

  /* Try to see if there are any other surfaces at this origin and if so,
   * adjust until we get something better.
   */
  surfaces = _gdk_macos_display_get_surfaces (self);
  while (has_surface_at_origin (surfaces, *x, *y))
    {
      *x += WARP_OFFSET_X;
      *y += WARP_OFFSET_Y;

      /* If we reached the bottom right, just bail and try the workspace origin */
      if (*x + WARP_OFFSET_X > workarea.x + workarea.width ||
          *y + WARP_OFFSET_Y > workarea.y + workarea.height)
        {
          *x = workarea.x;
          *y = workarea.y;
          return;
        }
    }
}

/*<private>
 * _gdk_macos_display_position_surface:
 *
 * Tries to position a window on a screen without landing in edges
 * and other weird areas the user can't use.
 */
void
_gdk_macos_display_position_surface (GdkMacosDisplay *self,
                                     GdkMacosSurface *surface,
                                     GdkMonitor      *monitor,
                                     int             *x,
                                     int             *y)
{
  GdkSurface *transient_for;

  g_return_if_fail (GDK_IS_MACOS_DISPLAY (self));
  g_return_if_fail (GDK_IS_MACOS_TOPLEVEL_SURFACE (surface));

  transient_for = GDK_SURFACE (surface)->transient_for;

  if (transient_for != NULL)
    _gdk_macos_display_position_toplevel_with_parent (self, surface, GDK_MACOS_SURFACE (transient_for), x, y);
  else
    _gdk_macos_display_position_toplevel (self, surface, monitor, x, y);
}
