/* GTK - The GIMP Toolkit
 *
 * Copyright (C) 2023 Red Hat, Inc.
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
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_GRAPHICS_OFFLOAD (gtk_graphics_offload_get_type ())

GDK_AVAILABLE_IN_4_14
G_DECLARE_FINAL_TYPE (GtkGraphicsOffload, gtk_graphics_offload, GTK, GRAPHICS_OFFLOAD, GtkWidget)

GDK_AVAILABLE_IN_4_14
GtkWidget *       gtk_graphics_offload_new          (GtkWidget                 *child);

GDK_AVAILABLE_IN_4_14
void              gtk_graphics_offload_set_child    (GtkGraphicsOffload        *self,
                                                     GtkWidget                 *child);

GDK_AVAILABLE_IN_4_14
GtkWidget *       gtk_graphics_offload_get_child    (GtkGraphicsOffload        *self);

/**
 * GtkGraphicsOffloadEnabled:
 * @GTK_GRAPHICS_OFFLOAD_ENABLED: Graphics offloading is enabled.
 * @GTK_GRAPHICS_OFFLOAD_DISABLED: Graphics offloading is disabled.
 *
 * Represents the state of graphics offlodading.
 *
 * Since: 4.14
 */
typedef enum
{
  GTK_GRAPHICS_OFFLOAD_ENABLED,
  GTK_GRAPHICS_OFFLOAD_DISABLED,
} GtkGraphicsOffloadEnabled;

GDK_AVAILABLE_IN_4_14
void             gtk_graphics_offload_set_enabled (GtkGraphicsOffload        *self,
                                                   GtkGraphicsOffloadEnabled  enabled);

GDK_AVAILABLE_IN_4_14
GtkGraphicsOffloadEnabled
                 gtk_graphics_offload_get_enabled  (GtkGraphicsOffload        *self);

G_END_DECLS
