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

#include "gtkcolumnviewcell.h"

#include "gtkcolumnviewcellwidgetprivate.h"
#include "gtklistitemprivate.h"

G_BEGIN_DECLS

struct _GtkColumnViewCell
{
  GtkListItem parent_instance;

  GtkColumnViewCellWidget *cell; /* has a reference */

  GtkWidget *child;

  guint focusable : 1;
};

GtkColumnViewCell *     gtk_column_view_cell_new                        (void);

void                    gtk_column_view_cell_do_notify                  (GtkColumnViewCell *column_view_cell,
                                                                         gboolean notify_item,
                                                                         gboolean notify_position,
                                                                         gboolean notify_selected);


G_END_DECLS

