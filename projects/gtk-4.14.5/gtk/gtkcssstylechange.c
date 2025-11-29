/* GTK - The GIMP Toolkit
 * Copyright (C) 2015 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssstylechangeprivate.h"

#include "gtkcssstylepropertyprivate.h"

static void
compute_change (GtkCssStyleChange *change)
{
  gboolean color_changed = FALSE;

  if (change->old_style->core != change->new_style->core)
    {
      gtk_css_core_values_compute_changes_and_affects (change->old_style,
                                                       change->new_style,
                                                       &change->changes,
                                                       &change->affects);
      color_changed = _gtk_bitmask_get (change->changes, GTK_CSS_PROPERTY_COLOR);
    }

  if (change->old_style->background != change->new_style->background)
    gtk_css_background_values_compute_changes_and_affects (change->old_style,
                                                           change->new_style,
                                                           &change->changes,
                                                           &change->affects);

  if (change->old_style->border != change->new_style->border ||
      (color_changed && (change->old_style->border->border_top_color == NULL ||
                         change->old_style->border->border_right_color == NULL ||
                         change->old_style->border->border_bottom_color == NULL ||
                         change->old_style->border->border_left_color == NULL)))
    gtk_css_border_values_compute_changes_and_affects (change->old_style,
                                                       change->new_style,
                                                       &change->changes,
                                                       &change->affects);

  if (change->old_style->icon != change->new_style->icon)
    gtk_css_icon_values_compute_changes_and_affects (change->old_style,
                                                     change->new_style,
                                                     &change->changes,
                                                     &change->affects);

  if (change->old_style->outline != change->new_style->outline ||
      (color_changed && change->old_style->outline->outline_color == NULL))
    gtk_css_outline_values_compute_changes_and_affects (change->old_style,
                                                        change->new_style,
                                                        &change->changes,
                                                        &change->affects);

  if (change->old_style->font != change->new_style->font ||
      (color_changed && (change->old_style->font->caret_color == NULL ||
                         change->old_style->font->secondary_caret_color == NULL)))
    gtk_css_font_values_compute_changes_and_affects (change->old_style,
                                                     change->new_style,
                                                     &change->changes,
                                                     &change->affects);

  if (change->old_style->font_variant != change->new_style->font_variant ||
      (color_changed && change->old_style->font_variant->text_decoration_color == NULL))
    gtk_css_font_variant_values_compute_changes_and_affects (change->old_style,
                                                             change->new_style,
                                                             &change->changes,
                                                             &change->affects);

  if (change->old_style->animation != change->new_style->animation)
    gtk_css_animation_values_compute_changes_and_affects (change->old_style,
                                                          change->new_style,
                                                          &change->changes,
                                                          &change->affects);

  if (change->old_style->transition != change->new_style->transition)
    gtk_css_transition_values_compute_changes_and_affects (change->old_style,
                                                           change->new_style,
                                                           &change->changes,
                                                           &change->affects);

  if (change->old_style->size != change->new_style->size)
    gtk_css_size_values_compute_changes_and_affects (change->old_style,
                                                     change->new_style,
                                                     &change->changes,
                                                     &change->affects);

  if (change->old_style->other != change->new_style->other)
    gtk_css_other_values_compute_changes_and_affects (change->old_style,
                                                      change->new_style,
                                                      &change->changes,
                                                      &change->affects);
}

void
gtk_css_style_change_init (GtkCssStyleChange *change,
                           GtkCssStyle       *old_style,
                           GtkCssStyle       *new_style)
{
  change->old_style = g_object_ref (old_style);
  change->new_style = g_object_ref (new_style);

  change->affects = 0;
  change->changes = _gtk_bitmask_new ();
  
  if (old_style != new_style)
    compute_change (change);
}

void
gtk_css_style_change_finish (GtkCssStyleChange *change)
{
  g_object_unref (change->old_style);
  g_object_unref (change->new_style);
  _gtk_bitmask_free (change->changes);
}

GtkCssStyle *
gtk_css_style_change_get_old_style (GtkCssStyleChange *change)
{
  return change->old_style;
}

GtkCssStyle *
gtk_css_style_change_get_new_style (GtkCssStyleChange *change)
{
  return change->new_style;
}

gboolean
gtk_css_style_change_has_change (GtkCssStyleChange *change)
{
  return !_gtk_bitmask_is_empty (change->changes);
}

gboolean
gtk_css_style_change_affects (GtkCssStyleChange *change,
                              GtkCssAffects      affects)
{
  return (change->affects & affects) != 0;
}

gboolean
gtk_css_style_change_changes_property (GtkCssStyleChange *change,
                                       guint              id)
{
  return _gtk_bitmask_get (change->changes, id);
}

void
gtk_css_style_change_print (GtkCssStyleChange *change,
                            GString           *string)
{
  int i;
  GtkCssStyle *old = gtk_css_style_change_get_old_style (change);
  GtkCssStyle *new = gtk_css_style_change_get_new_style (change);

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i ++)
    {
      if (gtk_css_style_change_changes_property (change, i))
        {
          GtkCssStyleProperty *prop;
          GtkCssValue *value;
          const char *name;

          prop = _gtk_css_style_property_lookup_by_id (i);
          name = _gtk_style_property_get_name (GTK_STYLE_PROPERTY (prop));

          g_string_append_printf (string, "%s: ", name);
          value = gtk_css_style_get_value (old, i);
          _gtk_css_value_print (value, string);
          g_string_append (string, "\n");

          g_string_append_printf (string, "%s: ", name);
          value = gtk_css_style_get_value (new, i);
          _gtk_css_value_print (value, string);
          g_string_append (string, "\n");
        }
    }

}

char *
gtk_css_style_change_to_string (GtkCssStyleChange *change)
{
  GString *string = g_string_new ("");

  gtk_css_style_change_print (change, string);

  return g_string_free (string, FALSE);
}
