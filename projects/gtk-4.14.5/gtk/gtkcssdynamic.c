/*
 * Copyright © 2018 Red Hat Inc.
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

#include "gtkcssdynamicprivate.h"
#include "gtkprogresstrackerprivate.h"

struct _GtkCssDynamic
{
  GtkStyleAnimation parent;
  gint64            timestamp;
};

static GtkStyleAnimation *
gtk_css_dynamic_advance (GtkStyleAnimation    *style_animation,
                         gint64                timestamp)
{
  return gtk_css_dynamic_new (timestamp);
}

static void
gtk_css_dynamic_apply_values (GtkStyleAnimation    *style_animation,
                              GtkCssAnimatedStyle  *style)
{
  GtkCssDynamic *dynamic = (GtkCssDynamic *)style_animation;
  guint i;

  for (i = 0; i < GTK_CSS_PROPERTY_N_PROPERTIES; i++)
    {
      GtkCssValue *value, *dynamic_value;

      value = gtk_css_style_get_value (GTK_CSS_STYLE (style), i);
      dynamic_value = gtk_css_value_get_dynamic_value (value, dynamic->timestamp);
      if (value != dynamic_value)
        gtk_css_animated_style_set_animated_value (style, i, dynamic_value);
      else
        gtk_css_value_unref (dynamic_value);
    }
}

static gboolean
gtk_css_dynamic_is_finished (GtkStyleAnimation *style_animation)
{
  return FALSE;
}

static gboolean
gtk_css_dynamic_is_static (GtkStyleAnimation *style_animation)
{
  return FALSE;
}

static void
gtk_css_dynamic_free (GtkStyleAnimation *animation)
{
  g_free (animation);
}

static const GtkStyleAnimationClass GTK_CSS_DYNAMIC_CLASS = {
  "GtkCssDynamic",
  gtk_css_dynamic_free,
  gtk_css_dynamic_is_finished,
  gtk_css_dynamic_is_static,
  gtk_css_dynamic_apply_values,
  gtk_css_dynamic_advance,
};

GtkStyleAnimation *
gtk_css_dynamic_new (gint64 timestamp)
{
  GtkCssDynamic *dynamic = g_new (GtkCssDynamic, 1);

  dynamic->parent.class = &GTK_CSS_DYNAMIC_CLASS;
  dynamic->parent.ref_count = 1;
  dynamic->timestamp = timestamp;

  return (GtkStyleAnimation *)dynamic;
}

