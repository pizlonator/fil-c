/* GTK - The GIMP Toolkit
 * Copyright (C) 2012 Red Hat, Inc.
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

#include <gtk/gtktypes.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_PLANE            (gtk_color_plane_get_type ())
#define GTK_COLOR_PLANE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COLOR_PLANE, GtkColorPlane))
#define GTK_IS_COLOR_PLANE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_PLANE))


typedef struct _GtkColorPlane         GtkColorPlane;


GType       gtk_color_plane_get_type (void) G_GNUC_CONST;
GtkWidget * gtk_color_plane_new      (GtkAdjustment *h_adj,
                                      GtkAdjustment *s_adj,
                                      GtkAdjustment *v_adj);

G_END_DECLS

