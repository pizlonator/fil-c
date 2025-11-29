/*
 * Copyright © 2015 Red Hat Inc.
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include "gtk/gtkcssimageprivate.h"
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_RADIAL           (_gtk_css_image_radial_get_type ())
#define GTK_CSS_IMAGE_RADIAL(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_RADIAL, GtkCssImageRadial))
#define GTK_CSS_IMAGE_RADIAL_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_RADIAL, GtkCssImageRadialClass))
#define GTK_IS_CSS_IMAGE_RADIAL(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_RADIAL))
#define GTK_IS_CSS_IMAGE_RADIAL_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_RADIAL))
#define GTK_CSS_IMAGE_RADIAL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_RADIAL, GtkCssImageRadialClass))

typedef struct _GtkCssImageRadial           GtkCssImageRadial;
typedef struct _GtkCssImageRadialClass      GtkCssImageRadialClass;
typedef struct _GtkCssImageRadialColorStop  GtkCssImageRadialColorStop;

struct _GtkCssImageRadialColorStop {
  GtkCssValue        *offset;
  GtkCssValue        *color;
};

typedef enum {
  GTK_CSS_EXPLICIT_SIZE,
  GTK_CSS_CLOSEST_SIDE,
  GTK_CSS_FARTHEST_SIDE,
  GTK_CSS_CLOSEST_CORNER,
  GTK_CSS_FARTHEST_CORNER
} GtkCssRadialSize;

struct _GtkCssImageRadial
{
  GtkCssImage parent;

  GtkCssValue *position;
  GtkCssValue *sizes[2];
  guint n_stops;
  GtkCssImageRadialColorStop *color_stops;
  GtkCssRadialSize size;
  guint circle : 1;
  guint repeating :1;
};

struct _GtkCssImageRadialClass
{
  GtkCssImageClass parent_class;
};

GType          _gtk_css_image_radial_get_type             (void) G_GNUC_CONST;

G_END_DECLS

