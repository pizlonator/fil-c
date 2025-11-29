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

#include "gtk/gtkcssstyleprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_ANIMATED_STYLE           (gtk_css_animated_style_get_type ())
#define GTK_CSS_ANIMATED_STYLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_ANIMATED_STYLE, GtkCssAnimatedStyle))
#define GTK_CSS_ANIMATED_STYLE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_ANIMATED_STYLE, GtkCssAnimatedStyleClass))
#define GTK_IS_CSS_ANIMATED_STYLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_ANIMATED_STYLE))
#define GTK_IS_CSS_ANIMATED_STYLE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_ANIMATED_STYLE))
#define GTK_CSS_ANIMATED_STYLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_ANIMATED_STYLE, GtkCssAnimatedStyleClass))

typedef struct _GtkCssAnimatedStyle           GtkCssAnimatedStyle;
typedef struct _GtkCssAnimatedStyleClass      GtkCssAnimatedStyleClass;

struct _GtkCssAnimatedStyle
{
  GtkCssStyle parent;

  GtkCssStyle           *style;                /* the style if we weren't animating */

  gint64                 current_time;         /* the current time in our world */
  gpointer              *animations;           /* GtkStyleAnimation**, least important one first */
  guint                  n_animations;
};

struct _GtkCssAnimatedStyleClass
{
  GtkCssStyleClass parent_class;
};

GType                   gtk_css_animated_style_get_type         (void) G_GNUC_CONST;

GtkCssStyle *           gtk_css_animated_style_new              (GtkCssStyle            *base_style,
                                                                 GtkCssStyle            *parent_style,
                                                                 gint64                  timestamp,
                                                                 GtkStyleProvider       *provider,
                                                                 GtkCssStyle            *previous_style);
GtkCssStyle *           gtk_css_animated_style_new_advance      (GtkCssAnimatedStyle    *source,
                                                                 GtkCssStyle            *base,
                                                                 gint64                  timestamp);

void                    gtk_css_animated_style_set_animated_value(GtkCssAnimatedStyle   *style,
                                                                 guint                   id,
                                                                 GtkCssValue            *value);

GtkCssValue *           gtk_css_animated_style_get_intrinsic_value (GtkCssAnimatedStyle *style,
                                                                 guint                   id);

G_END_DECLS

