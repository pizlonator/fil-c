/* GTK - The GIMP Toolkit
 * Copyright (C) 2017 Red Hat, Inc.
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
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssfontvariationsvalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GHashTable *axes;
};

static GtkCssValue *default_font_variations;

static GtkCssValue *gtk_css_font_variations_value_new_empty (void);

static void
gtk_css_font_variations_value_add_axis (GtkCssValue *value,
                                        const char  *name,
                                        GtkCssValue *coord)
{
  g_hash_table_insert (value->axes, g_strdup (name), coord);
}


static void
gtk_css_value_font_variations_free (GtkCssValue *value)
{
  g_hash_table_unref (value->axes);

  g_free (value);
}

static GtkCssValue *
gtk_css_value_font_variations_compute (GtkCssValue      *specified,
                                       guint             property_id,
                                       GtkStyleProvider *provider,
                                       GtkCssStyle      *style,
                                       GtkCssStyle      *parent_style)
{
  return _gtk_css_value_ref (specified);
}

static gboolean
gtk_css_value_font_variations_equal (const GtkCssValue *value1,
                                     const GtkCssValue *value2)
{
  gpointer name, coord1, coord2;
  GHashTableIter iter;

  if (g_hash_table_size (value1->axes) != g_hash_table_size (value2->axes))
    return FALSE;

  g_hash_table_iter_init (&iter, value1->axes);
  while (g_hash_table_iter_next (&iter, &name, &coord1))
    {
      coord2 = g_hash_table_lookup (value2->axes, name);
      if (coord2 == NULL)
        return FALSE;

      if (!_gtk_css_value_equal (coord1, coord2))
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_font_variations_transition (GtkCssValue *start,
                                          GtkCssValue *end,
                                          guint        property_id,
                                          double       progress)
{
  const char *name;
  GtkCssValue *start_coord, *end_coord;
  GHashTableIter iter;
  GtkCssValue *result, *transition;

  /* XXX: For value that are only in start or end but not both,
   * we don't transition but just keep the value.
   * That causes an abrupt transition to the new value at the end.
   */

  result = gtk_css_font_variations_value_new_empty ();

  g_hash_table_iter_init (&iter, start->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&start_coord))
    {
      end_coord = g_hash_table_lookup (end->axes, name);
      if (end_coord == NULL)
        transition = _gtk_css_value_ref (start_coord);
      else
        transition = _gtk_css_value_transition (start_coord, end_coord, property_id, progress);

      gtk_css_font_variations_value_add_axis (result, name, transition);
    }

  g_hash_table_iter_init (&iter, end->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&end_coord))
    {
      start_coord = g_hash_table_lookup (start->axes, name);
      if (start_coord != NULL)
        continue;

      gtk_css_font_variations_value_add_axis (result, name, _gtk_css_value_ref (end_coord));
    }

  return result;
}

static void
gtk_css_value_font_variations_print (const GtkCssValue *value,
                                     GString           *string)
{
  GHashTableIter iter;
  const char *name;
  GtkCssValue *coord;
  gboolean first = TRUE;

  if (value == default_font_variations)
    {
      g_string_append (string, "normal");
      return;
    }

  g_hash_table_iter_init (&iter, value->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&coord))
    {
      if (first)
        first = FALSE;
      else
        g_string_append (string, ", ");
      g_string_append_printf (string, "\"%s\" ", name);
      _gtk_css_value_print (coord, string);
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_VARIATIONS = {
  "GtkCssFontVariationsValue",
  gtk_css_value_font_variations_free,
  gtk_css_value_font_variations_compute,
  gtk_css_value_font_variations_equal,
  gtk_css_value_font_variations_transition,
  NULL,
  NULL,
  gtk_css_value_font_variations_print
};

static GtkCssValue *
gtk_css_font_variations_value_new_empty (void)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_FONT_VARIATIONS);
  result->axes = g_hash_table_new_full (g_str_hash, g_str_equal,
                                        g_free,
                                        (GDestroyNotify) _gtk_css_value_unref);
  result->is_computed = TRUE;

  return result;
}

GtkCssValue *
gtk_css_font_variations_value_new_default (void)
{
  if (default_font_variations == NULL)
    default_font_variations = gtk_css_font_variations_value_new_empty ();

  return _gtk_css_value_ref (default_font_variations);
}

static gboolean
is_valid_opentype_tag (const char *s)
{
  if (strlen (s) != 4)
    return FALSE;

  if (s[0] < 0x20 || s[0] > 0x7e ||
      s[1] < 0x20 || s[1] > 0x7e ||
      s[2] < 0x20 || s[2] > 0x7e ||
      s[3] < 0x20 || s[3] > 0x7e)
    return FALSE;

  return TRUE;
}

GtkCssValue *
gtk_css_font_variations_value_parse (GtkCssParser *parser)
{
  GtkCssValue *result, *coord;
  char *name;

  if (gtk_css_parser_try_ident (parser, "normal"))
    return gtk_css_font_variations_value_new_default ();

  result = gtk_css_font_variations_value_new_empty ();

  do {
    name = gtk_css_parser_consume_string (parser);
    if (name == NULL)
      {
        _gtk_css_value_unref (result);
        return NULL;
      }

    if (!is_valid_opentype_tag (name))
      {
        gtk_css_parser_error_value (parser, "Not a valid OpenType tag.");
        g_free (name);
        _gtk_css_value_unref (result);
        return NULL;
      }

    coord = _gtk_css_number_value_parse (parser, GTK_CSS_PARSE_NUMBER);
    if (coord == NULL)
      {
        g_free (name);
        _gtk_css_value_unref (result);
        return NULL;
      }

    gtk_css_font_variations_value_add_axis (result, name, coord);
    g_free (name);
  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  return result;
}

char *
gtk_css_font_variations_value_get_variations (GtkCssValue *value)
{
  GtkCssValue *coord;
  GHashTableIter iter;
  gboolean first = TRUE;
  const char *name;
  GString *string;

  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_VARIATIONS, NULL);

  if (value == default_font_variations)
    return NULL;

  string = g_string_new ("");

  g_hash_table_iter_init (&iter, value->axes);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&coord))
    {
      if (first)
        first = FALSE;
      else
        g_string_append (string, ",");
      g_string_append_printf (string, "%s=%g", name,
                              _gtk_css_number_value_get (coord, 100));
    }

  return g_string_free (string, FALSE);
}
