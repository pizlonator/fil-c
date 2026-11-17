/*
 *  Copyright © 2008 Christian Persch
 *
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

#ifndef TERMINAL_SEARCH_POPOVER_H
#define TERMINAL_SEARCH_POPOVER_H

#include <gtk/gtk.h>

#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_SEARCH_POPOVER         (terminal_search_popover_get_type ())
#define TERMINAL_SEARCH_POPOVER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_SEARCH_POPOVER, TerminalSearchPopover))
#define TERMINAL_SEARCH_POPOVER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_SEARCH_POPOVER, TerminalSearchPopoverClass))
#define TERMINAL_IS_SEARCH_POPOVER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_SEARCH_POPOVER))
#define TERMINAL_IS_SEARCH_POPOVER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_SEARCH_POPOVER))
#define TERMINAL_SEARCH_POPOVER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_SEARCH_POPOVER, TerminalSearchPopoverClass))

typedef struct _TerminalSearchPopover        TerminalSearchPopover;
typedef struct _TerminalSearchPopoverClass   TerminalSearchPopoverClass;

GType terminal_search_popover_get_type (void);

TerminalSearchPopover *terminal_search_popover_new (GtkWidget *relative_to_widget);

VteRegex *
          terminal_search_popover_get_regex (TerminalSearchPopover *popover);

gboolean terminal_search_popover_get_wrap_around (TerminalSearchPopover *popover);

G_END_DECLS

#endif /* !TERMINAL_SEARCH_POPOVER_H */
