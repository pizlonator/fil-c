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

GtkCssValue *   _gtk_css_ident_value_new            (const char             *ident);
GtkCssValue *   _gtk_css_ident_value_new_take       (char                   *ident);
GtkCssValue *   _gtk_css_ident_value_try_parse      (GtkCssParser           *parser);

const char *    _gtk_css_ident_value_get            (const GtkCssValue      *ident);

GtkCssValue *   _gtk_css_string_value_new           (const char             *string);
GtkCssValue *   _gtk_css_string_value_new_take      (char                   *string);
GtkCssValue *   _gtk_css_string_value_parse         (GtkCssParser           *parser);

const char *    _gtk_css_string_value_get           (const GtkCssValue      *string);


G_END_DECLS

