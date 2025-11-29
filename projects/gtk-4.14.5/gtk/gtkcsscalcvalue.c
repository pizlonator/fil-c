/* GTK - The GIMP Toolkit
 * Copyright © 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtkcsscalcvalueprivate.h"

#include <string.h>

GtkCssValue *   gtk_css_calc_value_parse_sum (GtkCssParser           *parser,
                                              GtkCssNumberParseFlags  flags);

static GtkCssValue *
gtk_css_calc_value_parse_value (GtkCssParser           *parser,
                                GtkCssNumberParseFlags  flags)
{
  if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_OPEN_PARENS))
    {
      GtkCssValue *result;

      gtk_css_parser_start_block (parser);

      result = gtk_css_calc_value_parse_sum (parser, flags);
      if (result == NULL)
        {
          gtk_css_parser_end_block (parser);
          return NULL;
        }

      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        {
          GtkCssLocation start = *gtk_css_parser_get_start_location (parser);
          gtk_css_parser_skip_until (parser, GTK_CSS_TOKEN_EOF);
          gtk_css_parser_error (parser,
                                GTK_CSS_PARSER_ERROR_SYNTAX,
                                &start,
                                gtk_css_parser_get_start_location (parser),
                                "Expected closing ')' in calc() subterm");
          gtk_css_value_unref (result);
          gtk_css_parser_end_block (parser);
          return NULL;
        }

      gtk_css_parser_end_block (parser);

      return result;
    }

  return _gtk_css_number_value_parse (parser, flags);
}

static gboolean
is_number (GtkCssValue *value)
{
  return gtk_css_number_value_get_dimension (value) == GTK_CSS_DIMENSION_NUMBER
      && !gtk_css_number_value_has_percent (value);
}

static GtkCssValue *
gtk_css_calc_value_parse_product (GtkCssParser           *parser,
                                  GtkCssNumberParseFlags  flags)
{
  GtkCssValue *result, *value, *temp;
  GtkCssNumberParseFlags actual_flags;
  GtkCssLocation start;

  actual_flags = flags | GTK_CSS_PARSE_NUMBER;
  gtk_css_parser_get_token (parser);
  start = *gtk_css_parser_get_start_location (parser);
  result = gtk_css_calc_value_parse_value (parser, actual_flags);
  if (result == NULL)
    return NULL;

  while (TRUE)
    {
      if (actual_flags != GTK_CSS_PARSE_NUMBER && !is_number (result))
        actual_flags = GTK_CSS_PARSE_NUMBER;

      if (gtk_css_parser_try_delim (parser, '*'))
        {
          value = gtk_css_calc_value_parse_product (parser, actual_flags);
          if (value == NULL)
            goto fail;
          if (is_number (value))
            temp = gtk_css_number_value_multiply (result, _gtk_css_number_value_get (value, 100));
          else
            temp = gtk_css_number_value_multiply (value, _gtk_css_number_value_get (result, 100));
          _gtk_css_value_unref (value);
          _gtk_css_value_unref (result);
          result = temp;
        }
      else if (gtk_css_parser_try_delim (parser, '/'))
        {
          value = gtk_css_calc_value_parse_product (parser, GTK_CSS_PARSE_NUMBER);
          if (value == NULL)
            goto fail;
          temp = gtk_css_number_value_multiply (result, 1.0 / _gtk_css_number_value_get (value, 100));
          _gtk_css_value_unref (value);
          _gtk_css_value_unref (result);
          result = temp;
        }
      else
        {
          break;
        }
    }

  if (is_number (result) && !(flags & GTK_CSS_PARSE_NUMBER))
    {
      gtk_css_parser_error (parser,
                            GTK_CSS_PARSER_ERROR_SYNTAX,
                            &start,
                            gtk_css_parser_get_start_location (parser),
                            "calc() product term has no units");
      goto fail;
    }

  return result;

fail:
  _gtk_css_value_unref (result);
  return NULL;
}

GtkCssValue *
gtk_css_calc_value_parse_sum (GtkCssParser           *parser,
                              GtkCssNumberParseFlags  flags)
{
  GtkCssValue *result;

  result = gtk_css_calc_value_parse_product (parser, flags);
  if (result == NULL)
    return NULL;

  while (TRUE)
    {
      GtkCssValue *next, *temp;

      if (gtk_css_parser_try_delim (parser, '+'))
        {
          next = gtk_css_calc_value_parse_product (parser, flags);
          if (next == NULL)
            goto fail;
        }
      else if (gtk_css_parser_try_delim (parser, '-'))
        {
          temp = gtk_css_calc_value_parse_product (parser, flags);
          if (temp == NULL)
            goto fail;
          next = gtk_css_number_value_multiply (temp, -1);
          _gtk_css_value_unref (temp);
        }
      else
        {
          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
              gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
              gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
              gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_SIGNED_DIMENSION))
            {
              gtk_css_parser_error_syntax (parser, "Unexpected signed number, did you forget a space between sign and number?");
              gtk_css_parser_consume_token (parser);
            }
          break;
        }

      temp = gtk_css_number_value_add (result, next);
      _gtk_css_value_unref (result);
      _gtk_css_value_unref (next);
      result = temp;
    }

  return result;

fail:
  _gtk_css_value_unref (result);
  return NULL;
}

typedef struct
{
  GtkCssNumberParseFlags flags;
  GtkCssValue *value;
} ParseCalcData;

static guint
gtk_css_calc_value_parse_arg (GtkCssParser *parser,
                              guint         arg,
                              gpointer      data_)
{
  ParseCalcData *data = data_;

  data->value = gtk_css_calc_value_parse_sum (parser, data->flags);
  if (data->value == NULL)
    return 0;

  return 1;
}

GtkCssValue *
gtk_css_calc_value_parse (GtkCssParser           *parser,
                          GtkCssNumberParseFlags  flags)
{
  ParseCalcData data;

  /* This can only be handled at compute time, we allow '-' after all */
  data.flags = flags & ~GTK_CSS_POSITIVE_ONLY;
  data.value = NULL;

  if (!gtk_css_parser_has_function (parser, "calc"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'calc('");
      return NULL;
    }

  if (!gtk_css_parser_consume_function (parser, 1, 1, gtk_css_calc_value_parse_arg, &data))
    return NULL;

  return data.value;
}

