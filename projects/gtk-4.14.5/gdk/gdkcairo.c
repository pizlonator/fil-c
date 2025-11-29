/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2005 Red Hat, Inc.
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

#include "config.h"

#include "gdkcairo.h"

#include "gdkrgba.h"
#include "gdktexture.h"

#include <math.h>

/**
 * gdk_cairo_set_source_rgba:
 * @cr: a cairo context
 * @rgba: a `GdkRGBA`
 *
 * Sets the specified `GdkRGBA` as the source color of @cr.
 */
void
gdk_cairo_set_source_rgba (cairo_t       *cr,
                           const GdkRGBA *rgba)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (rgba != NULL);

  cairo_set_source_rgba (cr,
                         rgba->red,
                         rgba->green,
                         rgba->blue,
                         rgba->alpha);
}

/**
 * gdk_cairo_rectangle:
 * @cr: a cairo context
 * @rectangle: a `GdkRectangle`
 *
 * Adds the given rectangle to the current path of @cr.
 */
void
gdk_cairo_rectangle (cairo_t            *cr,
                     const GdkRectangle *rectangle)
{
  g_return_if_fail (cr != NULL);
  g_return_if_fail (rectangle != NULL);

  cairo_rectangle (cr,
                   rectangle->x,     rectangle->y,
                   rectangle->width, rectangle->height);
}

/**
 * gdk_cairo_region:
 * @cr: a cairo context
 * @region: a `cairo_region_t`
 *
 * Adds the given region to the current path of @cr.
 */
void
gdk_cairo_region (cairo_t              *cr,
                  const cairo_region_t *region)
{
  cairo_rectangle_int_t box;
  int n_boxes, i;

  g_return_if_fail (cr != NULL);
  g_return_if_fail (region != NULL);

  n_boxes = cairo_region_num_rectangles (region);

  for (i = 0; i < n_boxes; i++)
    {
      cairo_region_get_rectangle (region, i, &box);
      cairo_rectangle (cr, box.x, box.y, box.width, box.height);
    }
}

static void
gdk_cairo_surface_paint_pixbuf (cairo_surface_t *surface,
                                const GdkPixbuf *pixbuf)
{
  GdkTexture *texture;

  if (cairo_surface_status (surface) != CAIRO_STATUS_SUCCESS)
    return;

  /* This function can't just copy any pixbuf to any surface, be
   * sure to read the invariants here before calling it */

  g_assert (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE);
  g_assert (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_RGB24 ||
            cairo_image_surface_get_format (surface) == CAIRO_FORMAT_ARGB32);
  g_assert (cairo_image_surface_get_width (surface) == gdk_pixbuf_get_width (pixbuf));
  g_assert (cairo_image_surface_get_height (surface) == gdk_pixbuf_get_height (pixbuf));

  cairo_surface_flush (surface);

  texture = gdk_texture_new_for_pixbuf (GDK_PIXBUF (pixbuf));
  gdk_texture_download (texture,
                        cairo_image_surface_get_data (surface),
                        cairo_image_surface_get_stride (surface));
  g_object_unref (texture);

  cairo_surface_mark_dirty (surface);
}

/**
 * gdk_cairo_set_source_pixbuf:
 * @cr: a cairo context
 * @pixbuf: a `GdkPixbuf`
 * @pixbuf_x: X coordinate of location to place upper left corner of @pixbuf
 * @pixbuf_y: Y coordinate of location to place upper left corner of @pixbuf
 *
 * Sets the given pixbuf as the source pattern for @cr.
 *
 * The pattern has an extend mode of %CAIRO_EXTEND_NONE and is aligned
 * so that the origin of @pixbuf is @pixbuf_x, @pixbuf_y.
 */
void
gdk_cairo_set_source_pixbuf (cairo_t         *cr,
                             const GdkPixbuf *pixbuf,
                             double           pixbuf_x,
                             double           pixbuf_y)
{
  cairo_format_t format;
  cairo_surface_t *surface;

  if (gdk_pixbuf_get_n_channels (pixbuf) == 3)
    format = CAIRO_FORMAT_RGB24;
  else
    format = CAIRO_FORMAT_ARGB32;

  surface = cairo_surface_create_similar_image (cairo_get_target (cr),
                                                format,
                                                gdk_pixbuf_get_width (pixbuf),
                                                gdk_pixbuf_get_height (pixbuf));

  gdk_cairo_surface_paint_pixbuf (surface, pixbuf);

  cairo_set_source_surface (cr, surface, pixbuf_x, pixbuf_y);
  cairo_surface_destroy (surface);
}

