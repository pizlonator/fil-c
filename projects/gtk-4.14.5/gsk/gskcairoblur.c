/* GSK - The GIMP Toolkit
 *
 * Copyright (C) 2014 Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *     Owen Taylor <otaylor@redhat.com>
 */

#include "gskcairoblurprivate.h"

#include <math.h>
#include <string.h>

/*
 * Gets the size for a single box blur.
 *
 * Much of this, the 3 * sqrt(2 * pi) / 4, is the known value for
 * approximating a Gaussian using box blurs.  This yields quite a good
 * approximation for a Gaussian.  For more details, see:
 * http://www.w3.org/TR/SVG11/filters.html#feGaussianBlurElement
 * https://bugzilla.mozilla.org/show_bug.cgi?id=590039#c19
 */
#define GAUSSIAN_SCALE_FACTOR ((3.0 * sqrt(2 * G_PI) / 4))

#define get_box_filter_size(radius) ((int)(GAUSSIAN_SCALE_FACTOR * (radius)))

/* Sadly, clang is picky about get_box_filter_size(2) not being a
 * constant expression, thus we have to use precomputed values.
 */
#define BOX_FILTER_SIZE_2 3
#define BOX_FILTER_SIZE_3 5
#define BOX_FILTER_SIZE_4 7
#define BOX_FILTER_SIZE_5 9
#define BOX_FILTER_SIZE_6 11
#define BOX_FILTER_SIZE_7 13
#define BOX_FILTER_SIZE_8 15
#define BOX_FILTER_SIZE_9 16
#define BOX_FILTER_SIZE_10 18

/* This applies a single box blur pass to a horizontal range of pixels;
 * since the box blur has the same weight for all pixels, we can
 * implement an efficient sliding window algorithm where we add
 * in pixels coming into the window from the right and remove
 * them when they leave the windw to the left.
 *
 * d is the filter width; for even d shift indicates how the blurred
 * result is aligned with the original - does ' x ' go to ' yy' (shift=1)
 * or 'yy ' (shift=-1)
 */
static void
blur_xspan (guchar *row,
            guchar *tmp_buffer,
            int     row_width,
            int     d,
            int     shift)
{
  int offset;
  int sum = 0;
  int i;

  if (d % 2 == 1)
    offset = d / 2;
  else
    offset = (d - shift) / 2;

  /* All the conditionals in here look slow, but the branches will
   * be well predicted and there are enough different possibilities
   * that trying to write this as a series of unconditional loops
   * is hard and not an obvious win. The main slow down here seems
   * to be the integer division per pixel; one possible optimization
   * would be to accumulate into two 16-bit integer buffers and
   * only divide down after all three passes. (SSE parallel implementation
   * of the divide step is possible.)
   */

#define BLUR_ROW_KERNEL(D)                                      \
  for (i = -(D) + offset; i < row_width + offset; i++)		\
    {                                                           \
      if (i >= 0 && i < row_width)                              \
        sum += row[i];                                          \
                                                                \
      if (i >= offset)						\
	{							\
	  if (i >= (D))						\
	    sum -= row[i - (D)];				\
                                                                \
	  tmp_buffer[i - offset] = (sum + (D) / 2) / (D);	\
	}							\
    }								\
  break;

  /* We unroll the values for d for radius 2-10 to avoid a generic
   * divide operation (not radius 1, because its a no-op) */
  switch (d)
    {
    case BOX_FILTER_SIZE_2: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_2);
    case BOX_FILTER_SIZE_3: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_3);
    case BOX_FILTER_SIZE_4: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_4);
    case BOX_FILTER_SIZE_5: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_5);
    case BOX_FILTER_SIZE_6: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_6);
    case BOX_FILTER_SIZE_7: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_7);
    case BOX_FILTER_SIZE_8: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_8);
    case BOX_FILTER_SIZE_9: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_9);
    case BOX_FILTER_SIZE_10: BLUR_ROW_KERNEL (BOX_FILTER_SIZE_10);
    default: BLUR_ROW_KERNEL (d);
    }

  memcpy (row, tmp_buffer, row_width);
}

