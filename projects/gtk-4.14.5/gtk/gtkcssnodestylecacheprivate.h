/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssnodedeclarationprivate.h"
#include "gtkcssstyleprivate.h"

G_BEGIN_DECLS

typedef struct _GtkCssNodeStyleCache GtkCssNodeStyleCache;

GtkCssNodeStyleCache *  gtk_css_node_style_cache_new            (GtkCssStyle            *style);
GtkCssNodeStyleCache *  gtk_css_node_style_cache_ref            (GtkCssNodeStyleCache   *cache);
void                    gtk_css_node_style_cache_unref          (GtkCssNodeStyleCache   *cache);

GtkCssStyle *           gtk_css_node_style_cache_get_style      (GtkCssNodeStyleCache   *cache);

GtkCssNodeStyleCache *  gtk_css_node_style_cache_insert         (GtkCssNodeStyleCache   *parent,
                                                                 GtkCssNodeDeclaration  *decl,
                                                                 gboolean                is_first,
                                                                 gboolean                is_last,
                                                                 GtkCssStyle            *style);
GtkCssNodeStyleCache *  gtk_css_node_style_cache_lookup         (GtkCssNodeStyleCache        *parent,
                                                                 const GtkCssNodeDeclaration *decl,
                                                                 gboolean                     is_first,
                                                                 gboolean                     is_last);

G_END_DECLS

