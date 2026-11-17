/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright 2012  Red Hat, Inc,
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Marek Kasik <mkasik@redhat.com>
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define PP_TYPE_MAINTENANCE_COMMAND (pp_maintenance_command_get_type ())
G_DECLARE_FINAL_TYPE (PpMaintenanceCommand, pp_maintenance_command, PP, MAINTENANCE_COMMAND, GObject)

PpMaintenanceCommand *pp_maintenance_command_new                 (const gchar *printer_name,
                                                                  const gchar *command,
                                                                  const gchar *parameters,
                                                                  const gchar *title);

void                  pp_maintenance_command_execute_async       (PpMaintenanceCommand *command,
                                                                  GCancellable         *cancellable,
                                                                  GAsyncReadyCallback   callback,
                                                                  gpointer              user_data);

gboolean              pp_maintenance_command_execute_finish      (PpMaintenanceCommand  *command,
                                                                  GAsyncResult          *result,
                                                                  GError               **error);
void                  pp_maintenance_command_is_supported_async  (PpMaintenanceCommand *command,
                                                                  GCancellable         *cancellable,
                                                                  GAsyncReadyCallback   callback,
                                                                  gpointer              user_data);

gboolean              pp_maintenance_command_is_supported_finish (PpMaintenanceCommand  *command,
                                                                  GAsyncResult          *result,
                                                                  GError               **error);
G_END_DECLS
