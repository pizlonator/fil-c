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
#include "gtk/gtkicontheme.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_IMAGE_ICON_THEME           (_gtk_css_image_icon_theme_get_type ())
#define GTK_CSS_IMAGE_ICON_THEME(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_IMAGE_ICON_THEME, GtkCssImageIconTheme))
#define GTK_CSS_IMAGE_ICON_THEME_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_IMAGE_ICON_THEME, GtkCssImageIconThemeClass))
#define GTK_IS_CSS_IMAGE_ICON_THEME(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_IMAGE_ICON_THEME))
#define GTK_IS_CSS_IMAGE_ICON_THEME_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_IMAGE_ICON_THEME))
#define GTK_CSS_IMAGE_ICON_THEME_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_IMAGE_ICON_THEME, GtkCssImageIconThemeClass))

typedef struct _GtkCssImageIconTheme           GtkCssImageIconTheme;
typedef struct _GtkCssImageIconThemeClass      GtkCssImageIconThemeClass;

struct _GtkCssImageIconTheme
{
  GtkCssImage parent;

  GtkIconTheme *icon_theme;
  GdkRGBA colors[4];
  int serial;
  int scale;
  char *name;

  int cached_size;
  gboolean cached_symbolic;
  GtkIconPaintable *cached_icon;
};

struct _GtkCssImageIconThemeClass
{
  GtkCssImageClass parent_class;
};

GType          _gtk_css_image_icon_theme_get_type             (void) G_GNUC_CONST;

G_END_DECLS