static void
blur_rows (guchar *dst_buffer,
           guchar *tmp_buffer,
           int     buffer_width,
           int     buffer_height,
           int     d)
{
  int i;

  for (i = 0; i < buffer_height; i++)
    {
      guchar *row = dst_buffer + i * buffer_width;

      /* We want to produce a symmetric blur that spreads a pixel
       * equally far to the left and right. If d is odd that happens
       * naturally, but for d even, we approximate by using a blur
       * on either side and then a centered blur of size d + 1.
       * (technique also from the SVG specification)
       */
      if (d % 2 == 1)
        {
          blur_xspan (row, tmp_buffer, buffer_width, d, 0);
          blur_xspan (row, tmp_buffer, buffer_width, d, 0);
          blur_xspan (row, tmp_buffer, buffer_width, d, 0);
        }
      else
        {
          blur_xspan (row, tmp_buffer, buffer_width, d, 1);
          blur_xspan (row, tmp_buffer, buffer_width, d, -1);
          blur_xspan (row, tmp_buffer, buffer_width, d + 1, 0);
        }
    }
}

/* Swaps width and height.
 */
static void
flip_buffer (guchar *dst_buffer,
             guchar *src_buffer,
             int     width,
             int     height)
{
  /* Working in blocks increases cache efficiency, compared to reading
   * or writing an entire column at once
   */
#define BLOCK_SIZE 16

  int i0, j0;

  for (i0 = 0; i0 < width; i0 += BLOCK_SIZE)
    for (j0 = 0; j0 < height; j0 += BLOCK_SIZE)
      {
        int max_j = MIN(j0 + BLOCK_SIZE, height);
        int max_i = MIN(i0 + BLOCK_SIZE, width);
        int i, j;

        for (i = i0; i < max_i; i++)
          for (j = j0; j < max_j; j++)
            dst_buffer[i * height + j] = src_buffer[j * width + i];
      }
#undef BLOCK_SIZE
}

static void
_boxblur (guchar      *buffer,
          int          width,
          int          height,
          int          radius,
          GskBlurFlags flags)
{
  guchar *flipped_buffer;
  int d = get_box_filter_size (radius);

  flipped_buffer = g_malloc (width * height);

  if (flags & GSK_BLUR_Y)
    {
      /* Step 1: swap rows and columns */
      flip_buffer (flipped_buffer, buffer, width, height);

      /* Step 2: blur rows (really columns) */
      blur_rows (flipped_buffer, buffer, height, width, d);

      /* Step 3: swap rows and columns */
      flip_buffer (buffer, flipped_buffer, height, width);
    }

  if (flags & GSK_BLUR_X)
    {
      /* Step 4: blur rows */
      blur_rows (buffer, flipped_buffer, width, height, d);
    }

  g_free (flipped_buffer);
}

/*
 * _gsk_cairo_blur_surface:
 * @surface: a cairo image surface.
 * @radius: the blur radius.
 *
 * Blurs the cairo image surface at the given radius.
 */
void
gsk_cairo_blur_surface (cairo_surface_t* surface,
                        double           radius_d,
                        GskBlurFlags     flags)
{
  int radius = radius_d;

  g_return_if_fail (surface != NULL);
  g_return_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE);
  g_return_if_fail (cairo_image_surface_get_format (surface) == CAIRO_FORMAT_A8);

  /* The code doesn't actually do any blurring for radius 1, as it
   * ends up with box filter size 1 */
  if (radius <= 1)
    return;

  if ((flags & (GSK_BLUR_X|GSK_BLUR_Y)) == 0)
    return;

  /* Before we mess with the surface, execute any pending drawing. */
  cairo_surface_flush (surface);

  _boxblur (cairo_image_surface_get_data (surface),
            cairo_image_surface_get_stride (surface),
            cairo_image_surface_get_height (surface),
            radius, flags);

  /* Inform cairo we altered the surface contents. */
  cairo_surface_mark_dirty (surface);
}

/*<private>
 * gsk_cairo_blur_compute_pixels:
 * @radius: the radius to compute the pixels for
 *
 * Computes the number of pixels necessary to extend an image in one
 * direction to hold the image with shadow.
 *
 * This is just the number of pixels added by the blur radius, shadow
 * offset and spread are not included.
 *
 * Much of this, the 3 * sqrt(2 * pi) / 4, is the known value for
 * approximating a Gaussian using box blurs.  This yields quite a good
 * approximation for a Gaussian.  Then we multiply this by 1.5 since our
 * code wants the radius of the entire triple-box-blur kernel instead of
 * the diameter of an individual box blur.  For more details, see:
 * http://www.w3.org/TR/SVG11/filters.html#feGaussianBlurElement
 * https://bugzilla.mozilla.org/show_bug.cgi?id=590039#c19
 */
