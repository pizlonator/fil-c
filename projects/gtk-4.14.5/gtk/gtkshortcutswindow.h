/* gtkshortcutswindow.h
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gtk/gtkwindow.h>
#include <gtk/gtkshortcutssection.h>

G_BEGIN_DECLS

#define GTK_TYPE_SHORTCUTS_WINDOW            (gtk_shortcuts_window_get_type ())
#define GTK_SHORTCUTS_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SHORTCUTS_WINDOW, GtkShortcutsWindow))
#define GTK_IS_SHORTCUTS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SHORTCUTS_WINDOW))

typedef struct _GtkShortcutsWindow GtkShortcutsWindow;

GDK_AVAILABLE_IN_ALL
GType gtk_shortcuts_window_get_type (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_14
void gtk_shortcuts_window_add_section (GtkShortcutsWindow  *self,
                                       GtkShortcutsSection *section);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GtkShortcutsWindow, g_object_unref)

G_END_DECLS

