/* GTK - The GIMP Toolkit
 * Copyright (C) 2014 Red Hat, Inc.
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

#include "gtkcssunsetvalueprivate.h"

#include "gtkcssinheritvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkcssstylepropertyprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
};

static void G_GNUC_NORETURN
gtk_css_value_unset_free (GtkCssValue *value)
{
  /* Can only happen if the unique value gets unreffed too often */
  g_assert_not_reached ();
}

static GtkCssValue *
gtk_css_value_unset_compute (GtkCssValue      *value,
                             guint             property_id,
                             GtkStyleProvider *provider,
                             GtkCssStyle      *style,
                             GtkCssStyle      *parent_style)
{
  GtkCssStyleProperty *property;
  GtkCssValue *unset_value;
  
  property = _gtk_css_style_property_lookup_by_id (property_id);

  if (_gtk_css_style_property_is_inherit (property))
    unset_value = _gtk_css_inherit_value_get ();
  else
    unset_value = _gtk_css_initial_value_get ();

  return _gtk_css_value_compute (unset_value,
                                 property_id,
                                 provider,
                                 style,
                                 parent_style);
}

static gboolean
gtk_css_value_unset_equal (const GtkCssValue *value1,
                           const GtkCssValue *value2)
{
  return TRUE;
}

static GtkCssValue *
gtk_css_value_unset_transition (GtkCssValue *start,
                                GtkCssValue *end,
                                guint        property_id,
                                double       progress)
{
  return NULL;
}

static void
gtk_css_value_unset_print (const GtkCssValue *value,
                             GString           *string)
{
  g_string_append (string, "unset");
}

static const GtkCssValueClass GTK_CSS_VALUE_UNSET = {
  "GtkCssUnsetValue",
  gtk_css_value_unset_free,
  gtk_css_value_unset_compute,
  gtk_css_value_unset_equal,
  gtk_css_value_unset_transition,
  NULL,
  NULL,
  gtk_css_value_unset_print
};

static GtkCssValue unset = { &GTK_CSS_VALUE_UNSET, 1 };

GtkCssValue *
_gtk_css_unset_value_new (void)
{
  return _gtk_css_value_ref (&unset);
}
