/*
 * Copyright © 2008, 2010 Christian Persch
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

#ifndef TERMINAL_SCREEN_CONTAINER_H
#define TERMINAL_SCREEN_CONTAINER_H

#include <gtk/gtk.h>
#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_SCREEN_CONTAINER         (terminal_screen_container_get_type ())
#define TERMINAL_SCREEN_CONTAINER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), TERMINAL_TYPE_SCREEN_CONTAINER, TerminalScreenContainer))
#define TERMINAL_SCREEN_CONTAINER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), TERMINAL_TYPE_SCREEN_CONTAINER, TerminalScreenContainerClass))
#define TERMINAL_IS_SCREEN_CONTAINER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), TERMINAL_TYPE_SCREEN_CONTAINER))
#define TERMINAL_IS_SCREEN_CONTAINER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), TERMINAL_TYPE_SCREEN_CONTAINER))
#define TERMINAL_SCREEN_CONTAINER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), TERMINAL_TYPE_SCREEN_CONTAINER, TerminalScreenContainerClass))

typedef struct _TerminalScreenContainer        TerminalScreenContainer;
typedef struct _TerminalScreenContainerClass   TerminalScreenContainerClass;
typedef struct _TerminalScreenContainerPrivate TerminalScreenContainerPrivate;

struct _TerminalScreenContainer
{
  GtkOverlay parent_instance;

  /*< private >*/
  TerminalScreenContainerPrivate *priv;
};

struct _TerminalScreenContainerClass
{
  GtkOverlayClass parent_class;
};

GType terminal_screen_container_get_type (void);

GtkWidget *terminal_screen_container_new (TerminalScreen *screen);

TerminalScreen *terminal_screen_container_get_screen (TerminalScreenContainer *container);

TerminalScreenContainer *terminal_screen_container_get_from_screen (TerminalScreen *screen);

void terminal_screen_container_set_policy (TerminalScreenContainer *container,
                                           GtkPolicyType hpolicy,
                                           GtkPolicyType vpolicy);

G_END_DECLS

#endif /* TERMINAL_SCREEN_CONTAINER_H */