int
gsk_cairo_blur_compute_pixels (double radius)
{
  return floor (radius * GAUSSIAN_SCALE_FACTOR * 1.5 + 0.5);
}

static gboolean
needs_blur (float        radius,
            GskBlurFlags blur_flags)
{
  /* Neither blurring horizontal nor vertical means no blurring at all. */
  if ((blur_flags & (GSK_BLUR_X | GSK_BLUR_Y)) == 0)
    return FALSE;

  /* The code doesn't actually do any blurring for radius 1, as it
   * ends up with box filter size 1 */
  if (radius <= 1.0)
    return FALSE;

  return TRUE;
}

static const cairo_user_data_key_t original_cr_key;

cairo_t *
gsk_cairo_blur_start_drawing (cairo_t         *cr,
                              float            radius,
                              GskBlurFlags     blur_flags)
{
  double clip_x1, clip_x2, clip_y1, clip_y2, clip_width, clip_height;
  cairo_surface_t *surface;
  cairo_t *blur_cr;
  double clip_radius;
  double x_scale, y_scale;
  gboolean blur_x = (blur_flags & GSK_BLUR_X) != 0;
  gboolean blur_y = (blur_flags & GSK_BLUR_Y) != 0;

  if (!needs_blur (radius, blur_flags))
    return cr;

  cairo_clip_extents (cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);
  clip_width = clip_x2 - clip_x1;
  clip_height = clip_y2 - clip_y1;

  clip_radius = gsk_cairo_blur_compute_pixels (radius);

  x_scale = y_scale = 1;
  cairo_surface_get_device_scale (cairo_get_target (cr), &x_scale, &y_scale);

  if (blur_flags & GSK_BLUR_REPEAT)
    {
      if (!blur_x)
        clip_width = 1;
      if (!blur_y)
        clip_height = 1;
    }

  /* Create a larger surface to center the blur. */
  surface = cairo_surface_create_similar_image (cairo_get_target (cr),
                                                CAIRO_FORMAT_A8,
                                                x_scale * (clip_width + (blur_x ? 2 * clip_radius : 0)),
                                                y_scale * (clip_height + (blur_y ? 2 * clip_radius : 0)));
  cairo_surface_set_device_scale (surface, x_scale, y_scale);
  cairo_surface_set_device_offset (surface,
                                    x_scale * ((blur_x ? clip_radius : 0) - clip_x1),
                                    y_scale * ((blur_y ? clip_radius : 0) - clip_y1));

  blur_cr = cairo_create (surface);
  cairo_set_user_data (blur_cr, &original_cr_key, cairo_reference (cr), (cairo_destroy_func_t) cairo_destroy);

  if (cairo_has_current_point (cr))
    {
      double x, y;

      cairo_get_current_point (cr, &x, &y);
      cairo_move_to (blur_cr, x, y);
    }

  return blur_cr;
}

static void
mask_surface_repeat (cairo_t         *cr,
                     cairo_surface_t *surface)
{
  cairo_pattern_t *pattern;

  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

  cairo_mask (cr, pattern);

  cairo_pattern_destroy (pattern);
}

cairo_t *
gsk_cairo_blur_finish_drawing (cairo_t         *cr,
                               float            radius,
                               const GdkRGBA   *color,
                               GskBlurFlags     blur_flags)
{
  cairo_t *original_cr;
  cairo_surface_t *surface;
  double x_scale;

  if (!needs_blur (radius, blur_flags))
    return cr;

  original_cr = cairo_get_user_data (cr, &original_cr_key);

  /* Blur the surface. */
  surface = cairo_get_target (cr);

  x_scale = 1;
  cairo_surface_get_device_scale (cairo_get_target (cr), &x_scale, NULL);

  gsk_cairo_blur_surface (surface, x_scale * radius, blur_flags);

  gdk_cairo_set_source_rgba (original_cr, color);
  if (blur_flags & GSK_BLUR_REPEAT)
    mask_surface_repeat (original_cr, surface);
  else
    cairo_mask_surface (original_cr, surface, 0, 0);

  cairo_destroy (cr);

  cairo_surface_destroy (surface);

  return original_cr;
}

