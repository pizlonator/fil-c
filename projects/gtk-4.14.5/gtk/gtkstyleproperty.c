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

#include "config.h"

#include "gtkstylepropertyprivate.h"

#include "gtkcssprovider.h"
#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcssshorthandpropertyprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkprivatetypebuiltins.h"

enum {
  PROP_0,
  PROP_NAME
};

G_DEFINE_ABSTRACT_TYPE (GtkStyleProperty, _gtk_style_property, G_TYPE_OBJECT)

static void
gtk_style_property_finalize (GObject *object)
{
  GtkStyleProperty *property = GTK_STYLE_PROPERTY (object);

  g_warning ("finalizing %s '%s', how could this happen?", G_OBJECT_TYPE_NAME (object), property->name);

  G_OBJECT_CLASS (_gtk_style_property_parent_class)->finalize (object);
}

static void
gtk_style_property_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkStyleProperty *property = GTK_STYLE_PROPERTY (object);
  GtkStylePropertyClass *klass = GTK_STYLE_PROPERTY_GET_CLASS (property);

  switch (prop_id)
    {
    case PROP_NAME:
      property->name = g_value_dup_string (value);
      g_assert (property->name);
      g_assert (g_hash_table_lookup (klass->properties, property->name) == NULL);
      g_hash_table_insert (klass->properties, property->name, property);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_style_property_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  GtkStyleProperty *property = GTK_STYLE_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_NAME:
      g_value_set_string (value, property->name);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
_gtk_style_property_class_init (GtkStylePropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_style_property_finalize;
  object_class->set_property = gtk_style_property_set_property;
  object_class->get_property = gtk_style_property_get_property;

  g_object_class_install_property (object_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  klass->properties = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
_gtk_style_property_init (GtkStyleProperty *property)
{
}

/**
 * _gtk_style_property_parse_value:
 * @property: the property
 * @parser: the parser to parse from
 *
 * Tries to parse the given @property from the given @parser into
 * @value. The type that @value will be assigned is dependent on
 * the parser and no assumptions must be made about it. If the
 * parsing fails, %FALSE will be returned and @value will be
 * left uninitialized.
 *
 * Only if @property is a `GtkCssShorthandProperty`, the @value will
 * always be a `GtkCssValue` whose values can be queried with
 * _gtk_css_array_value_get_nth().
 *
 * Returns: (nullable): %NULL on failure or the parsed `GtkCssValue`
 **/
GtkCssValue *
_gtk_style_property_parse_value (GtkStyleProperty *property,
                                 GtkCssParser     *parser)
{
  GtkStylePropertyClass *klass;

  g_return_val_if_fail (GTK_IS_STYLE_PROPERTY (property), NULL);
  g_return_val_if_fail (parser != NULL, NULL);

  klass = GTK_STYLE_PROPERTY_GET_CLASS (property);

  return klass->parse_value (property, parser);
}

void
_gtk_style_property_init_properties (void)
{
  static gboolean initialized = FALSE;

  if (G_LIKELY (initialized))
    return;

  initialized = TRUE;

  _gtk_css_style_property_init_properties ();
  /* initialize shorthands last, they depend on the real properties existing */
  _gtk_css_shorthand_property_init_properties ();
}

/**
 * _gtk_style_property_lookup:
 * @name: name of the property to lookup
 *
 * Looks up the CSS property with the given @name. If no such
 * property exists, %NULL is returned.
 *
 * Returns: (nullable) (transfer none): The property
 */
GtkStyleProperty *
_gtk_style_property_lookup (const char *name)
{
  GtkStylePropertyClass *klass;

  g_return_val_if_fail (name != NULL, NULL);

  _gtk_style_property_init_properties ();

  klass = g_type_class_peek (GTK_TYPE_STYLE_PROPERTY);

  return g_hash_table_lookup (klass->properties, name);
}

/**
 * _gtk_style_property_get_name:
 * @property: the property to query
 *
 * Gets the name of the given property.
 *
 * Returns: the name of the property
 **/
const char *
_gtk_style_property_get_name (GtkStyleProperty *property)
{
  g_return_val_if_fail (GTK_IS_STYLE_PROPERTY (property), NULL);

  return property->name;
}
