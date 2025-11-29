/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gdkpango.h"

#include <glib/gi18n-lib.h>

#include <math.h>
#include <pango/pangocairo.h>


/* Get a clip region to draw only part of a layout. index_ranges
 * contains alternating range starts/stops. The region is the
 * region which contains the given ranges, i.e. if you draw with the
 * region as clip, only the given ranges are drawn.
 */
static cairo_region_t*
layout_iter_get_line_clip_region (PangoLayoutIter *iter,
				  int              x_origin,
				  int              y_origin,
				  const int       *index_ranges,
				  int              n_ranges)
{
  PangoLayoutLine *line;
  cairo_region_t *clip_region;
  PangoRectangle logical_rect;
  int baseline;
  int i;

  line = pango_layout_iter_get_line_readonly (iter);

  clip_region = cairo_region_create ();

  pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
  baseline = pango_layout_iter_get_baseline (iter);

  i = 0;
  while (i < n_ranges)
    {
      int *pixel_ranges = NULL;
      int n_pixel_ranges = 0;
      int j;

      /* Note that get_x_ranges returns layout coordinates
       */
      if (index_ranges[i*2+1] >= pango_layout_line_get_start_index (line) &&
	  index_ranges[i*2] < pango_layout_line_get_start_index (line) + pango_layout_line_get_length (line))
	pango_layout_line_get_x_ranges (line,
					index_ranges[i*2],
					index_ranges[i*2+1],
					&pixel_ranges, &n_pixel_ranges);

      for (j = 0; j < n_pixel_ranges; j++)
        {
          GdkRectangle rect;
	  int x_off, y_off;

          x_off = PANGO_PIXELS (pixel_ranges[2*j] - logical_rect.x);
	  y_off = PANGO_PIXELS (baseline - logical_rect.y);

          rect.x = x_origin + x_off;
          rect.y = y_origin - y_off;
          rect.width = PANGO_PIXELS (pixel_ranges[2*j + 1] - logical_rect.x) - x_off;
          rect.height = PANGO_PIXELS (baseline - logical_rect.y + logical_rect.height) - y_off;

          cairo_region_union_rectangle (clip_region, &rect);
        }

      g_free (pixel_ranges);
      ++i;
    }
  return clip_region;
}

/**
 * gdk_pango_layout_line_get_clip_region: (skip)
 * @line: a `PangoLayoutLine`
 * @x_origin: X pixel where you intend to draw the layout line with this clip
 * @y_origin: baseline pixel where you intend to draw the layout line with this clip
 * @index_ranges: (array): array of byte indexes into the layout, where even
 *   members of array are start indexes and odd elements are end indexes
 * @n_ranges: number of ranges in @index_ranges, i.e. half the size of @index_ranges
 *
 * Obtains a clip region which contains the areas where the given
 * ranges of text would be drawn.
 *
 * @x_origin and @y_origin are the top left position of the layout.
 * @index_ranges should contain ranges of bytes in the layout’s text.
 * The clip region will include space to the left or right of the line
 * (to the layout bounding box) if you have indexes above or below the
 * indexes contained inside the line. This is to draw the selection all
 * the way to the side of the layout. However, the clip region is in line
 * coordinates, not layout coordinates.
 *
 * Note that the regions returned correspond to logical extents of the text
 * ranges, not ink extents. So the drawn line may in fact touch areas out of
 * the clip region.  The clip region is mainly useful for highlightling parts
 * of text, such as when text is selected.
 *
 * Returns: a clip region containing the given ranges
 */
cairo_region_t*
gdk_pango_layout_line_get_clip_region (PangoLayoutLine *line,
                                       int              x_origin,
                                       int              y_origin,
                                       const int       *index_ranges,
                                       int              n_ranges)
{
  cairo_region_t *clip_region;
  PangoLayoutIter *iter;

  g_return_val_if_fail (line != NULL, NULL);
  g_return_val_if_fail (index_ranges != NULL, NULL);

  iter = pango_layout_get_iter (line->layout);
  while (pango_layout_iter_get_line_readonly (iter) != line)
    pango_layout_iter_next_line (iter);

  clip_region = layout_iter_get_line_clip_region(iter, x_origin, y_origin, index_ranges, n_ranges);

  pango_layout_iter_free (iter);

  return clip_region;
}

/**
 * gdk_pango_layout_get_clip_region: (skip)
 * @layout: a `PangoLayout`
 * @x_origin: X pixel where you intend to draw the layout with this clip
 * @y_origin: Y pixel where you intend to draw the layout with this clip
 * @index_ranges: array of byte indexes into the layout, where even members of array are start indexes and odd elements are end indexes
 * @n_ranges: number of ranges in @index_ranges, i.e. half the size of @index_ranges
 *
 * Obtains a clip region which contains the areas where the given ranges
 * of text would be drawn.
 *
 * @x_origin and @y_origin are the top left point to center the layout.
 * @index_ranges should contain ranges of bytes in the layout’s text.
 *
 * Note that the regions returned correspond to logical extents of the text
 * ranges, not ink extents. So the drawn layout may in fact touch areas out of
 * the clip region.  The clip region is mainly useful for highlightling parts
 * of text, such as when text is selected.
 *
 * Returns: a clip region containing the given ranges
 */
cairo_region_t*
gdk_pango_layout_get_clip_region (PangoLayout *layout,
                                  int          x_origin,
                                  int          y_origin,
                                  const int   *index_ranges,
                                  int          n_ranges)
{
  PangoLayoutIter *iter;
  cairo_region_t *clip_region;

  g_return_val_if_fail (PANGO_IS_LAYOUT (layout), NULL);
  g_return_val_if_fail (index_ranges != NULL, NULL);

  clip_region = cairo_region_create ();

  iter = pango_layout_get_iter (layout);

  do
    {
      PangoRectangle logical_rect;
      cairo_region_t *line_region;
      int baseline;

      pango_layout_iter_get_line_extents (iter, NULL, &logical_rect);
      baseline = pango_layout_iter_get_baseline (iter);

      line_region = layout_iter_get_line_clip_region(iter,
						     x_origin + PANGO_PIXELS (logical_rect.x),
						     y_origin + PANGO_PIXELS (baseline),
						     index_ranges,
						     n_ranges);

      cairo_region_union (clip_region, line_region);
      cairo_region_destroy (line_region);
    }
  while (pango_layout_iter_next_line (iter));

  pango_layout_iter_free (iter);

  return clip_region;
}
