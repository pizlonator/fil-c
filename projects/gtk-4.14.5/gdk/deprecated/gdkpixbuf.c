/* GdkPixbuf library - convert X drawable information to RGB
 *
 * Copyright (C) 1999 Michael Zucchi
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *          Cody Russell <bratsche@dfw.net>
 * 	    Federico Mena-Quintero <federico@gimp.org>
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

#include "gdkpixbuf.h"

#include "gdkmemoryformatprivate.h"
#include "gdkmemorytextureprivate.h"
#include "gdksurface.h"
#include "gdktexturedownloaderprivate.h"

#include <gdk-pixbuf/gdk-pixbuf.h>


static cairo_format_t
gdk_cairo_format_for_content (cairo_content_t content)
{
  switch (content)
    {
    case CAIRO_CONTENT_COLOR:
      return CAIRO_FORMAT_RGB24;
    case CAIRO_CONTENT_ALPHA:
      return CAIRO_FORMAT_A8;
    case CAIRO_CONTENT_COLOR_ALPHA:
    default:
      return CAIRO_FORMAT_ARGB32;
    }
}

static cairo_surface_t *
gdk_cairo_surface_coerce_to_image (cairo_surface_t *surface,
                                   cairo_content_t  content,
                                   int              src_x,
                                   int              src_y,
                                   int              width,
                                   int              height)
{
  cairo_surface_t *copy;
  cairo_t *cr;

  copy = cairo_image_surface_create (gdk_cairo_format_for_content (content),
                                     width,
                                     height);

  cr = cairo_create (copy);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_surface (cr, surface, -src_x, -src_y);
  cairo_paint (cr);
  cairo_destroy (cr);

  return copy;
}

static void
convert_alpha (guchar *dest_data,
               int     dest_stride,
               guchar *src_data,
               int     src_stride,
               int     src_x,
               int     src_y,
               int     width,
               int     height)
{
  int x, y;

  src_data += src_stride * src_y + src_x * 4;

  for (y = 0; y < height; y++) {
    guint32 *src = (guint32 *) src_data;

    for (x = 0; x < width; x++) {
      guint alpha = src[x] >> 24;

      if (alpha == 0)
        {
          dest_data[x * 4 + 0] = 0;
          dest_data[x * 4 + 1] = 0;
          dest_data[x * 4 + 2] = 0;
        }
      else
        {
          dest_data[x * 4 + 0] = (((src[x] & 0xff0000) >> 16) * 255 + alpha / 2) / alpha;
          dest_data[x * 4 + 1] = (((src[x] & 0x00ff00) >>  8) * 255 + alpha / 2) / alpha;
          dest_data[x * 4 + 2] = (((src[x] & 0x0000ff) >>  0) * 255 + alpha / 2) / alpha;
        }
      dest_data[x * 4 + 3] = alpha;
    }

    src_data += src_stride;
    dest_data += dest_stride;
  }
}

static void
convert_no_alpha (guchar *dest_data,
                  int     dest_stride,
                  guchar *src_data,
                  int     src_stride,
                  int     src_x,
                  int     src_y,
                  int     width,
                  int     height)
{
  int x, y;

  src_data += src_stride * src_y + src_x * 4;

  for (y = 0; y < height; y++) {
    guint32 *src = (guint32 *) src_data;

    for (x = 0; x < width; x++) {
      dest_data[x * 3 + 0] = src[x] >> 16;
      dest_data[x * 3 + 1] = src[x] >>  8;
      dest_data[x * 3 + 2] = src[x];
    }

    src_data += src_stride;
    dest_data += dest_stride;
  }
}

/**
 * gdk_pixbuf_get_from_surface:
 * @surface: surface to copy from
 * @src_x: Source X coordinate within @surface
 * @src_y: Source Y coordinate within @surface
 * @width: Width in pixels of region to get
 * @height: Height in pixels of region to get
 *
 * Transfers image data from a `cairo_surface_t` and converts it
 * to a `GdkPixbuf`.
 *
 * This allows you to efficiently read individual pixels from cairo surfaces.
 *
 * This function will create an RGB pixbuf with 8 bits per channel.
 * The pixbuf will contain an alpha channel if the @surface contains one.
 *
 * Returns: (nullable) (transfer full): A newly-created pixbuf with a
 *   reference count of 1
 *
 * Deprecated: 4.12: Use [class@Gdk.Texture] and subclasses instead
 *   cairo surfaces and pixbufs
 */
