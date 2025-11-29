/*
 * Copyright © 2018 Benjamin Otte
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

#include "gtklistitem.h"

#include "gtklistitemwidgetprivate.h"
#include "gtkcolumnviewcellwidgetprivate.h"
#include "gtkversion.h"

G_BEGIN_DECLS

struct _GtkListItem
{
  GObject parent_instance;

  GtkListItemWidget *owner; /* has a reference */

  GtkWidget *child;

  char *accessible_label;
  char *accessible_description;

  guint activatable : 1;
  guint selectable : 1;
  guint focusable : 1;
#if !GTK_CHECK_VERSION (5, 0, 0)
  guint focusable_set : 1;
#endif
};

struct _GtkListItemClass
{
  GObjectClass parent_class;
};

GtkListItem *   gtk_list_item_new                               (void);

void            gtk_list_item_do_notify                         (GtkListItem *list_item,
                                                                 gboolean notify_item,
                                                                 gboolean notify_position,
                                                                 gboolean notify_selected);


G_END_DECLS