/*
 * _gdk_cairo_surface_extents:
 * @surface: surface to measure
 * @extents: (out): rectangle to put the extents
 *
 * Measures the area covered by @surface and puts it into @extents.
 *
 * Note that this function respects device offsets set on @surface.
 * If @surface is unbounded, the resulting extents will be empty and
 * not be a maximal sized rectangle. This is to avoid careless coding.
 * You must explicitly check the return value of you want to handle
 * that case.
 *
 * Returns: %TRUE if the extents fit in a `GdkRectangle`, %FALSE if not
 */
static gboolean
_gdk_cairo_surface_extents (cairo_surface_t *surface,
                            GdkRectangle    *extents)
{
  double x1, x2, y1, y2;
  cairo_t *cr;

  g_return_val_if_fail (surface != NULL, FALSE);
  g_return_val_if_fail (extents != NULL, FALSE);

  cr = cairo_create (surface);
  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);
  cairo_destroy (cr);

  x1 = floor (x1);
  y1 = floor (y1);
  x2 = ceil (x2);
  y2 = ceil (y2);
  x2 -= x1;
  y2 -= y1;

  if (x1 < G_MININT || x1 > G_MAXINT ||
      y1 < G_MININT || y1 > G_MAXINT ||
      x2 > G_MAXINT || y2 > G_MAXINT)
    {
      extents->x = extents->y = extents->width = extents->height = 0;
      return FALSE;
    }

  extents->x = x1;
  extents->y = y1;
  extents->width = x2;
  extents->height = y2;

  return TRUE;
}

/* This function originally from Jean-Edouard Lachand-Robert, and
 * available at www.codeguru.com. Simplified for our needs, not sure
 * how much of the original code left any longer. Now handles just
 * one-bit deep bitmaps (in Window parlance, ie those that GDK calls
 * bitmaps (and not pixmaps), with zero pixels being transparent.
 */
/**
 * gdk_cairo_region_create_from_surface:
 * @surface: a cairo surface
 *
 * Creates region that covers the area where the given
 * @surface is more than 50% opaque.
 *
 * This function takes into account device offsets that might be
 * set with cairo_surface_set_device_offset().
 *
 * Returns: (transfer full): A `cairo_region_t`
 */
cairo_region_t *
gdk_cairo_region_create_from_surface (cairo_surface_t *surface)
{
  cairo_region_t *region;
  GdkRectangle extents, rect;
  cairo_surface_t *image;
  cairo_t *cr;
  int x, y, stride;
  guchar *data;

  _gdk_cairo_surface_extents (surface, &extents);

  if (cairo_surface_get_content (surface) == CAIRO_CONTENT_COLOR)
    return cairo_region_create_rectangle (&extents);

  if (cairo_surface_get_type (surface) != CAIRO_SURFACE_TYPE_IMAGE ||
      cairo_image_surface_get_format (surface) != CAIRO_FORMAT_A1)
    {
      /* coerce to an A1 image */
      image = cairo_image_surface_create (CAIRO_FORMAT_A1,
                                          extents.width, extents.height);
      cr = cairo_create (image);
      cairo_set_source_surface (cr, surface, -extents.x, -extents.y);
      cairo_paint (cr);
      cairo_destroy (cr);
    }
  else
    image = cairo_surface_reference (surface);

  /* Flush the surface to make sure that the rendering is up to date. */
  cairo_surface_flush (image);

  data = cairo_image_surface_get_data (image);
  stride = cairo_image_surface_get_stride (image);

  region = cairo_region_create ();

  for (y = 0; y < extents.height; y++)
    {
      for (x = 0; x < extents.width; x++)
        {
          /* Search for a continuous range of "non transparent pixels"*/
          int x0 = x;
          while (x < extents.width)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
              if (((data[x / 8] >> (x%8)) & 1) == 0)
#else
              if (((data[x / 8] >> (7-(x%8))) & 1) == 0)
#endif
                /* This pixel is "transparent"*/
                break;
              x++;
            }

          if (x > x0)
            {
              /* Add the pixels (x0, y) to (x, y+1) as a new rectangle
               * in the region
               */
              rect.x = x0;
              rect.width = x - x0;
              rect.y = y;
              rect.height = 1;

              cairo_region_union_rectangle (region, &rect);
            }
        }
      data += stride;
    }

  cairo_surface_destroy (image);

  cairo_region_translate (region, extents.x, extents.y);

  return region;
}
