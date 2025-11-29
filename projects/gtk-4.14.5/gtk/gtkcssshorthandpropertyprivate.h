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

#include <glib-object.h>

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtk/gtkcssstylepropertyprivate.h"
#include "gtk/gtkstylepropertyprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_CSS_SHORTHAND_PROPERTY           (_gtk_css_shorthand_property_get_type ())
#define GTK_CSS_SHORTHAND_PROPERTY(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_CSS_SHORTHAND_PROPERTY, GtkCssShorthandProperty))
#define GTK_CSS_SHORTHAND_PROPERTY_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_CSS_SHORTHAND_PROPERTY, GtkCssShorthandPropertyClass))
#define GTK_IS_CSS_SHORTHAND_PROPERTY(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_CSS_SHORTHAND_PROPERTY))
#define GTK_IS_CSS_SHORTHAND_PROPERTY_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_CSS_SHORTHAND_PROPERTY))
#define GTK_CSS_SHORTHAND_PROPERTY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CSS_SHORTHAND_PROPERTY, GtkCssShorthandPropertyClass))

typedef struct _GtkCssShorthandProperty           GtkCssShorthandProperty;
typedef struct _GtkCssShorthandPropertyClass      GtkCssShorthandPropertyClass;

typedef gboolean              (* GtkCssShorthandPropertyParseFunc)      (GtkCssShorthandProperty *shorthand,
                                                                         GtkCssValue            **values,
                                                                         GtkCssParser            *parser);

struct _GtkCssShorthandProperty
{
  GtkStyleProperty parent;

  GPtrArray *subproperties;

  GtkCssShorthandPropertyParseFunc parse;
};

struct _GtkCssShorthandPropertyClass
{
  GtkStylePropertyClass parent_class;
};

void                    _gtk_css_shorthand_property_init_properties     (void);

GType                   _gtk_css_shorthand_property_get_type            (void) G_GNUC_CONST;

GtkCssStyleProperty *   _gtk_css_shorthand_property_get_subproperty     (GtkCssShorthandProperty *shorthand,
                                                                         guint                    property);
guint                   _gtk_css_shorthand_property_get_n_subproperties (GtkCssShorthandProperty *shorthand) G_GNUC_CONST;


G_END_DECLS

