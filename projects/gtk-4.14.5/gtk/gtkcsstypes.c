/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Benjamin Otte <otte@gnome.org>
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

#include "gtkcsstypesprivate.h"

#include "gtkcssnumbervalueprivate.h"

void
gtk_css_change_print (GtkCssChange  change,
                      GString      *string)
{
  const struct {
    GtkCssChange flags;
    const char *name;
  } names[] = {
    { GTK_CSS_CHANGE_CLASS, "class" },
    { GTK_CSS_CHANGE_NAME, "name" },
    { GTK_CSS_CHANGE_ID, "id" },
    { GTK_CSS_CHANGE_FIRST_CHILD, "first-child" },
    { GTK_CSS_CHANGE_LAST_CHILD, "last-child" },
    { GTK_CSS_CHANGE_NTH_CHILD, "nth-child" },
    { GTK_CSS_CHANGE_NTH_LAST_CHILD, "nth-last-child" },
    { GTK_CSS_CHANGE_STATE, "state" },
    { GTK_CSS_CHANGE_HOVER, "hover" },
    { GTK_CSS_CHANGE_DISABLED, "disabled" },
    { GTK_CSS_CHANGE_BACKDROP, "backdrop" },
    { GTK_CSS_CHANGE_SELECTED, "selected" },

    { GTK_CSS_CHANGE_SIBLING_CLASS, "sibling-class" },
    { GTK_CSS_CHANGE_SIBLING_NAME, "sibling-name" },
    { GTK_CSS_CHANGE_SIBLING_ID, "sibling-id" },
    { GTK_CSS_CHANGE_SIBLING_FIRST_CHILD, "sibling-first-child" },
    { GTK_CSS_CHANGE_SIBLING_LAST_CHILD, "sibling-last-child" },
    { GTK_CSS_CHANGE_SIBLING_NTH_CHILD, "sibling-nth-child" },
    { GTK_CSS_CHANGE_SIBLING_NTH_LAST_CHILD, "sibling-nth-last-child" },
    { GTK_CSS_CHANGE_SIBLING_STATE, "sibling-state" },
    { GTK_CSS_CHANGE_SIBLING_HOVER, "sibling-hover" },
    { GTK_CSS_CHANGE_SIBLING_DISABLED, "sibling-disabled" },
    { GTK_CSS_CHANGE_SIBLING_BACKDROP, "sibling-backdrop" },
    { GTK_CSS_CHANGE_SIBLING_SELECTED, "sibling-selected" },

    { GTK_CSS_CHANGE_PARENT_CLASS, "parent-class" },
    { GTK_CSS_CHANGE_PARENT_NAME, "parent-name" },
    { GTK_CSS_CHANGE_PARENT_ID, "parent-id" },
    { GTK_CSS_CHANGE_PARENT_FIRST_CHILD, "parent-first-child" },
    { GTK_CSS_CHANGE_PARENT_LAST_CHILD, "parent-last-child" },
    { GTK_CSS_CHANGE_PARENT_NTH_CHILD, "parent-nth-child" },
    { GTK_CSS_CHANGE_PARENT_NTH_LAST_CHILD, "parent-nth-last-child" },
    { GTK_CSS_CHANGE_PARENT_STATE, "parent-state" },
    { GTK_CSS_CHANGE_PARENT_HOVER, "parent-hover" },
    { GTK_CSS_CHANGE_PARENT_DISABLED, "parent-disabled" },
    { GTK_CSS_CHANGE_PARENT_BACKDROP, "parent-backdrop" },
    { GTK_CSS_CHANGE_PARENT_SELECTED, "parent-selected" },

    { GTK_CSS_CHANGE_PARENT_SIBLING_CLASS, "parent-sibling-class" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_NAME, "parent-sibling-name" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_ID, "parent-sibling-id" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_FIRST_CHILD, "parent-sibling-first-child" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_LAST_CHILD, "parent-sibling-last-child" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_NTH_CHILD, "parent-sibling-nth-child" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_NTH_LAST_CHILD, "parent-sibling-nth-last-child" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_STATE, "parent-sibling-state" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_HOVER, "parent-sibling-hover" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_DISABLED, "parent-sibling-disabled" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_BACKDROP, "parent-sibling-backdrop" },
    { GTK_CSS_CHANGE_PARENT_SIBLING_SELECTED, "parent-sibling-selected" },

    { GTK_CSS_CHANGE_SOURCE, "source" },
    { GTK_CSS_CHANGE_PARENT_STYLE, "parent-style" },
    { GTK_CSS_CHANGE_TIMESTAMP, "timestamp" },
    { GTK_CSS_CHANGE_ANIMATIONS, "animations" },
  };
  guint i;
  gboolean first;

  first = TRUE;

  for (i = 0; i < G_N_ELEMENTS (names); i++)
    {
      if (change & names[i].flags)
        {
          if (first)
            first = FALSE;
          else
            g_string_append (string, "|");
          g_string_append (string, names[i].name);
        }
    }
}

GtkCssDimension
gtk_css_unit_get_dimension (GtkCssUnit unit)
{
  switch (unit)
    {
    case GTK_CSS_NUMBER:
      return GTK_CSS_DIMENSION_NUMBER;

    case GTK_CSS_PERCENT:
      return GTK_CSS_DIMENSION_PERCENTAGE;

    case GTK_CSS_PX:
    case GTK_CSS_PT:
    case GTK_CSS_EM:
    case GTK_CSS_EX:
    case GTK_CSS_REM:
    case GTK_CSS_PC:
    case GTK_CSS_IN:
    case GTK_CSS_CM:
    case GTK_CSS_MM:
      return GTK_CSS_DIMENSION_LENGTH;

    case GTK_CSS_RAD:
    case GTK_CSS_DEG:
    case GTK_CSS_GRAD:
    case GTK_CSS_TURN:
      return GTK_CSS_DIMENSION_ANGLE;

    case GTK_CSS_S:
    case GTK_CSS_MS:
      return GTK_CSS_DIMENSION_TIME;

    default:
      g_assert_not_reached ();
      return GTK_CSS_DIMENSION_PERCENTAGE;
    }
}

char *
gtk_css_change_to_string (GtkCssChange change)
{
  GString *string = g_string_new (NULL);

  gtk_css_change_print (change, string);

  return g_string_free (string, FALSE);
}

const char *
gtk_css_pseudoclass_name (GtkStateFlags state)
{
  static const char * state_names[] = {
    "active",
    "hover",
    "selected",
    "disabled",
    "indeterminate",
    "focus",
    "backdrop",
    "dir(ltr)",
    "dir(rtl)",
    "link",
    "visited",
    "checked",
    "drop(active)",
    "focus-visible",
    "focus-within"
  };
  guint i;

  for (i = 0; i < G_N_ELEMENTS (state_names); i++)
    {
      if (state == (1 << i))
        return state_names[i];
    }

  return NULL;
}

