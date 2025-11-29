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

#include <gtk/gtkdrawingarea.h>

G_BEGIN_DECLS

#define GTK_TYPE_COLOR_SWATCH                  (gtk_color_swatch_get_type ())
#define GTK_COLOR_SWATCH(obj)                  (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_COLOR_SWATCH, GtkColorSwatch))
#define GTK_IS_COLOR_SWATCH(obj)               (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_SWATCH))


typedef struct _GtkColorSwatch        GtkColorSwatch;

GType       gtk_color_swatch_get_type         (void) G_GNUC_CONST;
GtkWidget * gtk_color_swatch_new              (void);
void        gtk_color_swatch_set_rgba         (GtkColorSwatch *swatch,
                                               const GdkRGBA  *color);
gboolean    gtk_color_swatch_get_rgba         (GtkColorSwatch *swatch,
                                               GdkRGBA        *color);
void        gtk_color_swatch_set_hsva         (GtkColorSwatch *swatch,
                                               double          h,
                                               double          s,
                                               double          v,
                                               double          a);
void        gtk_color_swatch_set_can_drop     (GtkColorSwatch *swatch,
                                               gboolean        can_drop);
void        gtk_color_swatch_set_can_drag     (GtkColorSwatch *swatch,
                                               gboolean        can_drag);
void        gtk_color_swatch_set_icon         (GtkColorSwatch *swatch,
                                               const char     *icon);
void        gtk_color_swatch_set_use_alpha    (GtkColorSwatch *swatch,
                                               gboolean        use_alpha);
void        gtk_color_swatch_set_selectable   (GtkColorSwatch *swatch,
                                               gboolean        selectable);
gboolean    gtk_color_swatch_get_selectable   (GtkColorSwatch *swatch);

void        gtk_color_swatch_select    (GtkColorSwatch *swatch);
void        gtk_color_swatch_activate  (GtkColorSwatch *swatch);
void        gtk_color_swatch_customize (GtkColorSwatch *swatch);

G_END_DECLS

