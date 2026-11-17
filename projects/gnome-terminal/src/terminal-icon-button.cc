/*
 * terminal-icon-button.c
 *
 * Copyright © 2010 - Paolo Borelli
 * Copyright © 2011 - Ignacio Casal Quinteiro
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "terminal-icon-button.hh"
#include "terminal-libgsystem.hh"

GtkWidget *
terminal_icon_button_new (const char *gicon_name)
{
  GtkWidget *button, *image;
  gs_unref_object GIcon *icon;

  button = (GtkWidget *) g_object_new (GTK_TYPE_BUTTON,
                                       "relief", GTK_RELIEF_NONE,
                                       "focus-on-click", FALSE,
                                       nullptr);

  icon = g_themed_icon_new_with_default_fallbacks (gicon_name);
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_MENU);

  gtk_widget_show (image);
  gtk_container_add (GTK_CONTAINER (button), image);

  return button;
}

GtkWidget *
terminal_close_button_new (void)
{
  return terminal_icon_button_new ("window-close-symbolic");
}
