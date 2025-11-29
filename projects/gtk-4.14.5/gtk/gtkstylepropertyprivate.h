/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_STYLE_PROPERTY           (_gtk_style_property_get_type ())
#define GTK_STYLE_PROPERTY(obj)           (G_TYPE_CHECK_INSTANCE_CAST (obj, GTK_TYPE_STYLE_PROPERTY, GtkStyleProperty))
#define GTK_STYLE_PROPERTY_CLASS(cls)     (G_TYPE_CHECK_CLASS_CAST (cls, GTK_TYPE_STYLE_PROPERTY, GtkStylePropertyClass))
#define GTK_IS_STYLE_PROPERTY(obj)        (G_TYPE_CHECK_INSTANCE_TYPE (obj, GTK_TYPE_STYLE_PROPERTY))
#define GTK_IS_STYLE_PROPERTY_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE (obj, GTK_TYPE_STYLE_PROPERTY))
#define GTK_STYLE_PROPERTY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_STYLE_PROPERTY, GtkStylePropertyClass))

typedef struct _GtkStyleProperty           GtkStyleProperty;
typedef struct _GtkStylePropertyClass      GtkStylePropertyClass;

typedef GtkCssValue *   (* GtkStyleQueryFunc)        (guint                   id,
						      gpointer                data);

struct _GtkStyleProperty
{
  GObject parent;

  char *name;
};

struct _GtkStylePropertyClass
{
  GObjectClass  parent_class;
  
  GtkCssValue *     (* parse_value)                        (GtkStyleProperty *      property,
                                                            GtkCssParser           *parser);

  GHashTable   *properties;
};

GType               _gtk_style_property_get_type             (void) G_GNUC_CONST;

void                _gtk_style_property_init_properties      (void);

GtkStyleProperty *       _gtk_style_property_lookup        (const char             *name);

const char *             _gtk_style_property_get_name      (GtkStyleProperty       *property);

GtkCssValue *            _gtk_style_property_parse_value   (GtkStyleProperty *      property,
                                                            GtkCssParser           *parser);

G_END_DECLS

