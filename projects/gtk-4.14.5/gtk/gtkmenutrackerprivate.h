/*
 * Copyright © 2013 Canonical Limited
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#pragma once

#include "gtkmenutrackeritemprivate.h"

typedef struct _GtkMenuTracker GtkMenuTracker;

typedef void         (* GtkMenuTrackerInsertFunc)                       (GtkMenuTrackerItem       *item,
                                                                         int                       position,
                                                                         gpointer                  user_data);

typedef void         (* GtkMenuTrackerRemoveFunc)                       (int                       position,
                                                                         gpointer                  user_data);


GtkMenuTracker *        gtk_menu_tracker_new                            (GtkActionObservable      *observer,
                                                                         GMenuModel               *model,
                                                                         gboolean                  with_separators,
                                                                         gboolean                  merge_sections,
                                                                         gboolean                  mac_os_mode,
                                                                         const char               *action_namespace,
                                                                         GtkMenuTrackerInsertFunc  insert_func,
                                                                         GtkMenuTrackerRemoveFunc  remove_func,
                                                                         gpointer                  user_data);

GtkMenuTracker *        gtk_menu_tracker_new_for_item_link              (GtkMenuTrackerItem       *item,
                                                                         const char               *link_name,
                                                                         gboolean                  merge_sections,
                                                                         gboolean                  mac_os_mode,
                                                                         GtkMenuTrackerInsertFunc  insert_func,
                                                                         GtkMenuTrackerRemoveFunc  remove_func,
                                                                         gpointer                  user_data);

void                    gtk_menu_tracker_free                           (GtkMenuTracker           *tracker);

