/* gtkcellrenderertoggle.h
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/deprecated/gtkcellrenderer.h>


G_BEGIN_DECLS


#define GTK_TYPE_CELL_RENDERER_TOGGLE			(gtk_cell_renderer_toggle_get_type ())
#define GTK_CELL_RENDERER_TOGGLE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_CELL_RENDERER_TOGGLE, GtkCellRendererToggle))
#define GTK_IS_CELL_RENDERER_TOGGLE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_CELL_RENDERER_TOGGLE))

typedef struct _GtkCellRendererToggle              GtkCellRendererToggle;


GDK_AVAILABLE_IN_ALL
GType            gtk_cell_renderer_toggle_get_type       (void) G_GNUC_CONST;
GDK_DEPRECATED_IN_4_10
GtkCellRenderer *gtk_cell_renderer_toggle_new            (void);

GDK_DEPRECATED_IN_4_10
gboolean         gtk_cell_renderer_toggle_get_radio      (GtkCellRendererToggle *toggle);
GDK_DEPRECATED_IN_4_10
void             gtk_cell_renderer_toggle_set_radio      (GtkCellRendererToggle *toggle,
                                                          gboolean               radio);

GDK_DEPRECATED_IN_4_10
gboolean        gtk_cell_renderer_toggle_get_active      (GtkCellRendererToggle *toggle);
GDK_DEPRECATED_IN_4_10
void            gtk_cell_renderer_toggle_set_active      (GtkCellRendererToggle *toggle,
                                                          gboolean               setting);

GDK_DEPRECATED_IN_4_10
gboolean        gtk_cell_renderer_toggle_get_activatable (GtkCellRendererToggle *toggle);
GDK_DEPRECATED_IN_4_10
void            gtk_cell_renderer_toggle_set_activatable (GtkCellRendererToggle *toggle,
                                                          gboolean               setting);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkCellRendererToggle, g_object_unref)

G_END_DECLS

