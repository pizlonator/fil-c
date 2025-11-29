/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cairo.h>
#include <pango/pango.h>

#include <gtk/css/gtkcss.h>
#include "gtk/css/gtkcsstokenizerprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtkborder.h"
#include "gtktypes.h"
#include "gtkcssvalueprivate.h"
#include "gtksnapshot.h"

G_BEGIN_DECLS

GtkCssValue *   gtk_css_shadow_value_new_none         (void);
GtkCssValue *   gtk_css_shadow_value_new_filter       (const GtkCssValue        *other);

GtkCssValue *   gtk_css_shadow_value_parse            (GtkCssParser             *parser,
                                                       gboolean                  box_shadow_mode);
GtkCssValue *   gtk_css_shadow_value_parse_filter     (GtkCssParser             *parser);

void            gtk_css_shadow_value_get_extents      (const GtkCssValue        *shadow,
                                                       GtkBorder                *border);
void            gtk_css_shadow_value_snapshot_outset  (const GtkCssValue        *shadow,
                                                       GtkSnapshot              *snapshot,
                                                       const GskRoundedRect     *border_box);
void            gtk_css_shadow_value_snapshot_inset   (const GtkCssValue        *shadow,
                                                       GtkSnapshot              *snapshot,
                                                       const GskRoundedRect     *padding_box);

gboolean        gtk_css_shadow_value_is_clear         (const GtkCssValue        *shadow) G_GNUC_PURE;
gboolean        gtk_css_shadow_value_is_none          (const GtkCssValue        *shadow) G_GNUC_PURE;

gboolean        gtk_css_shadow_value_push_snapshot    (const GtkCssValue        *value,
                                                       GtkSnapshot              *snapshot);
void            gtk_css_shadow_value_pop_snapshot     (const GtkCssValue        *value,
                                                       GtkSnapshot              *snapshot);

G_END_DECLS

