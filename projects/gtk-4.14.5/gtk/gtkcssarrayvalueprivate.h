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
#include "gtkcssvalueprivate.h"
#include "gtktypes.h"

G_BEGIN_DECLS

GtkCssValue *       _gtk_css_array_value_new            (GtkCssValue           *content);
GtkCssValue *       _gtk_css_array_value_new_from_array (GtkCssValue          **values,
                                                         guint                  n_values);
GtkCssValue *       _gtk_css_array_value_parse          (GtkCssParser          *parser,
                                                         GtkCssValue *          (* parse_func) (GtkCssParser *));

GtkCssValue *       _gtk_css_array_value_get_nth        (GtkCssValue           *value,
                                                         guint                  i);
guint               _gtk_css_array_value_get_n_values   (const GtkCssValue     *value) G_GNUC_PURE;


G_END_DECLS

