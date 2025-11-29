/*
 * Copyright © 2014 Codethink Limited
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
 * Author: Ryan Lortie <desrt@desrt.ca>
 */

#pragma once

#include <gtk/gtkmenutrackeritemprivate.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkpopovermenu.h>

G_BEGIN_DECLS

#define GTK_TYPE_MENU_SECTION_BOX                           (gtk_menu_section_box_get_type ())
#define GTK_MENU_SECTION_BOX(inst)                          (G_TYPE_CHECK_INSTANCE_CAST ((inst),                     \
                                                             GTK_TYPE_MENU_SECTION_BOX, GtkMenuSectionBox))
#define GTK_MENU_SECTION_BOX_CLASS(class)                   (G_TYPE_CHECK_CLASS_CAST ((class),                       \
                                                             GTK_TYPE_MENU_SECTION_BOX, GtkMenuSectionBoxClass))
#define GTK_IS_MENU_SECTION_BOX(inst)                       (G_TYPE_CHECK_INSTANCE_TYPE ((inst),                     \
                                                             GTK_TYPE_MENU_SECTION_BOX))
#define GTK_IS_MENU_SECTION_BOX_CLASS(class)                (G_TYPE_CHECK_CLASS_TYPE ((class),                       \
                                                             GTK_TYPE_MENU_SECTION_BOX))
#define GTK_MENU_SECTION_BOX_GET_CLASS(inst)                (G_TYPE_INSTANCE_GET_CLASS ((inst),                      \
                                                             GTK_TYPE_MENU_SECTION_BOX, GtkMenuSectionBoxClass))

typedef struct _GtkMenuSectionBox                           GtkMenuSectionBox;

GType                   gtk_menu_section_box_get_type                   (void) G_GNUC_CONST;
void                    gtk_menu_section_box_new_toplevel               (GtkPopoverMenu      *popover,
                                                                         GMenuModel          *model,
                                                                         GtkPopoverMenuFlags  flags);

gboolean                gtk_menu_section_box_add_custom                 (GtkPopoverMenu *popover,
                                                                         GtkWidget      *child,
                                                                         const char     *id);

gboolean                gtk_menu_section_box_remove_custom              (GtkPopoverMenu *popover,
                                                                         GtkWidget      *child);

G_END_DECLS

