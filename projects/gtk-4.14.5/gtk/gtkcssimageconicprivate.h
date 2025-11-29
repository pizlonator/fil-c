/*
 * Copyright © 2012 Red Hat Inc.
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
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_CONIC           (gtk_css_image_conic_get_type ())
#define GTK_CSS_IMAGE_CONIC(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_CONIC, GtkCssImageConic))
#define GTK_CSS_IMAGE_CONIC_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_CONIC, GtkCssImageConicClass))
#define GTK_IS_CSS_IMAGE_CONIC(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_CONIC))
#define GTK_IS_CSS_IMAGE_CONIC_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_CONIC))
#define GTK_CSS_IMAGE_CONIC_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_CONIC, GtkCssImageConicClass))

typedef struct _GtkCssImageConic           GtkCssImageConic;
typedef struct _GtkCssImageConicClass      GtkCssImageConicClass;
typedef struct _GtkCssImageConicColorStop  GtkCssImageConicColorStop;

struct _GtkCssImageConicColorStop {
  GtkCssValue        *offset;
  GtkCssValue        *color;
};

struct _GtkCssImageConic
{
  GtkCssImage parent;

  GtkCssValue *center;
  GtkCssValue *rotation;

  guint n_stops;
  GtkCssImageConicColorStop *color_stops;
};

struct _GtkCssImageConicClass
{
  GtkCssImageClass parent_class;
};

GType           gtk_css_image_conic_get_type                    (void) G_GNUC_CONST;

G_END_DECLS

