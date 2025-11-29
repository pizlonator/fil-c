/*
 * Copyright © 2018 Red Hat Inc.
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
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcssimagepaintableprivate.h"

#include "gtkprivate.h"

G_DEFINE_TYPE (GtkCssImagePaintable, gtk_css_image_paintable, GTK_TYPE_CSS_IMAGE)

static inline GdkPaintable *
get_paintable (GtkCssImagePaintable *paintable)
{
  if (paintable->static_paintable)
    return paintable->static_paintable;

  return paintable->paintable;
}

static int
gtk_css_image_paintable_get_width (GtkCssImage *image)
{
  GtkCssImagePaintable *paintable = GTK_CSS_IMAGE_PAINTABLE (image);

  return gdk_paintable_get_intrinsic_width (get_paintable (paintable));
}

static int
gtk_css_image_paintable_get_height (GtkCssImage *image)
{
  GtkCssImagePaintable *paintable = GTK_CSS_IMAGE_PAINTABLE (image);

  return gdk_paintable_get_intrinsic_height (get_paintable (paintable));
}

static double
gtk_css_image_paintable_get_aspect_ratio (GtkCssImage *image)
{
  GtkCssImagePaintable *paintable = GTK_CSS_IMAGE_PAINTABLE (image);

  return gdk_paintable_get_intrinsic_aspect_ratio (get_paintable (paintable));
}

static void
gtk_css_image_paintable_snapshot (GtkCssImage *image,
                                  GtkSnapshot *snapshot,
                                  double       width,
                                  double       height)
{
  GtkCssImagePaintable *paintable = GTK_CSS_IMAGE_PAINTABLE (image);

  gdk_paintable_snapshot (get_paintable (paintable),
                          snapshot,
                          width, height);
}

static GtkCssImage *
gtk_css_image_paintable_get_static_image (GtkCssImage *image)
{
  GtkCssImagePaintable *paintable = GTK_CSS_IMAGE_PAINTABLE (image);
  GdkPaintable *static_image;
  GtkCssImage *result;

  static_image = gdk_paintable_get_current_image (paintable->paintable);

  if (paintable->static_paintable == static_image)
    {
      result = g_object_ref (image);
    }
  else
    {
      result = gtk_css_image_paintable_new (paintable->paintable,
                                            static_image);
    }

  g_object_unref (static_image);

  return result;
}

static GtkCssImage *
gtk_css_image_paintable_compute (GtkCssImage      *image,
                                 guint             property_id,
                                 GtkStyleProvider *provider,
                                 GtkCssStyle      *style,
                                 GtkCssStyle      *parent_style)
{
  return gtk_css_image_paintable_get_static_image (image);
}

static gboolean
gtk_css_image_paintable_equal (GtkCssImage *image1,
                               GtkCssImage *image2)
{
  GtkCssImagePaintable *paintable1 = GTK_CSS_IMAGE_PAINTABLE (image1);
  GtkCssImagePaintable *paintable2 = GTK_CSS_IMAGE_PAINTABLE (image2);

  return paintable1->paintable == paintable2->paintable
      && paintable1->static_paintable == paintable2->static_paintable;
}

#define GDK_PAINTABLE_IMMUTABLE (GDK_PAINTABLE_STATIC_SIZE | GDK_PAINTABLE_STATIC_CONTENTS)
static gboolean
gtk_css_image_paintable_is_dynamic (GtkCssImage *image)
{
  GtkCssImagePaintable *paintable = GTK_CSS_IMAGE_PAINTABLE (image);

  return (gdk_paintable_get_flags (paintable->paintable) & GDK_PAINTABLE_IMMUTABLE) == GDK_PAINTABLE_IMMUTABLE;
}

static GtkCssImage *
gtk_css_image_paintable_get_dynamic_image (GtkCssImage *image,
                                           gint64       monotonic_time)
{
  return gtk_css_image_paintable_get_static_image (image);
}

static void
gtk_css_image_paintable_print (GtkCssImage *image,
                             GString     *string)
{
  g_string_append (string, "none /* FIXME */");
}

static void
gtk_css_image_paintable_dispose (GObject *object)
{
  GtkCssImagePaintable *paintable = GTK_CSS_IMAGE_PAINTABLE (object);

  g_clear_object (&paintable->paintable);
  g_clear_object (&paintable->static_paintable);

  G_OBJECT_CLASS (gtk_css_image_paintable_parent_class)->dispose (object);
}

static gboolean
gtk_css_image_paintable_is_computed (GtkCssImage *image)
{
  GtkCssImagePaintable *self = GTK_CSS_IMAGE_PAINTABLE (image);

  return (gdk_paintable_get_flags (self->paintable) & GDK_PAINTABLE_IMMUTABLE) == GDK_PAINTABLE_IMMUTABLE;
}

static void
gtk_css_image_paintable_class_init (GtkCssImagePaintableClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  image_class->get_width = gtk_css_image_paintable_get_width;
  image_class->get_height = gtk_css_image_paintable_get_height;
  image_class->get_aspect_ratio = gtk_css_image_paintable_get_aspect_ratio;
  image_class->snapshot = gtk_css_image_paintable_snapshot;
  image_class->print = gtk_css_image_paintable_print;
  image_class->compute = gtk_css_image_paintable_compute;
  image_class->equal = gtk_css_image_paintable_equal;
  image_class->is_dynamic = gtk_css_image_paintable_is_dynamic;
  image_class->is_computed = gtk_css_image_paintable_is_computed;
  image_class->get_dynamic_image = gtk_css_image_paintable_get_dynamic_image;

  object_class->dispose = gtk_css_image_paintable_dispose;
}

static void
gtk_css_image_paintable_init (GtkCssImagePaintable *image_paintable)
{
}

GtkCssImage *
gtk_css_image_paintable_new (GdkPaintable *paintable,
                             GdkPaintable *static_paintable)
{
  GtkCssImage *image;

  gtk_internal_return_val_if_fail (GDK_IS_PAINTABLE (paintable), NULL);
  gtk_internal_return_val_if_fail (static_paintable == NULL || GDK_IS_PAINTABLE (static_paintable), NULL);

  image = g_object_new (GTK_TYPE_CSS_IMAGE_PAINTABLE, NULL);
  
  GTK_CSS_IMAGE_PAINTABLE (image)->paintable = g_object_ref (paintable);
  if (static_paintable)
    GTK_CSS_IMAGE_PAINTABLE (image)->static_paintable = g_object_ref (static_paintable);

  return image;
}
