/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
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

#include "gtkcssinitialvalueprivate.h"

#include "gtkcssarrayvalueprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssstringvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtksettingsprivate.h"
#include "gtkstyleproviderprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
};

static void G_GNUC_NORETURN
gtk_css_value_initial_free (GtkCssValue *value)
{
  /* Can only happen if the unique value gets unreffed too often */
  g_assert_not_reached ();
}

static GtkCssValue *
gtk_css_value_initial_compute (GtkCssValue      *value,
                               guint             property_id,
                               GtkStyleProvider *provider,
                               GtkCssStyle      *style,
                               GtkCssStyle      *parent_style)
{
  GtkSettings *settings;

  switch (property_id)
    {
    case GTK_CSS_PROPERTY_DPI:
      settings = gtk_style_provider_get_settings (provider);
      if (settings)
        {
          int dpi_int;

          g_object_get (settings, "gtk-xft-dpi", &dpi_int, NULL);

          if (dpi_int > 0.0)
            return _gtk_css_number_value_new (dpi_int / 1024., GTK_CSS_NUMBER);
        }
      break;

    case GTK_CSS_PROPERTY_FONT_FAMILY:
      settings = gtk_style_provider_get_settings (provider);
      if (settings && gtk_settings_get_font_family (settings) != NULL)
        return _gtk_css_array_value_new (_gtk_css_string_value_new (gtk_settings_get_font_family (settings)));
      break;

    default:
      break;
    }

  return _gtk_css_value_compute (_gtk_css_style_property_get_initial_value (_gtk_css_style_property_lookup_by_id (property_id)),
                                 property_id,
                                 provider,
                                 style,
                                 parent_style);
}

static gboolean
gtk_css_value_initial_equal (const GtkCssValue *value1,
                             const GtkCssValue *value2)
{
  return TRUE;
}

static GtkCssValue *
gtk_css_value_initial_transition (GtkCssValue *start,
                                  GtkCssValue *end,
                                  guint        property_id,
                                  double       progress)
{
  return NULL;
}

static void
gtk_css_value_initial_print (const GtkCssValue *value,
                             GString           *string)
{
  g_string_append (string, "initial");
}

static const GtkCssValueClass GTK_CSS_VALUE_INITIAL = {
  "GtkCssInitialValue",
  gtk_css_value_initial_free,
  gtk_css_value_initial_compute,
  gtk_css_value_initial_equal,
  gtk_css_value_initial_transition,
  NULL,
  NULL,
  gtk_css_value_initial_print
};

static GtkCssValue initial = { &GTK_CSS_VALUE_INITIAL, 1 };

GtkCssValue *
_gtk_css_initial_value_new (void)
{
  return _gtk_css_value_ref (&initial);
}

GtkCssValue *
_gtk_css_initial_value_get (void)
{
  return &initial;
}
GtkCssValue *
_gtk_css_initial_value_new_compute (guint             property_id,
                                    GtkStyleProvider *provider,
                                    GtkCssStyle      *style,
                                    GtkCssStyle      *parent_style)
{
  return gtk_css_value_initial_compute (NULL,
                                        property_id,
                                        provider,
                                        style,
                                        parent_style);
}
