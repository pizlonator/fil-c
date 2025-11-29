/* GTK - The GIMP Toolkit
 * Copyright (C) 2014,2015 Benjamin Otte
 * 
 * Authors: Benjamin Otte <otte@gnome.org>
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

#include <glib-object.h>
#include <cairo.h>
#include <gsk/gsk.h>

#include "gtkcsstypesprivate.h"
#include "gtktypes.h"

G_BEGIN_DECLS

void    gtk_css_style_snapshot_icon             (GtkCssStyle            *style,
                                                 GtkSnapshot            *snapshot,
                                                 double                  width,
                                                 double                  height);

void    gtk_css_style_snapshot_icon_paintable   (GtkCssStyle            *style,
                                                 GtkSnapshot            *snapshot,
                                                 GdkPaintable           *paintable,
                                                 double                  width,
                                                 double                  height);

void    gtk_css_style_render_icon_get_extents   (GtkCssStyle            *style,
                                                 GdkRectangle           *extents,
                                                 int                     x,
                                                 int                     y,
                                                 int                     width,
                                                 int                     height);

G_END_DECLS