GdkPixbuf *
gdk_pixbuf_get_from_surface (cairo_surface_t *surface,
                             int              src_x,
                             int              src_y,
                             int              width,
                             int              height)
{
  cairo_content_t content;
  GdkPixbuf *dest;

  /* General sanity checks */
  g_return_val_if_fail (surface != NULL, NULL);
  g_return_val_if_fail (width > 0 && height > 0, NULL);

  content = cairo_surface_get_content (surface) | CAIRO_CONTENT_COLOR;
  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                         !!(content & CAIRO_CONTENT_ALPHA),
                         8,
                         width, height);

  if (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE &&
      cairo_image_surface_get_format (surface) == gdk_cairo_format_for_content (content))
    surface = cairo_surface_reference (surface);
  else
    {
      surface = gdk_cairo_surface_coerce_to_image (surface, content,
						   src_x, src_y,
						   width, height);
      src_x = 0;
      src_y = 0;
    }
  cairo_surface_flush (surface);
  if (cairo_surface_status (surface) || dest == NULL)
    {
      cairo_surface_destroy (surface);
      g_clear_object (&dest);
      return NULL;
    }

  if (gdk_pixbuf_get_has_alpha (dest))
    convert_alpha (gdk_pixbuf_get_pixels (dest),
                   gdk_pixbuf_get_rowstride (dest),
                   cairo_image_surface_get_data (surface),
                   cairo_image_surface_get_stride (surface),
                   src_x, src_y,
                   width, height);
  else
    convert_no_alpha (gdk_pixbuf_get_pixels (dest),
                      gdk_pixbuf_get_rowstride (dest),
                      cairo_image_surface_get_data (surface),
                      cairo_image_surface_get_stride (surface),
                      src_x, src_y,
                      width, height);

  cairo_surface_destroy (surface);
  return dest;
}

static void
pixbuf_texture_unref_cb (guchar   *pixels,
                         gpointer  bytes)
{
  g_bytes_unref (bytes);
}

/**
 * gdk_pixbuf_get_from_texture:
 * @texture: a `GdkTexture`
 *
 * Creates a new pixbuf from @texture.
 *
 * This should generally not be used in newly written code as later
 * stages will almost certainly convert the pixbuf back into a texture
 * to draw it on screen.
 *
 * Returns: (transfer full) (nullable): a new `GdkPixbuf`
 *
 * Deprecated: 4.12: Use [class@Gdk.Texture] and subclasses instead
 *   cairo surfaces and pixbufs
 */
GdkPixbuf *
gdk_pixbuf_get_from_texture (GdkTexture *texture)
{
  GdkTextureDownloader downloader;
  GBytes *bytes;
  gsize stride;
  gboolean alpha;

  alpha = gdk_memory_format_alpha (gdk_texture_get_format (texture)) != GDK_MEMORY_ALPHA_OPAQUE;

  gdk_texture_downloader_init (&downloader, texture);
  gdk_texture_downloader_set_format (&downloader, 
                                     alpha ? GDK_MEMORY_GDK_PIXBUF_ALPHA
                                           : GDK_MEMORY_GDK_PIXBUF_OPAQUE);
  bytes = gdk_texture_downloader_download_bytes (&downloader, &stride);
  gdk_texture_downloader_finish (&downloader);

  return gdk_pixbuf_new_from_data (g_bytes_get_data (bytes, NULL),
                                   GDK_COLORSPACE_RGB,
                                   alpha,
                                   8,
                                   gdk_texture_get_width (texture),
                                   gdk_texture_get_height (texture),
                                   stride,
                                   pixbuf_texture_unref_cb,
                                   bytes);
}
