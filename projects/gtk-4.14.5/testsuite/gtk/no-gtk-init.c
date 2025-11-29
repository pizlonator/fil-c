/* GTK - The GIMP Toolkit
 * Copyright (C) 2013 Benjamin Otte <otte@gnome.org>
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

#include <gtk/gtk.h>
#include <locale.h>

/* This test tests functions that are supposed to work without calling gtk_init()
 * or gdk_init().
 */

static void
test_gdk_cairo_set_source_pixbuf (void)
{
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkPixbuf *pixbuf;

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 5, 5);
  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, 10, 10);
  cr = cairo_create (surface);

  gdk_cairo_set_source_pixbuf (cr, pixbuf, 0, 0);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);
  g_object_unref (pixbuf);
}

int
main (int   argc,
      char *argv[])
{
  /* Keep in sync with gtk_test_init() */
  (g_test_init) (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");


  g_test_add_func ("/no_gtk_init/gdk_cairo_set_source_pixbuf", test_gdk_cairo_set_source_pixbuf);


  return g_test_run();
}
