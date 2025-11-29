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

#include "gtkcssdimensionvalueprivate.h"

GtkCssValue *
gtk_css_dimension_value_parse (GtkCssParser           *parser,
                               GtkCssNumberParseFlags  flags)
{
  static const struct {
    const char *name;
    GtkCssUnit unit;
    GtkCssNumberParseFlags required_flags;
  } units[] = {
    { "px",   GTK_CSS_PX,      GTK_CSS_PARSE_LENGTH },
    { "pt",   GTK_CSS_PT,      GTK_CSS_PARSE_LENGTH },
    { "em",   GTK_CSS_EM,      GTK_CSS_PARSE_LENGTH },
    { "ex",   GTK_CSS_EX,      GTK_CSS_PARSE_LENGTH },
    { "rem",  GTK_CSS_REM,     GTK_CSS_PARSE_LENGTH },
    { "pc",   GTK_CSS_PC,      GTK_CSS_PARSE_LENGTH },
    { "in",   GTK_CSS_IN,      GTK_CSS_PARSE_LENGTH },
    { "cm",   GTK_CSS_CM,      GTK_CSS_PARSE_LENGTH },
    { "mm",   GTK_CSS_MM,      GTK_CSS_PARSE_LENGTH },
    { "rad",  GTK_CSS_RAD,     GTK_CSS_PARSE_ANGLE  },
    { "deg",  GTK_CSS_DEG,     GTK_CSS_PARSE_ANGLE  },
    { "grad", GTK_CSS_GRAD,    GTK_CSS_PARSE_ANGLE  },
    { "turn", GTK_CSS_TURN,    GTK_CSS_PARSE_ANGLE  },
    { "s",    GTK_CSS_S,       GTK_CSS_PARSE_TIME   },
    { "ms",   GTK_CSS_MS,      GTK_CSS_PARSE_TIME   }
  };
  const GtkCssToken *token;
  GtkCssValue *result;
  GtkCssUnit unit;
  double number;

  token = gtk_css_parser_get_token (parser);

  /* Handle percentages first */
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
    {
      if (!(flags & GTK_CSS_PARSE_PERCENT))
        {
          gtk_css_parser_error_value (parser, "Percentages are not allowed here");
          return NULL;
        }
      number = token->number.number;
      unit = GTK_CSS_PERCENT;
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      number = token->number.number;
      if (number == 0.0)
        {
          if (flags & GTK_CSS_PARSE_NUMBER)
            unit = GTK_CSS_NUMBER;
          else if (flags & GTK_CSS_PARSE_LENGTH)
            unit = GTK_CSS_PX;
          else if (flags & GTK_CSS_PARSE_ANGLE)
            unit = GTK_CSS_DEG;
          else if (flags & GTK_CSS_PARSE_TIME)
            unit = GTK_CSS_S;
          else
            unit = GTK_CSS_PERCENT;
        }
      else if (flags & GTK_CSS_PARSE_NUMBER)
        {
          unit = GTK_CSS_NUMBER;
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "Unit is missing.");
          return NULL;
        }
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_DIMENSION) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_DIMENSION))
    {
      guint i;

      for (i = 0; i < G_N_ELEMENTS (units); i++)
        {
          if (flags & units[i].required_flags &&
              g_ascii_strcasecmp (token->dimension.dimension, units[i].name) == 0)
            break;
        }

      if (i >= G_N_ELEMENTS (units))
        {
          gtk_css_parser_error_syntax (parser, "'%s' is not a valid unit", token->dimension.dimension);
          return NULL;
        }

      unit = units[i].unit;
      number = token->dimension.value;
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Expected a number");
      return NULL;
    }

  if (flags & GTK_CSS_POSITIVE_ONLY &&
      number < 0)
    {
      gtk_css_parser_error_value (parser, "Negative values are not allowed");
      return NULL;
    }

  result = gtk_css_dimension_value_new (number, unit);
  gtk_css_parser_consume_token (parser);

  return result;
}
