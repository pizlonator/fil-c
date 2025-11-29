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

#include "gtklistheader.h"

#include "gtklistheaderwidgetprivate.h"

G_BEGIN_DECLS

struct _GtkListHeader
{
  GObject parent_instance;

  GtkListHeaderWidget *owner; /* has a reference */

  GtkWidget *child;
};

struct _GtkListHeaderClass
{
  GObjectClass parent_class;
};

GtkListHeader * gtk_list_header_new                             (void);

void            gtk_list_header_do_notify                       (GtkListHeader          *list_header,
                                                                 gboolean                notify_item,
                                                                 gboolean                notify_start,
                                                                 gboolean                notify_end,
                                                                 gboolean                notify_n_items);


G_END_DECLS

