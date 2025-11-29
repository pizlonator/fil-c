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

#include "gtkcssstringvalueprivate.h"
#include "gtk/css/gtkcssserializerprivate.h"

#include <string.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  char *string;
};

static void
gtk_css_value_string_free (GtkCssValue *value)
{
  g_free (value->string);
  g_free (value);
}

static GtkCssValue *
gtk_css_value_string_compute (GtkCssValue      *value,
                              guint             property_id,
                              GtkStyleProvider *provider,
                              GtkCssStyle      *style,
                              GtkCssStyle      *parent_style)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_string_equal (const GtkCssValue *value1,
                            const GtkCssValue *value2)
{
  return g_strcmp0 (value1->string, value2->string) == 0;
}

static GtkCssValue *
gtk_css_value_string_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  return NULL;
}

static void
gtk_css_value_string_print (const GtkCssValue *value,
                            GString           *str)
{
  if (value->string == NULL)
    {
      g_string_append (str, "none");
      return;
    }

  gtk_css_print_string (str, value->string, FALSE);
}

static void
gtk_css_value_ident_print (const GtkCssValue *value,
                            GString           *str)
{
  char *string = value->string;
  gsize len;

  do {
    len = strcspn (string, "\"\n\r\f");
    g_string_append_len (str, string, len);
    string += len;
    switch (*string)
      {
      case '\0':
        goto out;
      case '\n':
        g_string_append (str, "\\A ");
        break;
      case '\r':
        g_string_append (str, "\\D ");
        break;
      case '\f':
        g_string_append (str, "\\C ");
        break;
      case '\"':
        g_string_append (str, "\\\"");
        break;
      case '\'':
        g_string_append (str, "\\'");
        break;
      case '\\':
        g_string_append (str, "\\\\");
        break;
      default:
        g_assert_not_reached ();
        break;
      }
    string++;
  } while (*string);

out:
  ;
}

static const GtkCssValueClass GTK_CSS_VALUE_STRING = {
  "GtkCssStringValue",
  gtk_css_value_string_free,
  gtk_css_value_string_compute,
  gtk_css_value_string_equal,
  gtk_css_value_string_transition,
  NULL,
  NULL,
  gtk_css_value_string_print
};

static const GtkCssValueClass GTK_CSS_VALUE_IDENT = {
  "GtkCssIdentValue",
  gtk_css_value_string_free,
  gtk_css_value_string_compute,
  gtk_css_value_string_equal,
  gtk_css_value_string_transition,
  NULL,
  NULL,
  gtk_css_value_ident_print
};

GtkCssValue *
_gtk_css_string_value_new (const char *string)
{
  return _gtk_css_string_value_new_take (g_strdup (string));
}

GtkCssValue *
_gtk_css_string_value_new_take (char *string)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_STRING);
  result->string = string;
  result->is_computed = TRUE;

  return result;
}

GtkCssValue *
_gtk_css_string_value_parse (GtkCssParser *parser)
{
  char *s;

  g_return_val_if_fail (parser != NULL, NULL);

  s = gtk_css_parser_consume_string (parser);
  if (s == NULL)
    return NULL;
  
  return _gtk_css_string_value_new_take (s);
}

const char *
_gtk_css_string_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_STRING, NULL);

  return value->string;
}

GtkCssValue *
_gtk_css_ident_value_new (const char *ident)
{
  return _gtk_css_ident_value_new_take (g_strdup (ident));
}

GtkCssValue *
_gtk_css_ident_value_new_take (char *ident)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_IDENT);
  result->string = ident;
  result->is_computed = TRUE;

  return result;
}

GtkCssValue *
_gtk_css_ident_value_try_parse (GtkCssParser *parser)
{
  char *ident;

  g_return_val_if_fail (parser != NULL, NULL);

  if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_IDENT))
    return NULL;

  ident = gtk_css_parser_consume_ident (parser);
  if (ident == NULL)
    {
      g_assert_not_reached ();
    }
  
  return _gtk_css_ident_value_new_take (ident);
}

const char *
_gtk_css_ident_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value != NULL, NULL);
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_IDENT, NULL);

  return value->string;
}

