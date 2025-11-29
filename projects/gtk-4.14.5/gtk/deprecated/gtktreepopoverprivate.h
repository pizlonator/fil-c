/*
 * Copyright © 2019 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Matthias Clasen
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GTK_TYPE_TREE_POPOVER         (gtk_tree_popover_get_type ())
G_DECLARE_FINAL_TYPE (GtkTreePopover, gtk_tree_popover, GTK, TREE_POPOVER, GtkPopover)

void gtk_tree_popover_set_model              (GtkTreePopover              *popover,
                                              GtkTreeModel                *model);
void gtk_tree_popover_set_row_separator_func (GtkTreePopover              *popover,
                                              GtkTreeViewRowSeparatorFunc  func,
                                              gpointer                     data,
                                              GDestroyNotify               destroy);
void gtk_tree_popover_set_active             (GtkTreePopover              *popover,
                                              int                          item);
void gtk_tree_popover_open_submenu           (GtkTreePopover              *popover,
                                              const char                  *name);

G_END_DECLS

