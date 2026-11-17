/*
 *  Copyright © 2008, 2017 Christian Persch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANMENUILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TERMINAL_MENU_BUTTON_H
#define TERMINAL_MENU_BUTTON_H

#include <gtk/gtk.h>

#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_MENU_BUTTON         (terminal_menu_button_get_type ())
#define TERMINAL_MENU_BUTTON(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_MENU_BUTTON, TerminalMenuButton))
#define TERMINAL_MENU_BUTTON_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_MENU_BUTTON, TerminalMenuButtonClass))
#define TERMINAL_IS_MENU_BUTTON(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_MENU_BUTTON))
#define TERMINAL_IS_MENU_BUTTON_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_MENU_BUTTON))
#define TERMINAL_MENU_BUTTON_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_MENU_BUTTON, TerminalMenuButtonClass))

typedef struct _TerminalMenuButton        TerminalMenuButton;
typedef struct _TerminalMenuButtonClass   TerminalMenuButtonClass;
typedef struct _TerminalMenuButtonPrivate TerminalMenuButtonPrivate;

struct _TerminalMenuButton
{
  GtkMenuButton parent_instance;
};

struct _TerminalMenuButtonClass
{
  GtkMenuButtonClass parent_class;

  /* Signals */
  void (* update_menu) (TerminalMenuButton *menu_button);
};

GType      terminal_menu_button_get_type (void);

GtkWidget *terminal_menu_button_new      (void);

G_END_DECLS

#endif /* !TERMINAL_MENU_BUTTON_H */
