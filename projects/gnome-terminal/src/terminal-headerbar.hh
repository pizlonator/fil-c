/*
 *  Copyright © 2018 Christian Persch
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

#pragma once

#include <gtk/gtk.h>

#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_HEADERBAR         (terminal_headerbar_get_type ())
#define TERMINAL_HEADERBAR(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_HEADERBAR, TerminalHeaderbar))
#define TERMINAL_HEADERBAR_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_HEADERBAR, TerminalHeaderbarClass))
#define TERMINAL_IS_HEADERBAR(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_HEADERBAR))
#define TERMINAL_IS_HEADERBAR_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_HEADERBAR))
#define TERMINAL_HEADERBAR_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_HEADERBAR, TerminalHeaderbarClass))

typedef struct _TerminalHeaderbar        TerminalHeaderbar;
typedef struct _TerminalHeaderbarClass   TerminalHeaderbarClass;

GType      terminal_headerbar_get_type (void);

GtkWidget *terminal_headerbar_new      (void);

G_END_DECLS
