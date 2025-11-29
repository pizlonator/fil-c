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

#include "gtk/gtkcountingbloomfilterprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_STATIC_STYLE           (gtk_css_static_style_get_type ())
#define GTK_CSS_STATIC_STYLE(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyle))
#define GTK_CSS_STATIC_STYLE_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyleClass))
#define GTK_IS_CSS_STATIC_STYLE(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_STATIC_STYLE))
#define GTK_IS_CSS_STATIC_STYLE_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_STATIC_STYLE))
#define GTK_CSS_STATIC_STYLE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_STATIC_STYLE, GtkCssStaticStyleClass))

typedef struct _GtkCssStaticStyleClass      GtkCssStaticStyleClass;


struct _GtkCssStaticStyle
{
  GtkCssStyle parent;

  GPtrArray             *sections;             /* sections the values are defined in */

  GtkCssChange           change;               /* change as returned by value lookup */
};

struct _GtkCssStaticStyleClass
{
  GtkCssStyleClass parent_class;
};

GType                   gtk_css_static_style_get_type           (void) G_GNUC_CONST;

GtkCssStyle *           gtk_css_static_style_get_default        (void);
GtkCssStyle *           gtk_css_static_style_new_compute        (GtkStyleProvider               *provider,
                                                                 const GtkCountingBloomFilter   *filter,
                                                                 GtkCssNode                     *node,
                                                                 GtkCssChange                    change);
GtkCssChange            gtk_css_static_style_get_change         (GtkCssStaticStyle              *style);

G_END_DECLS

