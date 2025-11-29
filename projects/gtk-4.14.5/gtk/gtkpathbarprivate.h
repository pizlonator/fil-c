/* gtkpathbarprivate.h
 * Copyright (C) 2004  Red Hat, Inc.,  Jonathan Blandford <jrb@gnome.org>
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

#include "gtkwidget.h"

G_BEGIN_DECLS

#define GTK_TYPE_PATH_BAR    (gtk_path_bar_get_type ())
#define GTK_PATH_BAR(obj)    (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PATH_BAR, GtkPathBar))
#define GTK_IS_PATH_BAR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PATH_BAR))

typedef struct _GtkPathBar GtkPathBar;

GDK_AVAILABLE_IN_ALL
GType    gtk_path_bar_get_type (void) G_GNUC_CONST;
void     _gtk_path_bar_set_file        (GtkPathBar         *path_bar,
                                        GFile              *file,
                                        gboolean            keep_trail);
void     _gtk_path_bar_up              (GtkPathBar *path_bar);
void     _gtk_path_bar_down            (GtkPathBar *path_bar);

G_END_DECLS

