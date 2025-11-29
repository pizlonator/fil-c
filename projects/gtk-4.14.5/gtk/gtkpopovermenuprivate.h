/* GTK - The GIMP Toolkit
 * Copyright © 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkpopovermenu.h"

G_BEGIN_DECLS

GtkWidget *gtk_popover_menu_get_active_item  (GtkPopoverMenu *menu);
void       gtk_popover_menu_set_active_item  (GtkPopoverMenu *menu,
                                              GtkWidget      *item);
GtkWidget *gtk_popover_menu_get_open_submenu (GtkPopoverMenu *menu);
void       gtk_popover_menu_set_open_submenu (GtkPopoverMenu *menu,
                                              GtkWidget      *submenu);
void       gtk_popover_menu_close_submenus   (GtkPopoverMenu *menu);

GtkWidget *gtk_popover_menu_get_parent_menu  (GtkPopoverMenu *menu);
void       gtk_popover_menu_set_parent_menu  (GtkPopoverMenu *menu,
                                              GtkWidget      *parent);

GtkWidget * gtk_popover_menu_new (void);

void  gtk_popover_menu_add_submenu (GtkPopoverMenu *popover,
                                    GtkWidget      *submenu,
                                    const char     *name);
void gtk_popover_menu_open_submenu (GtkPopoverMenu *popover,
                                    const char     *name);

GtkWidget * gtk_popover_menu_get_stack (GtkPopoverMenu *menu);

G_END_DECLS

