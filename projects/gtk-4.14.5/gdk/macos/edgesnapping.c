/* edgesnapping.c
 *
 * Copyright © 2020 Red Hat, Inc.
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "gdkrectangle.h"
#include "edgesnapping.h"

#define LEAVE_THRESHOLD 3.0
#define ENTER_THRESHOLD 2.0

#define X1(r) ((r)->x)
#define X2(r) ((r)->x + (r)->width)
#define Y1(r) ((r)->y)
#define Y2(r) ((r)->y + (r)->height)

void
_edge_snapping_init (EdgeSnapping       *self,
                     const GdkRectangle *geometry,
                     const GdkRectangle *workarea,
                     const GdkPoint     *pointer_position,
                     const GdkRectangle *window)
{
  g_assert (self != NULL);
  g_assert (geometry != NULL);
  g_assert (workarea != NULL);
  g_assert (pointer_position != NULL);

  self->geometry = *geometry;
  self->workarea = *workarea;
  self->last_pointer_position = *pointer_position;
  self->pointer_offset_in_window.x = pointer_position->x - window->x;
  self->pointer_offset_in_window.y = pointer_position->y - window->y;
}

static void
edge_snapping_constrain_left (EdgeSnapping       *self,
                              int                 change,
                              const GdkRectangle *geometry,
                              GdkRectangle       *window)
{
  if (change < 0)
    {
      if (X1 (window) < X1 (geometry) &&
          X1 (window) > X1 (geometry) - LEAVE_THRESHOLD &&
          ABS (change) < LEAVE_THRESHOLD)
        window->x = geometry->x;
    }

  /* We don't constrain when returning from left edge */
}

static void
edge_snapping_constrain_right (EdgeSnapping       *self,
                               int                 change,
                               const GdkRectangle *geometry,
                               GdkRectangle       *window)
{
  if (change > 0)
    {
      if (X2 (window) > X2 (geometry) &&
          X2 (window) < X2 (geometry) + LEAVE_THRESHOLD &&
          ABS (change) < LEAVE_THRESHOLD)
        window->x = X2 (geometry) - window->width;
    }

  /* We don't constrain when returning from right edge */
}

static void
edge_snapping_constrain_top (EdgeSnapping       *self,
                             int                 change,
                             const GdkRectangle *geometry,
                             GdkRectangle       *window)
{
  if (change < 0)
    {
      if (Y1 (window) < Y1 (geometry))
        window->y = geometry->y;
    }

  /* We don't constrain when returning from top edge */
}

static void
edge_snapping_constrain_bottom (EdgeSnapping       *self,
                                int                 change,
                                const GdkRectangle *geometry,
                                GdkRectangle       *window)
{
  if (change > 0)
    {
      if (Y2 (window) > Y2 (geometry) &&
          Y2 (window) < Y2 (geometry) + LEAVE_THRESHOLD &&
          ABS (change) < LEAVE_THRESHOLD)
        window->y = Y2 (geometry) - window->height;
    }
  else if (change < 0)
    {
      if (Y2 (window) < Y2 (geometry) &&
          Y2 (window) > Y2 (geometry) - ENTER_THRESHOLD &&
          ABS (change) < ENTER_THRESHOLD)
        window->y = Y2 (geometry) - window->height;
    }

}

static void
edge_snapping_constrain_horizontal (EdgeSnapping       *self,
                                    int                 change,
                                    const GdkRectangle *geometry,
                                    GdkRectangle       *window)
{
  g_assert (self != NULL);
  g_assert (geometry != NULL);
  g_assert (window != NULL);
  g_assert (change != 0);

  if (ABS (X1 (geometry) - X1 (window)) < ABS (X2 (geometry)) - ABS (X2 (window)))
    edge_snapping_constrain_left (self, change, geometry, window);
  else
    edge_snapping_constrain_right (self, change, geometry, window);
}

static void
edge_snapping_constrain_vertical (EdgeSnapping       *self,
                                  int                 change,
                                  const GdkRectangle *geometry,
                                  GdkRectangle       *window,
                                  gboolean            bottom_only)
{
  g_assert (self != NULL);
  g_assert (geometry != NULL);
  g_assert (window != NULL);
  g_assert (change != 0);

  if (!bottom_only &&
      ABS (Y1 (geometry) - Y1 (window)) < ABS (Y2 (geometry)) - ABS (Y2 (window)))
    edge_snapping_constrain_top (self, change, geometry, window);
  else
    edge_snapping_constrain_bottom (self, change, geometry, window);
}

void
_edge_snapping_motion (EdgeSnapping   *self,
                       const GdkPoint *pointer_position,
                       GdkRectangle   *window)
{
  GdkRectangle new_window;
  GdkRectangle overlap;
  GdkPoint change;

  g_assert (self != NULL);
  g_assert (pointer_position != NULL);

  change.x = pointer_position->x - self->last_pointer_position.x;
  change.y = pointer_position->y - self->last_pointer_position.y;

  self->last_pointer_position = *pointer_position;

  window->x += change.x;
  window->y += change.y;

  new_window = *window;

  /* First constrain horizontal */
  if (change.x)
    {
      edge_snapping_constrain_horizontal (self, change.x, &self->workarea, &new_window);
      if (gdk_rectangle_equal (&new_window, window))
        edge_snapping_constrain_horizontal (self, change.x, &self->geometry, &new_window);
    }

  /* Now constrain vertically */
  if (change.y)
    {
      edge_snapping_constrain_vertical (self, change.y, &self->workarea, &new_window, FALSE);
      if (new_window.y == window->y)
        edge_snapping_constrain_vertical (self, change.y, &self->geometry, &new_window, TRUE);
    }

  /* If the window is not placed in the monitor at all, then we need to
   * just move the window onto the new screen using the original offset
   * of the pointer within the window.
   */
  if (!gdk_rectangle_intersect (&self->geometry, &new_window, &overlap))
    {
      new_window.x = pointer_position->x - self->pointer_offset_in_window.x;
      new_window.y = pointer_position->y - self->pointer_offset_in_window.y;
    }

  /* And finally make sure we aren't underneath the top bar of the
   * particular monitor.
   */
  if (Y1 (&new_window) < Y1 (&self->workarea))
    new_window.y = self->workarea.y;

  *window = new_window;
}

void
_edge_snapping_set_monitor (EdgeSnapping       *self,
                            const GdkRectangle *geometry,
                            const GdkRectangle *workarea)
{
  g_assert (self != NULL);
  g_assert (geometry != NULL);
  g_assert (workarea != NULL);

  self->geometry = *geometry;
  self->workarea = *workarea;
}
