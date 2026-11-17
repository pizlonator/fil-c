/*
 * Copyright © 2008, 2010, 2011, 2012 Christian Persch
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

#include <config.h>

#include "terminal-mdi-container.hh"
#include "terminal-debug.hh"
#include "terminal-intl.hh"

enum {
  SCREEN_ADDED,
  SCREEN_REMOVED,
  SCREEN_SWITCHED,
  SCREENS_REORDERED,
  SCREEN_CLOSE_REQUEST,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (TerminalMdiContainer, terminal_mdi_container, GTK_TYPE_WIDGET)

static void
terminal_mdi_container_default_init (TerminalMdiContainerInterface *iface)
{
  signals[SCREEN_ADDED] =
    g_signal_new (I_("screen-added"),
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalMdiContainerInterface, screen_added),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, TERMINAL_TYPE_SCREEN);

  signals[SCREEN_ADDED] =
    g_signal_new (I_("screen-removed"),
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalMdiContainerInterface, screen_added),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, TERMINAL_TYPE_SCREEN);

  signals[SCREEN_ADDED] =
    g_signal_new (I_("screen-switched"),
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalMdiContainerInterface, screen_switched),
                  nullptr, nullptr,
                  nullptr,
                  G_TYPE_NONE,
                  2, TERMINAL_TYPE_SCREEN, TERMINAL_TYPE_SCREEN);

  signals[SCREENS_REORDERED] =
    g_signal_new (I_("screens-reordered"),
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalMdiContainerInterface, screens_reordered),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE,
                  0);

  signals[SCREEN_CLOSE_REQUEST] =
    g_signal_new (I_("screen-close-request"),
                  G_TYPE_FROM_INTERFACE (iface),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (TerminalMdiContainerInterface, screen_close_request),
                  nullptr, nullptr,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE,
                  1, TERMINAL_TYPE_SCREEN);

  g_object_interface_install_property (iface,
    g_param_spec_object ("active-screen", nullptr, nullptr,
                         TERMINAL_TYPE_SCREEN,
                         GParamFlags(G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));
}

/* public API */

void 
terminal_mdi_container_add_screen (TerminalMdiContainer *container,
                                   TerminalScreen *screen,
                                   int position)
{
  g_return_if_fail (TERMINAL_IS_MDI_CONTAINER (container));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  TERMINAL_MDI_CONTAINER_GET_IFACE (container)->add_screen (container, screen, position);
}

void 
terminal_mdi_container_remove_screen (TerminalMdiContainer *container,
                                      TerminalScreen *screen)
{
  g_return_if_fail (TERMINAL_IS_MDI_CONTAINER (container));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  TERMINAL_MDI_CONTAINER_GET_IFACE (container)->remove_screen (container, screen);
}

TerminalScreen *
terminal_mdi_container_get_active_screen (TerminalMdiContainer *container)
{
  g_return_val_if_fail (TERMINAL_IS_MDI_CONTAINER (container), nullptr);

  return TERMINAL_MDI_CONTAINER_GET_IFACE (container)->get_active_screen (container);
}

void 
terminal_mdi_container_set_active_screen (TerminalMdiContainer *container,
                                          TerminalScreen *screen)
{
  g_return_if_fail (TERMINAL_IS_MDI_CONTAINER (container));
  g_return_if_fail (TERMINAL_IS_SCREEN (screen));

  TERMINAL_MDI_CONTAINER_GET_IFACE (container)->set_active_screen (container, screen);
}


GList *
terminal_mdi_container_list_screens (TerminalMdiContainer *container)
{
  g_return_val_if_fail (TERMINAL_IS_MDI_CONTAINER (container), nullptr);

  return TERMINAL_MDI_CONTAINER_GET_IFACE (container)->list_screens (container);
}

GList *
terminal_mdi_container_list_screen_containers (TerminalMdiContainer *container)
{
  g_return_val_if_fail (TERMINAL_IS_MDI_CONTAINER (container), nullptr);

  return TERMINAL_MDI_CONTAINER_GET_IFACE (container)->list_screen_containers (container);
}

int
terminal_mdi_container_get_n_screens (TerminalMdiContainer *container)
{
  g_return_val_if_fail (TERMINAL_IS_MDI_CONTAINER (container), 0);

  return TERMINAL_MDI_CONTAINER_GET_IFACE (container)->get_n_screens (container);
}

int
terminal_mdi_container_get_active_screen_num (TerminalMdiContainer *container)
{
  g_return_val_if_fail (TERMINAL_IS_MDI_CONTAINER (container), -1);

  return TERMINAL_MDI_CONTAINER_GET_IFACE (container)->get_active_screen_num (container);
}

void
terminal_mdi_container_set_active_screen_num (TerminalMdiContainer *container,
                                              int position)
{
  g_return_if_fail (TERMINAL_IS_MDI_CONTAINER (container));

  TERMINAL_MDI_CONTAINER_GET_IFACE (container)->set_active_screen_num (container, position);
}

void
terminal_mdi_container_reorder_screen (TerminalMdiContainer *container,
                                       TerminalScreen *screen,
                                       int new_position)
{
  g_return_if_fail (TERMINAL_IS_MDI_CONTAINER (container));

  return TERMINAL_MDI_CONTAINER_GET_IFACE (container)->reorder_screen (container, screen, new_position);
}

void
terminal_mdi_container_change_screen (TerminalMdiContainer *container,
                                      int change)
{
  int active, n;

  g_return_if_fail (TERMINAL_IS_MDI_CONTAINER (container));
  g_return_if_fail (change == -1 || change == 1);

  n = terminal_mdi_container_get_n_screens (container);
  active = terminal_mdi_container_get_active_screen_num (container);

  active += change;
  if (active < 0)
    active = n - 1;
  else if (active >= n)
    active = 0;

  terminal_mdi_container_set_active_screen_num (container, active);
}
