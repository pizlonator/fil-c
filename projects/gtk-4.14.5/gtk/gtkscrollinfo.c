/* GTK - The GIMP Toolkit
 * Copyright (C) 2023 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.          See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * GtkScrollInfo:
 *
 * The `GtkScrollInfo` can be used to provide more accurate data on how a scroll
 * operation should be performed.
 *
 * Scrolling functions usually allow passing a %NULL scroll info which will cause
 * the default values to be used and just scroll the element into view.
 *
 * Since: 4.12
 */

#include "config.h"

#include "gtkscrollinfoprivate.h"

#include <math.h>

struct _GtkScrollInfo
{
  guint ref_count;

  gboolean enabled[2]; /* directions */
};

static GtkScrollInfo default_scroll_info = {
  1,
  { TRUE, TRUE }
};

G_DEFINE_BOXED_TYPE (GtkScrollInfo, gtk_scroll_info,
                     gtk_scroll_info_ref,
                     gtk_scroll_info_unref)


/**
 * gtk_scroll_info_new:
 *
 * Creates a new scroll info for scrolling an element into view.
 *
 * Returns: A new scroll info
 *
 * Since: 4.12
 **/
GtkScrollInfo *
gtk_scroll_info_new (void)
{
  GtkScrollInfo *self;

  self = g_new0 (GtkScrollInfo, 1);
  self->ref_count = 1;
  self->enabled[GTK_ORIENTATION_HORIZONTAL] = TRUE;
  self->enabled[GTK_ORIENTATION_VERTICAL] = TRUE;

  return self;
}

/**
 * gtk_scroll_info_ref:
 * @self:  a `GtkScrollInfo`
 *
 * Increases the reference count of a `GtkScrollInfo` by one.
 *
 * Returns: the passed in `GtkScrollInfo`.
 *
 * Since: 4.12
 */
GtkScrollInfo *
gtk_scroll_info_ref (GtkScrollInfo *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  self->ref_count++;

  return self;
}

/**
 * gtk_scroll_info_unref:
 * @self: a `GtkScrollInfo`
 *
 * Decreases the reference count of a `GtkScrollInfo` by one.
 *
 * If the resulting reference count is zero, frees the self.
 *
 * Since: 4.12
 */
void
gtk_scroll_info_unref (GtkScrollInfo *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  self->ref_count--;
  if (self->ref_count > 0)
    return;

  g_free (self);
}

/**
 * gtk_scroll_info_set_enable_horizontal:
 * @self: a `GtkScrollInfo`
 * @horizontal: if scrolling in the horizontal direction
 *     should happen
 *
 * Turns horizontal scrolling on or off.
 *
 * Since: 4.12
 **/
void
gtk_scroll_info_set_enable_horizontal (GtkScrollInfo *self,
                                       gboolean       horizontal)
{
  g_return_if_fail (self != NULL);

  self->enabled[GTK_ORIENTATION_HORIZONTAL] = horizontal;
}

/**
 * gtk_scroll_info_get_enable_horizontal:
 * @self: a `GtkScrollInfo`
 *
 * Checks if horizontal scrolling is enabled.
 *
 * Returns: %TRUE if horizontal scrolling is enabled.
 *
 * Since: 4.12
 **/
gboolean
gtk_scroll_info_get_enable_horizontal (GtkScrollInfo *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->enabled[GTK_ORIENTATION_HORIZONTAL];
}

/**
 * gtk_scroll_info_set_enable_vertical:
 * @self: a `GtkScrollInfo`
 * @vertical: if scrolling in the vertical direction
 *     should happen
 *
 * Turns vertical scrolling on or off.
 *
 * Since: 4.12
 **/
void
gtk_scroll_info_set_enable_vertical (GtkScrollInfo *self,
                                     gboolean       vertical)
{
  g_return_if_fail (self != NULL);

  self->enabled[GTK_ORIENTATION_VERTICAL] = vertical;
}

/**
 * gtk_scroll_info_get_enable_vertical:
 * @self: a `GtkScrollInfo`
 *
 * Checks if vertical scrolling is enabled.
 *
 * Returns: %TRUE if vertical scrolling is enabled.
 *
 * Since: 4.12
 **/
gboolean
gtk_scroll_info_get_enable_vertical (GtkScrollInfo *self)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return self->enabled[GTK_ORIENTATION_VERTICAL];
}

int
gtk_scroll_info_compute_for_orientation (GtkScrollInfo  *self,
                                         GtkOrientation  orientation,
                                         int             area_origin,
                                         int             area_size,
                                         int             viewport_origin,
                                         int             viewport_size)
{
  float origin, size;
  int delta;

  if (self == NULL)
    self = &default_scroll_info;

  if (!self->enabled[orientation])
    return viewport_origin;

  origin = viewport_origin;
  size = viewport_size;

  if (area_origin <= origin)
    delta = area_origin - ceil (origin);
  else if (area_origin + area_size > origin + size)
    delta = area_origin + area_size - floor (origin + size);
  else
    delta = 0;
                                      
  return viewport_origin + delta;
}

/*<private>
 * gtk_scroll_info_compute_scroll:
 * @self: a `GtkScrollInfo`
 * @area: area to scroll
 * @viewport: viewport area to scroll into
 * @out_x: (out): x coordinate to scroll viewport to
 * @out_y: (out): y coordinate to scroll viewport to
 *
 * Computes The new x/y coordinate to move the viewport to
 * according to this scroll info.
 **/
void
gtk_scroll_info_compute_scroll (GtkScrollInfo               *self,
                                const cairo_rectangle_int_t *area,
                                const cairo_rectangle_int_t *viewport,
                                int                         *out_x,
                                int                         *out_y)
{
  *out_x = gtk_scroll_info_compute_for_orientation (self,
                                                    GTK_ORIENTATION_HORIZONTAL,
                                                    area->x, area->width,
                                                    viewport->x, viewport->width);
  *out_y = gtk_scroll_info_compute_for_orientation (self,
                                                    GTK_ORIENTATION_VERTICAL,
                                                    area->y, area->height,
                                                    viewport->y, viewport->height);
}

