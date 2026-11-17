/*
 * Copyright © 2008, 2010, 2012 Christian Persch
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

#ifndef TERMINAL_MDI_CONTAINER_H
#define TERMINAL_MDI_CONTAINER_H

#include <gtk/gtk.h>

#include "terminal-screen.hh"

G_BEGIN_DECLS

#define TERMINAL_TYPE_MDI_CONTAINER            (terminal_mdi_container_get_type ())
#define TERMINAL_MDI_CONTAINER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TERMINAL_TYPE_MDI_CONTAINER, TerminalMdiContainer))
#define TERMINAL_IS_MDI_CONTAINER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TERMINAL_TYPE_MDI_CONTAINER))
#define TERMINAL_MDI_CONTAINER_GET_IFACE(inst) (G_TYPE_INSTANCE_GET_INTERFACE ((inst), TERMINAL_TYPE_MDI_CONTAINER, TerminalMdiContainerInterface))

typedef struct _TerminalMdiContainer          TerminalMdiContainer;
typedef struct _TerminalMdiContainerInterface TerminalMdiContainerInterface;

struct _TerminalMdiContainerInterface {
  GTypeInterface parent_iface;

  /* vfuncs */
  void                  (* add_screen)              (TerminalMdiContainer *container,
                                                     TerminalScreen *screen,
                                                     int position);
  void                  (* remove_screen)           (TerminalMdiContainer *container,
                                                     TerminalScreen *screen);
  TerminalScreen *      (* get_active_screen)       (TerminalMdiContainer *container);
  void                  (* set_active_screen)       (TerminalMdiContainer *container,
                                                     TerminalScreen *screen);
  GList *               (* list_screens)            (TerminalMdiContainer *container);
  GList *               (* list_screen_containers)  (TerminalMdiContainer *container);
  int                   (* get_n_screens)           (TerminalMdiContainer *container);
  int                   (* get_active_screen_num)   (TerminalMdiContainer *container);
  void                  (* set_active_screen_num)   (TerminalMdiContainer *container,
                                                     int position);
  void                  (* reorder_screen)          (TerminalMdiContainer *container,
                                                     TerminalScreen *screen,
                                                     int new_position);

  /* signals */
  void (* screen_added)         (TerminalMdiContainer *container,
                                 TerminalScreen *screen);
  void (* screen_removed)       (TerminalMdiContainer *container,
                                 TerminalScreen *screen);
  void (* screen_switched)      (TerminalMdiContainer *container,
                                 TerminalScreen *old_active_screen,
                                 TerminalScreen *new_active_screen);
  void (* screens_reordered)    (TerminalMdiContainer *container);
  void (* screen_close_request) (TerminalMdiContainer *container,
                                 TerminalScreen *screen);
};

GType terminal_mdi_container_get_type (void);

void terminal_mdi_container_add_screen (TerminalMdiContainer *container,
                                        TerminalScreen *screen,
                                        int position);

void terminal_mdi_container_remove_screen (TerminalMdiContainer *container,
                                           TerminalScreen *screen);

TerminalScreen *terminal_mdi_container_get_active_screen (TerminalMdiContainer *container);

void terminal_mdi_container_set_active_screen (TerminalMdiContainer *container,
                                               TerminalScreen *screen);

void terminal_mdi_container_set_active_screen_num (TerminalMdiContainer *container,
                                                   int position);

GList *terminal_mdi_container_list_screens (TerminalMdiContainer *container);

GList *terminal_mdi_container_list_screen_containers (TerminalMdiContainer *container);

int terminal_mdi_container_get_n_screens (TerminalMdiContainer *container);

int terminal_mdi_container_get_active_screen_num (TerminalMdiContainer *container);

void terminal_mdi_container_reorder_screen (TerminalMdiContainer *container,
                                            TerminalScreen *screen,
                                            int new_position);

void terminal_mdi_container_change_screen (TerminalMdiContainer *container,
                                           int change);

G_END_DECLS

#endif /* TERMINAL_MDI_CONTAINER_H */
