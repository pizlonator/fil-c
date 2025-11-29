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

#pragma once

#include "gtk/gtkcssimageprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_PAINTABLE           (gtk_css_image_paintable_get_type ())
#define GTK_CSS_IMAGE_PAINTABLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_PAINTABLE, GtkCssImagePaintable))
#define GTK_CSS_IMAGE_PAINTABLE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_PAINTABLE, GtkCssImagePaintableClass))
#define GTK_IS_CSS_IMAGE_PAINTABLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_PAINTABLE))
#define GTK_IS_CSS_IMAGE_PAINTABLE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_PAINTABLE))
#define GTK_CSS_IMAGE_PAINTABLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_PAINTABLE, GtkCssImagePaintableClass))

typedef struct _GtkCssImagePaintable           GtkCssImagePaintable;
typedef struct _GtkCssImagePaintableClass      GtkCssImagePaintableClass;

struct _GtkCssImagePaintable
{
  GtkCssImage parent;

  GdkPaintable *paintable; /* the paintable we observe */
  GdkPaintable *static_paintable; /* the paintable we render (only set for computed values) */
};

struct _GtkCssImagePaintableClass
{
  GtkCssImageClass parent_class;
};

GType           gtk_css_image_paintable_get_type              (void) G_GNUC_CONST;

GtkCssImage *   gtk_css_image_paintable_new                   (GdkPaintable     *paintable,
                                                               GdkPaintable     *static_paintable);

G_END_DECLS

