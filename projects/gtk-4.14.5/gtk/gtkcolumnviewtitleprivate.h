/*
 * Copyright © 2019 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#pragma once

#include "gtkcolumnviewcolumn.h"

G_BEGIN_DECLS

#define GTK_TYPE_COLUMN_VIEW_TITLE         (gtk_column_view_title_get_type ())
#define GTK_COLUMN_VIEW_TITLE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_COLUMN_VIEW_TITLE, GtkColumnViewTitle))
#define GTK_COLUMN_VIEW_TITLE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GTK_TYPE_COLUMN_VIEW_TITLE, GtkColumnViewTitleClass))
#define GTK_IS_COLUMN_VIEW_TITLE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_COLUMN_VIEW_TITLE))
#define GTK_IS_COLUMN_VIEW_TITLE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GTK_TYPE_COLUMN_VIEW_TITLE))
#define GTK_COLUMN_VIEW_TITLE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GTK_TYPE_COLUMN_VIEW_TITLE, GtkColumnViewTitleClass))

typedef struct _GtkColumnViewTitle GtkColumnViewTitle;
typedef struct _GtkColumnViewTitleClass GtkColumnViewTitleClass;

GType                   gtk_column_view_title_get_type          (void) G_GNUC_CONST;

GtkWidget *             gtk_column_view_title_new               (GtkColumnViewColumn    *column);

void                    gtk_column_view_title_set_title         (GtkColumnViewTitle     *self,
                                                                 const char             *title);
void                    gtk_column_view_title_set_menu          (GtkColumnViewTitle     *self,
                                                                 GMenuModel             *model);

void                    gtk_column_view_title_update_sort       (GtkColumnViewTitle     *self);

GtkColumnViewColumn *   gtk_column_view_title_get_column        (GtkColumnViewTitle     *self);

G_END_DECLS

