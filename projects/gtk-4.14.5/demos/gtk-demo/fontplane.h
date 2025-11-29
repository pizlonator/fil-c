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

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_FONT_PLANE            (gtk_font_plane_get_type ())
#define GTK_FONT_PLANE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_FONT_PLANE, GtkFontPlane))
#define GTK_FONT_PLANE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_FONT_PLANE, GtkFontPlaneClass))
#define GTK_IS_FONT_PLANE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_COLOR_PLANE))
#define GTK_IS_FONT_PLANE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_COLOR_PLANE))
#define GTK_FONT_PLANE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_FONT_PLANE, GtkFontPlaneClass))


typedef struct _GtkFontPlane         GtkFontPlane;
typedef struct _GtkFontPlaneClass    GtkFontPlaneClass;

struct _GtkFontPlane
{
  GtkWidget parent_instance;

  GtkAdjustment *weight_adj;
  GtkAdjustment *width_adj;

  GtkGesture *drag_gesture;
};

struct _GtkFontPlaneClass
{
  GtkWidgetClass parent_class;

  /* Padding for future expansion */
  void (*_gtk_reserved1) (void);
  void (*_gtk_reserved2) (void);
  void (*_gtk_reserved3) (void);
  void (*_gtk_reserved4) (void);
};


GType       gtk_font_plane_get_type (void) G_GNUC_CONST;
GtkWidget * gtk_font_plane_new      (GtkAdjustment *width_adj,
                                     GtkAdjustment *weight_adj);

G_END_DECLS
