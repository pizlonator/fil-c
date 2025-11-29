/*
 * Copyright © 2011 Red Hat Inc.
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

#include "gtkcssimageinvalidprivate.h"

G_DEFINE_TYPE (GtkCssImageInvalid, gtk_css_image_invalid, GTK_TYPE_CSS_IMAGE)

static void
gtk_css_image_invalid_snapshot (GtkCssImage *image,
                                GtkSnapshot *snapshot,
                                double       width,
                                double       height)
{
}

static gboolean
gtk_css_image_invalid_equal (GtkCssImage *image1,
                             GtkCssImage *image2)
{
  return TRUE;
}

static void
gtk_css_image_invalid_print (GtkCssImage *image,
                             GString     *string)
{
  g_string_append (string, "none /* invalid image */");
}

static gboolean
gtk_css_image_invalid_is_invalid (GtkCssImage *image)
{
  return TRUE;
}

static void
gtk_css_image_invalid_class_init (GtkCssImageInvalidClass *klass)
{
  GtkCssImageClass *image_class = GTK_CSS_IMAGE_CLASS (klass);

  image_class->snapshot = gtk_css_image_invalid_snapshot;
  image_class->print = gtk_css_image_invalid_print;
  image_class->equal = gtk_css_image_invalid_equal;
  image_class->is_invalid = gtk_css_image_invalid_is_invalid;
}

static void
gtk_css_image_invalid_init (GtkCssImageInvalid *image_invalid)
{
}

GtkCssImage *
gtk_css_image_invalid_new (void)
{
  return g_object_new (GTK_TYPE_CSS_IMAGE_INVALID, NULL);
}
