/*
 * Copyright © 2023 Benjamin Otte
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

#include "gtkcolumnviewrow.h"

#include "gtkcolumnviewrowwidgetprivate.h"

G_BEGIN_DECLS

struct _GtkColumnViewRow
{
  GObject parent_instance;

  GtkColumnViewRowWidget *owner; /* has a reference */

  char *accessible_label;
  char *accessible_description;

  guint activatable : 1;
  guint selectable : 1;
  guint focusable : 1;
};

GtkColumnViewRow *      gtk_column_view_row_new                         (void);

void                    gtk_column_view_row_do_notify                   (GtkColumnViewRow       *self,
                                                                         gboolean                notify_item,
                                                                         gboolean                notify_position,
                                                                         gboolean                notify_selected);


G_END_DECLS

