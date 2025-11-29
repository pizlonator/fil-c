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

#pragma once

#include "gtk/gtkcssimageprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_URL           (_gtk_css_image_url_get_type ())
#define GTK_CSS_IMAGE_URL(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_URL, GtkCssImageUrl))
#define GTK_CSS_IMAGE_URL_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_URL, GtkCssImageUrlClass))
#define GTK_IS_CSS_IMAGE_URL(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_URL))
#define GTK_IS_CSS_IMAGE_URL_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_URL))
#define GTK_CSS_IMAGE_URL_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_URL, GtkCssImageUrlClass))

typedef struct _GtkCssImageUrl           GtkCssImageUrl;
typedef struct _GtkCssImageUrlClass      GtkCssImageUrlClass;

struct _GtkCssImageUrl
{
  GtkCssImage parent;

  GFile           *file;                /* the file we're loading from */
  GtkCssImage     *loaded_image;        /* the actual image we render */
};

struct _GtkCssImageUrlClass
{
  GtkCssImageClass parent_class;
};

GType          _gtk_css_image_url_get_type             (void) G_GNUC_CONST;

G_END_DECLS

