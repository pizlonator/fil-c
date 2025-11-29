/* gtkquartz.c: Utility functions used by the Quartz port
 *
 * Copyright (C) 2006 Imendio AB
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

#include "gtkquartz.h"

#include <gdk/macos/gdkmacos.h>

static gboolean
_cairo_surface_extents (cairo_surface_t *surface,
                        GdkRectangle    *extents)
{
  double x1, x2, y1, y2;
  cairo_t *cr;

  g_return_val_if_fail (surface != NULL, FALSE);
  g_return_val_if_fail (extents != NULL, FALSE);

  cr = cairo_create (surface);
  cairo_clip_extents (cr, &x1, &y1, &x2, &y2);

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

static void
_data_provider_release_cairo_surface (void       *info,
                                      const void *data,
                                      size_t      size)
{
  cairo_surface_destroy ((cairo_surface_t *)info);
}

/* Returns a new NSImage or %NULL in case of an error.
 * The device scale factor will be transferred to the NSImage (hidpi)
 */
NSImage *
_gtk_quartz_create_image_from_surface (cairo_surface_t *surface)
{
  CGColorSpaceRef colorspace;
  CGDataProviderRef data_provider;
  CGImageRef image;
  void *data;
  NSImage *nsimage;
  double sx, sy;
  cairo_t *cr;
  cairo_surface_t *img_surface;
  cairo_rectangle_int_t extents;
  int width, height, rowstride;

  if (!_cairo_surface_extents (surface, &extents))
    return NULL;

  cairo_surface_get_device_scale (surface, &sx, &sy);
  width = extents.width * sx;
  height = extents.height * sy;

  img_surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);
  cr = cairo_create (img_surface);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_scale (cr, sx, sy);
  cairo_set_source_surface (cr, surface, -extents.x, -extents.y);
  cairo_paint (cr);
  cairo_destroy (cr);

  cairo_surface_flush (img_surface);
  rowstride = cairo_image_surface_get_stride (img_surface);
  data = cairo_image_surface_get_data (img_surface);

  colorspace = CGColorSpaceCreateDeviceRGB ();
  /* Note: the release callback will only be called after NSImage below dies */
  data_provider = CGDataProviderCreateWithData (surface, data, height * rowstride,
                                                _data_provider_release_cairo_surface);

  image = CGImageCreate (width, height, 8,
                         32, rowstride,
                         colorspace,
                         /* XXX: kCGBitmapByteOrderDefault gives wrong colors..?? */
                         kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst,
                         data_provider, NULL, FALSE,
                         kCGRenderingIntentDefault);
  CGDataProviderRelease (data_provider);
  CGColorSpaceRelease (colorspace);

  nsimage = [[NSImage alloc] initWithCGImage:image size:NSMakeSize (extents.width, extents.height)];
  CGImageRelease (image);

  return nsimage;
}

#ifdef QUARTZ_RELOCATION

/* Bundle-based functions for various directories. These almost work
 * even when the application isn’t in a bundle, because mainBundle
 * paths point to the bin directory in that case. It’s a simple matter
 * to test for that and remove the last element.
 */

static const char *
get_bundle_path (void)
{
  static char *path = NULL;

  if (path == NULL)
    {
      NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
      char *resource_path = g_strdup ([[[NSBundle mainBundle] resourcePath] UTF8String]);
      char *base;
      [pool drain];

      base = g_path_get_basename (resource_path);
      if (strcmp (base, "bin") == 0)
	path = g_path_get_dirname (resource_path);
      else
	path = strdup (resource_path);

      g_free (resource_path);
      g_free (base);
    }

  return path;
}

const char *
_gtk_get_datadir (void)
{
  static char *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "share", NULL);

  return path;
}

const char *
_gtk_get_libdir (void)
{
  static char *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "lib", NULL);

  return path;
}

const char *
_gtk_get_localedir (void)
{
  static char *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "share", "locale", NULL);

  return path;
}

const char *
_gtk_get_sysconfdir (void)
{
  static char *path = NULL;

  if (path == NULL)
    path = g_build_filename (get_bundle_path (), "etc", NULL);

  return path;
}

const char *
_gtk_get_data_prefix (void)
{
  return get_bundle_path ();
}

#endif /* QUARTZ_RELOCATION */
