/*
 * Copyright © 2001 Havoc Pennington
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

#ifndef TERMINAL_WINDOW_H
#define TERMINAL_WINDOW_H

#include <gtk/gtk.h>

#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_WINDOW              (terminal_window_get_type ())
#define TERMINAL_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), TERMINAL_TYPE_WINDOW, TerminalWindow))
#define TERMINAL_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), TERMINAL_TYPE_WINDOW, TerminalWindowClass))
#define TERMINAL_IS_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), TERMINAL_TYPE_WINDOW))
#define TERMINAL_IS_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), TERMINAL_TYPE_WINDOW))
#define TERMINAL_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), TERMINAL_TYPE_WINDOW, TerminalWindowClass))

typedef struct _TerminalWindowClass   TerminalWindowClass;
typedef struct _TerminalWindowPrivate TerminalWindowPrivate;

struct _TerminalWindow
{
  GtkApplicationWindow parent_instance;

  TerminalWindowPrivate *priv;
};

struct _TerminalWindowClass
{
  GtkApplicationWindowClass parent_class;
};

GType terminal_window_get_type (void) G_GNUC_CONST;

TerminalWindow* terminal_window_new (GApplication *app);

GtkUIManager *terminal_window_get_ui_manager (TerminalWindow *window);

int terminal_window_get_active_screen_num (TerminalWindow *window);

void terminal_window_add_screen (TerminalWindow *window,
                                 TerminalScreen *screen,
                                 int position);

void terminal_window_remove_screen (TerminalWindow *window,
                                    TerminalScreen *screen);

void terminal_window_move_screen (TerminalWindow *source_window,
                                  TerminalWindow *dest_window,
                                  TerminalScreen *screen,
                                  int dest_position);

/* Menubar visibility is part of session state, except that
 * if it isn't restored from session, the window gets the setting
 * from the profile of the first screen added to the window
 */
void terminal_window_set_menubar_visible     (TerminalWindow *window,
                                              gboolean        setting);

void terminal_window_update_size (TerminalWindow *window);

void            terminal_window_switch_screen (TerminalWindow *window,
                                               TerminalScreen *screen);
TerminalScreen* terminal_window_get_active (TerminalWindow *window);

GList* terminal_window_list_screen_containers (TerminalWindow *window);

gboolean terminal_window_parse_geometry (TerminalWindow *window,
					 const char     *geometry);

void terminal_window_update_geometry  (TerminalWindow *window);

GtkWidget* terminal_window_get_mdi_container (TerminalWindow *window);

void terminal_window_request_close (TerminalWindow *window);

const char *terminal_window_get_uuid (TerminalWindow *window);

G_END_DECLS

#endif /* TERMINAL_WINDOW_H */
