/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#pragma once

#include "gtkstylecontext.h"

#include "gtkcssnodeprivate.h"
#include "gtkstyleproviderprivate.h"
#include "gtkcssvalueprivate.h"

G_BEGIN_DECLS

GtkStyleContext *gtk_style_context_new_for_node              (GtkCssNode      *node);

GtkCssNode     *gtk_style_context_get_node                   (GtkStyleContext *context);
GtkStyleProvider *
                gtk_style_context_get_style_provider         (GtkStyleContext *context);

void            gtk_style_context_save_to_node               (GtkStyleContext *context,
                                                              GtkCssNode      *node);

GtkCssStyle *   gtk_style_context_lookup_style               (GtkStyleContext *context);
GtkCssValue   * _gtk_style_context_peek_property             (GtkStyleContext *context,
                                                              guint            property_id);
void           _gtk_style_context_get_cursor_color           (GtkStyleContext    *context,
                                                              GdkRGBA            *primary_color,
                                                              GdkRGBA            *secondary_color);

G_END_DECLS

