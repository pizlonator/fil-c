/*
 * Copyright © 2016 Red Hat Inc.
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

#include "gtk/gtkcssimageurlprivate.h"
#include "gtk/gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_RECOLOR           (_gtk_css_image_recolor_get_type ())
#define GTK_CSS_IMAGE_RECOLOR(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_RECOLOR, GtkCssImageRecolor))
#define GTK_CSS_IMAGE_RECOLOR_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_RECOLOR, GtkCssImageRecolorClass))
#define GTK_IS_CSS_IMAGE_RECOLOR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_RECOLOR))
#define GTK_IS_CSS_IMAGE_RECOLOR_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_RECOLOR))
#define GTK_CSS_IMAGE_RECOLOR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_RECOLOR, GtkCssImageRecolorClass))

typedef struct _GtkCssImageRecolor           GtkCssImageRecolor;
typedef struct _GtkCssImageRecolorClass      GtkCssImageRecolorClass;

struct _GtkCssImageRecolor
{
  GtkCssImage parent;

  GFile *file;
  GtkCssValue *palette;
  GdkTexture *texture;
  GdkRGBA color;
  GdkRGBA success;
  GdkRGBA warning;
  GdkRGBA error;
};

struct _GtkCssImageRecolorClass
{
  GtkCssImageClass parent_class;
};

GType          _gtk_css_image_recolor_get_type             (void) G_GNUC_CONST;

G_END_DECLS

