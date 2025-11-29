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

#include "gtkcsstypesprivate.h"
#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcssnumbervalueprivate.h"
#include "gtkcssfontfeaturesvalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GHashTable *features;
};

static GtkCssValue *default_font_features;

static GtkCssValue *gtk_css_font_features_value_new_empty (void);

static void
gtk_css_font_features_value_add_feature (GtkCssValue *value,
                                         const char  *name,
                                         int          num)
{
  g_hash_table_insert (value->features, g_strdup (name), GINT_TO_POINTER (num));
}


static void
gtk_css_value_font_features_free (GtkCssValue *value)
{
  g_hash_table_unref (value->features);

  g_free (value);
}

static GtkCssValue *
gtk_css_value_font_features_compute (GtkCssValue      *specified,
                                     guint             property_id,
                                     GtkStyleProvider *provider,
                                     GtkCssStyle      *style,
                                     GtkCssStyle      *parent_style)
{
  return _gtk_css_value_ref (specified);
}

static gboolean
gtk_css_value_font_features_equal (const GtkCssValue *value1,
                                   const GtkCssValue *value2)
{
  gpointer name, val1, val2;
  GHashTableIter iter;

  if (g_hash_table_size (value1->features) != g_hash_table_size (value2->features))
    return FALSE;

  g_hash_table_iter_init (&iter, value1->features);
  while (g_hash_table_iter_next (&iter, &name, &val1))
    {
      if (!g_hash_table_lookup_extended (value2->features, name, NULL, &val2))
        return FALSE;

      if (val1 != val2)
        return FALSE;
    }

  return TRUE;
}

static GtkCssValue *
gtk_css_value_font_features_transition (GtkCssValue *start,
                                        GtkCssValue *end,
                                        guint        property_id,
                                        double       progress)
{
  const char *name;
  gpointer start_val, end_val;
  GHashTableIter iter;
  gpointer transition;
  GtkCssValue *result;

  /* XXX: For value that are only in start or end but not both,
   * we don't transition but just keep the value.
   * That causes an abrupt transition to the new value at the end.
   */

  result = gtk_css_font_features_value_new_empty ();

  g_hash_table_iter_init (&iter, start->features);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&start_val))
    {
      if (!g_hash_table_lookup_extended (end->features, name, NULL, &end_val))
        transition = start_val;
      else
        transition = progress < 0.5 ? start_val : end_val;

      gtk_css_font_features_value_add_feature (result, name, GPOINTER_TO_INT (transition));
    }

  g_hash_table_iter_init (&iter, end->features);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&end_val))
    {
      if (g_hash_table_lookup_extended (end->features, name, NULL, &start_val))
        continue;

      gtk_css_font_features_value_add_feature (result, name, GPOINTER_TO_INT (end_val));
    }

  return result;
}

static void
gtk_css_value_font_features_print (const GtkCssValue *value,
                                   GString           *string)
{
  GHashTableIter iter;
  const char *name;
  gpointer val;
  gboolean first = TRUE;

  if (value == default_font_features)
    {
      g_string_append (string, "normal");
      return;
    }

  g_hash_table_iter_init (&iter, value->features);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&val))
    {
      if (first)
        first = FALSE;
      else
        g_string_append (string, ", ");
      g_string_append_printf (string, "\"%s\" ", name);
      g_string_append_printf (string, "%d", GPOINTER_TO_INT (val));
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_FONT_FEATURES = {
  "GtkCssFontFeaturesValue",
  gtk_css_value_font_features_free,
  gtk_css_value_font_features_compute,
  gtk_css_value_font_features_equal,
  gtk_css_value_font_features_transition,
  NULL,
  NULL,
  gtk_css_value_font_features_print
};

static GtkCssValue *
gtk_css_font_features_value_new_empty (void)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_FONT_FEATURES);
  result->features = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  result->is_computed = TRUE;

  return result;
}

GtkCssValue *
gtk_css_font_features_value_new_default (void)
{
  if (default_font_features == NULL)
    default_font_features = gtk_css_font_features_value_new_empty ();

  return _gtk_css_value_ref (default_font_features);
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
gtk_css_font_features_value_parse (GtkCssParser *parser)
{
  GtkCssValue *result;
  char *name;
  int num;

  if (gtk_css_parser_try_ident (parser, "normal"))
    return gtk_css_font_features_value_new_default ();

  result = gtk_css_font_features_value_new_empty ();

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

    if (gtk_css_parser_try_ident (parser, "on"))
      num = 1;
    else if (gtk_css_parser_try_ident (parser, "off"))
      num = 0;
    else if (gtk_css_parser_has_integer (parser))
      {
        if (!gtk_css_parser_consume_integer (parser, &num))
          {
            g_free (name);
            _gtk_css_value_unref (result);
            return NULL;
          }
      }
    else
      num = 1;

    gtk_css_font_features_value_add_feature (result, name, num);
    g_free (name);
  } while (gtk_css_parser_try_token (parser, GTK_CSS_TOKEN_COMMA));

  return result;
}

char *
gtk_css_font_features_value_get_features (GtkCssValue *value)
{
  GtkCssValue *val;
  GHashTableIter iter;
  gboolean first = TRUE;
  const char *name;
  GString *string;

  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_FONT_FEATURES, NULL);

  if (value == default_font_features)
    return NULL;

  string = g_string_new ("");

  g_hash_table_iter_init (&iter, value->features);
  while (g_hash_table_iter_next (&iter, (gpointer *)&name, (gpointer *)&val))
    {
      if (first)
        first = FALSE;
      else
        g_string_append (string, ", ");
      g_string_append_printf (string, "%s %d", name, GPOINTER_TO_INT (val));
    }

  return g_string_free (string, FALSE);
}
