/*
 * Copyright © 2015 Endless Mobile, Inc.
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
 * Authors: Cosimo Cecchi <cosimoc@gnome.org>
 */

#pragma once

#include "gtkwidget.h"
#include "gtkcsstypesprivate.h"

G_BEGIN_DECLS

#define GTK_TYPE_BUILTIN_ICON (gtk_builtin_icon_get_type ())

G_DECLARE_FINAL_TYPE (GtkBuiltinIcon, gtk_builtin_icon, GTK, BUILTIN_ICON, GtkWidget)

GtkWidget *  gtk_builtin_icon_new          (const char     *css_name);
void         gtk_builtin_icon_set_css_name (GtkBuiltinIcon *self,
                                            const char     *css_name);

G_END_DECLS

