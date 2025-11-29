/* gtkconstraintvflparserprivate.h: VFL constraint definition parser 
 *
 * Copyright 2017  Endless
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkconstrainttypesprivate.h"

G_BEGIN_DECLS

typedef struct _GtkConstraintVflParser       GtkConstraintVflParser;

typedef struct {
  const char *view1;
  const char *attr1;
  GtkConstraintRelation relation;
  const char *view2;
  const char *attr2;
  double constant;
  double multiplier;
  double strength;
} GtkConstraintVfl;

GtkConstraintVflParser *
gtk_constraint_vfl_parser_new (void);

void
gtk_constraint_vfl_parser_free (GtkConstraintVflParser *parser);

void
gtk_constraint_vfl_parser_set_default_spacing (GtkConstraintVflParser *parser,
                                               int hspacing,
                                               int vspacing);

void
gtk_constraint_vfl_parser_set_metrics (GtkConstraintVflParser *parser,
                                       GHashTable *metrics);

void
gtk_constraint_vfl_parser_set_views (GtkConstraintVflParser *parser,
                                     GHashTable *views);

gboolean
gtk_constraint_vfl_parser_parse_line (GtkConstraintVflParser *parser,
                                      const char *line,
                                      gssize len,
                                      GError **error);

int
gtk_constraint_vfl_parser_get_error_offset (GtkConstraintVflParser *parser);

int
gtk_constraint_vfl_parser_get_error_range (GtkConstraintVflParser *parser);

GtkConstraintVfl *
gtk_constraint_vfl_parser_get_constraints (GtkConstraintVflParser *parser,
                                           int *n_constraints);

G_END_DECLS
