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

#include "config.h"

#include "gtkcssstylepropertyprivate.h"

#include "gtkcssenumvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkcssunsetvalueprivate.h"
#include "gtkprivatetypebuiltins.h"
#include "gtkprivate.h"

enum {
  PROP_0,
  PROP_ANIMATED,
  PROP_AFFECTS,
  PROP_ID,
  PROP_INHERIT,
  PROP_INITIAL
};

G_DEFINE_TYPE (GtkCssStyleProperty, _gtk_css_style_property, GTK_TYPE_STYLE_PROPERTY)

static GtkCssStylePropertyClass *gtk_css_style_property_class = NULL;

static void
gtk_css_style_property_constructed (GObject *object)
{
  GtkCssStyleProperty *property = GTK_CSS_STYLE_PROPERTY (object);
  GtkCssStylePropertyClass *klass = GTK_CSS_STYLE_PROPERTY_GET_CLASS (property);

  property->id = klass->style_properties->len;
  g_ptr_array_add (klass->style_properties, property);

  G_OBJECT_CLASS (_gtk_css_style_property_parent_class)->constructed (object);
}

static void
gtk_css_style_property_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  GtkCssStyleProperty *property = GTK_CSS_STYLE_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_ANIMATED:
      property->animated = g_value_get_boolean (value);
      break;
    case PROP_AFFECTS:
      property->affects = g_value_get_flags (value);
      break;
    case PROP_INHERIT:
      property->inherit = g_value_get_boolean (value);
      break;
    case PROP_INITIAL:
      property->initial_value = g_value_dup_boxed (value);
      g_assert (property->initial_value != NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gtk_css_style_property_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  GtkCssStyleProperty *property = GTK_CSS_STYLE_PROPERTY (object);

  switch (prop_id)
    {
    case PROP_ANIMATED:
      g_value_set_boolean (value, property->animated);
      break;
    case PROP_AFFECTS:
      g_value_set_flags (value, property->affects);
      break;
    case PROP_ID:
      g_value_set_boolean (value, property->id);
      break;
    case PROP_INHERIT:
      g_value_set_boolean (value, property->inherit);
      break;
    case PROP_INITIAL:
      g_value_set_boxed (value, property->initial_value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkCssValue *
gtk_css_style_property_parse_value (GtkStyleProperty *property,
                                    GtkCssParser     *parser)
{
  GtkCssStyleProperty *style_property = GTK_CSS_STYLE_PROPERTY (property);

  if (gtk_css_parser_try_ident (parser, "initial"))
    {
      /* the initial value can be explicitly specified with the
       * ‘initial’ keyword which all properties accept.
       */
      return _gtk_css_initial_value_new ();
    }
  else if (gtk_css_parser_try_ident (parser, "inherit"))
    {
      /* All properties accept the ‘inherit’ value which
       * explicitly specifies that the value will be determined
       * by inheritance. The ‘inherit’ value can be used to
       * strengthen inherited values in the cascade, and it can
       * also be used on properties that are not normally inherited.
       */
      return _gtk_css_inherit_value_new ();
    }
  else if (gtk_css_parser_try_ident (parser, "unset"))
    {
      /* If the cascaded value of a property is the unset keyword,
       * then if it is an inherited property, this is treated as
       * inherit, and if it is not, this is treated as initial.
       */
      return _gtk_css_unset_value_new ();
    }

  return (* style_property->parse_value) (style_property, parser);
}

static void
_gtk_css_style_property_class_init (GtkCssStylePropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkStylePropertyClass *property_class = GTK_STYLE_PROPERTY_CLASS (klass);

  object_class->constructed = gtk_css_style_property_constructed;
  object_class->set_property = gtk_css_style_property_set_property;
  object_class->get_property = gtk_css_style_property_get_property;

  g_object_class_install_property (object_class,
                                   PROP_ANIMATED,
                                   g_param_spec_boolean ("animated", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_AFFECTS,
                                   g_param_spec_flags ("affects", NULL, NULL,
                                                       GTK_TYPE_CSS_AFFECTS,
                                                       0,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_ID,
                                   g_param_spec_uint ("id", NULL, NULL,
                                                      0, G_MAXUINT, 0,
                                                      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_INHERIT,
                                   g_param_spec_boolean ("inherit", NULL, NULL,
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (object_class,
                                   PROP_INITIAL,
                                   g_param_spec_boxed ("initial-value", NULL, NULL,
                                                       GTK_TYPE_CSS_VALUE,
                                                       G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

  property_class->parse_value = gtk_css_style_property_parse_value;

  klass->style_properties = g_ptr_array_new ();

  gtk_css_style_property_class = klass;
}

static GtkCssValue *
gtk_css_style_property_real_parse_value (GtkCssStyleProperty *property,
                                         GtkCssParser        *parser)
{
  g_assert_not_reached ();
  return NULL;
}

static void
_gtk_css_style_property_init (GtkCssStyleProperty *property)
{
  property->parse_value = gtk_css_style_property_real_parse_value;
}

/**
 * _gtk_css_style_property_get_n_properties:
 *
 * Gets the number of style properties. This number can increase when new
 * theme engines are loaded. Shorthand properties are not included here.
 *
 * Returns: The number of style properties.
 **/
guint
_gtk_css_style_property_get_n_properties (void)
{
  if (G_UNLIKELY (gtk_css_style_property_class == NULL))
    {
      _gtk_style_property_init_properties ();
      g_assert (gtk_css_style_property_class);
    }

  return gtk_css_style_property_class->style_properties->len;
}

/**
 * _gtk_css_style_property_lookup_by_id:
 * @id: the id of the property
 *
 * Gets the style property with the given id. All style properties (but not
 * shorthand properties) are indexable by id so that it’s easy to use arrays
 * when doing style lookups.
 *
 * Returns: (transfer none): The style property with the given id
 **/
GtkCssStyleProperty *
_gtk_css_style_property_lookup_by_id (guint id)
{

  if (G_UNLIKELY (gtk_css_style_property_class == NULL))
    {
      _gtk_style_property_init_properties ();
      g_assert (gtk_css_style_property_class);
    }

  gtk_internal_return_val_if_fail (id < gtk_css_style_property_class->style_properties->len, NULL);

  return g_ptr_array_index (gtk_css_style_property_class->style_properties, id);
}

/**
 * _gtk_css_style_property_is_inherit:
 * @property: the property
 *
 * Queries if the given @property is inherited. See the
 * [CSS Documentation](http://www.w3.org/TR/css3-cascade/#inheritance)
 * for an explanation of this concept.
 *
 * Returns: %TRUE if the property is inherited by default.
 **/
gboolean
_gtk_css_style_property_is_inherit (GtkCssStyleProperty *property)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), FALSE);

  return property->inherit;
}

/**
 * _gtk_css_style_property_is_animated:
 * @property: the property
 *
 * Queries if the given @property can be is animated. See the
 * [CSS Documentation](http://www.w3.org/TR/css3-transitions/#animatable-css)
 * for animatable properties.
 *
 * Returns: %TRUE if the property can be animated.
 **/
gboolean
_gtk_css_style_property_is_animated (GtkCssStyleProperty *property)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), FALSE);

  return property->animated;
}

/**
 * _gtk_css_style_property_get_affects:
 * @property: the property
 *
 * Returns all the things this property affects. See @GtkCssAffects for what
 * the flags mean.
 *
 * Returns: The things this property affects.
 **/
GtkCssAffects
_gtk_css_style_property_get_affects (GtkCssStyleProperty *property)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), 0);

  return property->affects;
}

/**
 * _gtk_css_style_property_get_id:
 * @property: the property
 *
 * Gets the id for the given property. IDs are used to allow using arrays
 * for style lookups.
 *
 * Returns: The id of the property
 **/
guint
_gtk_css_style_property_get_id (GtkCssStyleProperty *property)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), 0);

  return property->id;
}

/**
 * _gtk_css_style_property_get_initial_value:
 * @property: the property
 *
 * Queries the initial value of the given @property. See the
 * [CSS Documentation](http://www.w3.org/TR/css3-cascade/#initial)
 * for an explanation of this concept.
 *
 * Returns: (transfer none): the initial value. The value will never change.
 **/
GtkCssValue *
_gtk_css_style_property_get_initial_value (GtkCssStyleProperty *property)
{
  gtk_internal_return_val_if_fail (GTK_IS_CSS_STYLE_PROPERTY (property), NULL);

  return property->initial_value;
}
