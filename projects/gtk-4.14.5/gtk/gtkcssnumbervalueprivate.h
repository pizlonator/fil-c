/*
 * Copyright © 2012 Red Hat Inc.
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
 * Authors: Alexander Larsson <alexl@gnome.org>
 */

#pragma once

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkcsstypesprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

typedef enum /*< skip >*/ {
  GTK_CSS_POSITIVE_ONLY = (1 << 0),
  GTK_CSS_PARSE_PERCENT = (1 << 1),
  GTK_CSS_PARSE_NUMBER = (1 << 2),
  GTK_CSS_PARSE_LENGTH = (1 << 3),
  GTK_CSS_PARSE_ANGLE = (1 << 4),
  GTK_CSS_PARSE_TIME = (1 << 5)
} GtkCssNumberParseFlags;

GtkCssValue *   gtk_css_dimension_value_new         (double                  value,
                                                     GtkCssUnit              unit);

GtkCssValue *   _gtk_css_number_value_new           (double                  value,
                                                     GtkCssUnit              unit);
gboolean        gtk_css_number_value_can_parse      (GtkCssParser           *parser);
GtkCssValue *   _gtk_css_number_value_parse         (GtkCssParser           *parser,
                                                     GtkCssNumberParseFlags  flags);

GtkCssDimension gtk_css_number_value_get_dimension  (const GtkCssValue      *value) G_GNUC_PURE;
gboolean        gtk_css_number_value_has_percent    (const GtkCssValue      *value) G_GNUC_PURE;
GtkCssValue *   gtk_css_number_value_multiply       (GtkCssValue            *value,
                                                     double                  factor);
GtkCssValue *   gtk_css_number_value_add            (GtkCssValue            *value1,
                                                     GtkCssValue            *value2);
GtkCssValue *   gtk_css_number_value_try_add        (GtkCssValue            *value1,
                                                     GtkCssValue            *value2);
double          _gtk_css_number_value_get           (const GtkCssValue      *number,
                                                     double                  one_hundred_percent) G_GNUC_PURE;

gboolean        gtk_css_dimension_value_is_zero     (const GtkCssValue      *value) G_GNUC_PURE;

G_END_DECLS

