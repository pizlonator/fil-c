/*
 * Copyright (C) 2011,2021 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
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
 */

#include "config.h"

#include "reftest-compare.h"

/* Compares two GDK_MEMORY_DEFAULT buffers, returning NULL if the
 * buffers are equal or a surface containing a diff between the two
 * surfaces.
 *
 * This function is originally from cairo:test/buffer-diff.c.
 * Copyright © 2004 Richard D. Worth
 */
static GdkTexture *
buffer_diff_core (const guchar *buf_a,
                  int           stride_a,
        	  const guchar *buf_b,
                  int           stride_b,
        	  int		width,
        	  int		height)
{
  int x, y;
  guchar *buf_diff = NULL;
  int stride_diff = 0;
  GdkTexture *diff = NULL;

  for (y = 0; y < height; y++)
    {
      const guint32 *row_a = (const guint32 *) (buf_a + y * stride_a);
      const guint32 *row_b = (const guint32 *) (buf_b + y * stride_b);
      guint32 *row = (guint32 *) (buf_diff + y * stride_diff);

      for (x = 0; x < width; x++)
        {
          int channel;
          guint32 diff_pixel = 0;

          /* check if the pixels are the same */
          if (row_a[x] == row_b[x])
            continue;

          /* even if they're not literally the same, fully-transparent
           * pixels are effectively the same regardless of colour */
          if ((row_a[x] & 0xff000000) == 0 && (row_b[x] & 0xff000000) == 0)
            continue;

          if (diff == NULL)
            {
              GBytes *bytes;

              stride_diff = 4 * width;
              buf_diff = g_malloc0_n (stride_diff, height);
              bytes = g_bytes_new_take (buf_diff, stride_diff * height);
              diff = gdk_memory_texture_new (width, height,
                                             GDK_MEMORY_DEFAULT,
                                             bytes,
                                             stride_diff);
              row = (guint32 *) (buf_diff + y * stride_diff);
            }

          /* calculate a difference value for all 4 channels */
          for (channel = 0; channel < 4; channel++)
            {
              int value_a = (row_a[x] >> (channel*8)) & 0xff;
              int value_b = (row_b[x] >> (channel*8)) & 0xff;
              guint channel_diff;

              channel_diff = ABS (value_a - value_b);
              channel_diff *= 4;  /* emphasize */
              if (channel_diff)
                channel_diff += 128; /* make sure it's visible */
              if (channel_diff > 255)
                channel_diff = 255;
              diff_pixel |= channel_diff << (channel * 8);
            }

          if ((diff_pixel & 0x00ffffff) == 0)
            {
              /* alpha only difference, convert to luminance */
              guint8 alpha = diff_pixel >> 24;
              diff_pixel = alpha * 0x010101;
            }
          /* make the pixel fully opaque */
          diff_pixel |= 0xff000000;
          
          row[x] = diff_pixel;
      }
  }

  return diff;
}

GdkTexture *
reftest_compare_textures (GdkTexture *texture1,
                          GdkTexture *texture2)
{
  int w, h;
  guchar *data1, *data2;
  GdkTexture *diff;
  
  w = MAX (gdk_texture_get_width (texture1), gdk_texture_get_width (texture2));
  h = MAX (gdk_texture_get_height (texture1), gdk_texture_get_height (texture2));

  data1 = g_malloc_n (w * 4, h);
  gdk_texture_download (texture1, data1, w * 4);
  data2 = g_malloc_n (w * 4, h);
  gdk_texture_download (texture2, data2, w * 4);

  diff = buffer_diff_core (data1, w * 4,
                           data2, w * 4,
                           w, h);

  g_free (data1);
  g_free (data2);

  return diff;
}
