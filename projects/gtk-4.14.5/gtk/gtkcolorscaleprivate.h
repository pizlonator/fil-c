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

#include <gtk/gtkscale.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_SCALE            (gtk_color_scale_get_type ())
#define GTK_COLOR_SCALE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COLOR_SCALE, GtkColorScale))
#define GTK_IS_COLOR_SCALE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_SCALE))


typedef struct _GtkColorScale GtkColorScale;

typedef enum
{
  GTK_COLOR_SCALE_HUE,
  GTK_COLOR_SCALE_ALPHA
} GtkColorScaleType;

GType       gtk_color_scale_get_type        (void) G_GNUC_CONST;
GtkWidget * gtk_color_scale_new             (GtkAdjustment     *adjustment,
                                             GtkColorScaleType  type);
void        gtk_color_scale_set_rgba        (GtkColorScale     *scale,
                                             const GdkRGBA     *color);

void        gtk_color_scale_snapshot_trough (GtkColorScale     *scale,
                                             GtkSnapshot       *snapshot,
                                             int                width,
                                             int                height);

G_END_DECLS

