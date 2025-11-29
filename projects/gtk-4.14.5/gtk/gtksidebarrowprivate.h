/* gtksidebarrowprivate.h
 *
 * Copyright (C) 2015 Carlos Soriano <csoriano@gnome.org>
 *
 * This file is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#pragma once

#include <glib.h>
#include "gtklistbox.h"

G_BEGIN_DECLS

#define GTK_TYPE_SIDEBAR_ROW             (gtk_sidebar_row_get_type())
#define GTK_SIDEBAR_ROW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_SIDEBAR_ROW, GtkSidebarRow))
#define GTK_SIDEBAR_ROW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_SIDEBAR_ROW, GtkSidebarRowClass))
#define GTK_IS_SIDEBAR_ROW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_SIDEBAR_ROW))
#define GTK_IS_SIDEBAR_ROW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_SIDEBAR_ROW))
#define GTK_SIDEBAR_ROW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_SIDEBAR_ROW, GtkSidebarRowClass))

typedef struct _GtkSidebarRow GtkSidebarRow;
typedef struct _GtkSidebarRowClass GtkSidebarRowClass;

struct _GtkSidebarRowClass
{
  GtkListBoxRowClass parent;
};

GType      gtk_sidebar_row_get_type   (void) G_GNUC_CONST;

GtkSidebarRow *gtk_sidebar_row_new    (void);
GtkSidebarRow *gtk_sidebar_row_clone  (GtkSidebarRow *self);

/* Use these methods instead of gtk_widget_hide/show to use an animation */
void           gtk_sidebar_row_hide   (GtkSidebarRow *self,
                                       gboolean       immediate);
void           gtk_sidebar_row_reveal (GtkSidebarRow *self);

GtkWidget     *gtk_sidebar_row_get_eject_button (GtkSidebarRow *self);
void           gtk_sidebar_row_set_start_icon   (GtkSidebarRow *self,
                                                 GIcon         *icon);
void           gtk_sidebar_row_set_end_icon     (GtkSidebarRow *self,
                                                 GIcon         *icon);
void           gtk_sidebar_row_set_busy         (GtkSidebarRow *row,
                                                 gboolean       is_busy);

G_END_DECLS

