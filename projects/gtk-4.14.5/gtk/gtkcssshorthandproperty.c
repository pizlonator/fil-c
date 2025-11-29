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

#include "gtkcssshorthandpropertyprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssunsetvalueprivate.h"

enum {
  PROP_0,
  PROP_SUBPROPERTIES,
};

G_DEFINE_TYPE (GtkCssShorthandProperty, _gtk_css_shorthand_property, GTK_TYPE_STYLE_PROPERTY)

static void
gtk_css_shorthand_property_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
  GtkCssShorthandProperty *property = GTK_CSS_SHORTHAND_PROPERTY (object);
  const char **subproperties;
  guint i;

  switch (prop_id)
    {
    case PROP_SUBPROPERTIES:
      subproperties = g_value_get_boxed (value);
      g_assert (subproperties);
      for (i = 0; subproperties[i] != NULL; i++)
        {
          GtkStyleProperty *subproperty = _gtk_style_property_lookup (subproperties[i]);
          g_assert (GTK_IS_CSS_STYLE_PROPERTY (subproperty));
          g_ptr_array_add (property->subproperties, subproperty);
        }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static GtkCssValue *
gtk_css_shorthand_property_parse_value (GtkStyleProperty *property,
                                        GtkCssParser     *parser)
{
  GtkCssShorthandProperty *shorthand = GTK_CSS_SHORTHAND_PROPERTY (property);
  const guint n_props = shorthand->subproperties->len;
  GtkCssValue **data;
  GtkCssValue *result;
  guint i;

  data = g_newa (GtkCssValue *, n_props);
  memset (data, 0, sizeof (GtkCssValue *) * n_props);

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
  else if (!shorthand->parse (shorthand, data, parser))
    {
      for (i = 0; i < n_props; i++)
        {
          if (data[i] != NULL)
            _gtk_css_value_unref (data[i]);
        }
      return NULL;
    }

  /* All values that aren't set by the parse func are set to their
   * default values here.
   * XXX: Is the default always initial or can it be inherit? */
  for (i = 0; i < n_props; i++)
    {
      if (data[i] == NULL)
        data[i] = _gtk_css_initial_value_new ();
    }

  result = _gtk_css_array_value_new_from_array (data, n_props);

  return result;
}

static void
_gtk_css_shorthand_property_class_init (GtkCssShorthandPropertyClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkStylePropertyClass *property_class = GTK_STYLE_PROPERTY_CLASS (klass);

  object_class->set_property = gtk_css_shorthand_property_set_property;

  g_object_class_install_property (object_class,
                                   PROP_SUBPROPERTIES,
                                   g_param_spec_boxed ("subproperties", NULL, NULL,
                                                       G_TYPE_STRV,
                                                       G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

  property_class->parse_value = gtk_css_shorthand_property_parse_value;
}

static void
_gtk_css_shorthand_property_init (GtkCssShorthandProperty *shorthand)
{
  shorthand->subproperties = g_ptr_array_new_with_free_func (g_object_unref);
}

GtkCssStyleProperty *
_gtk_css_shorthand_property_get_subproperty (GtkCssShorthandProperty *shorthand,
                                             guint                    property)
{
  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_PROPERTY (shorthand), NULL);
  g_return_val_if_fail (property < shorthand->subproperties->len, NULL);

  return g_ptr_array_index (shorthand->subproperties, property);
}

guint
_gtk_css_shorthand_property_get_n_subproperties (GtkCssShorthandProperty *shorthand)
{
  g_return_val_if_fail (GTK_IS_CSS_SHORTHAND_PROPERTY (shorthand), 0);
  
  return shorthand->subproperties->len;
}

