/*
 * terminal-close-button.h
 *
 * Copyright © 2010 - Paolo Borelli
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

#ifndef __TERMINAL_ICON_BUTTON_H__
#define __TERMINAL_ICON_BUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

GtkWidget *terminal_icon_button_new (const char *gicon_name);

GtkWidget *terminal_close_button_new (void);

G_END_DECLS

#endif /* __TERMINAL_ICON_BUTTON_H__ */
