/* cc-snapd-client.h
 *
 * Copyright 2023 Canonical Ltd.
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
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <json-glib/json-glib.h>

G_BEGIN_DECLS

#define CC_TYPE_SNAPD_CLIENT (cc_snapd_client_get_type())
G_DECLARE_FINAL_TYPE (CcSnapdClient, cc_snapd_client, CC, SNAPD_CLIENT, GObject)

// Creates a client to contact snapd.
CcSnapdClient *cc_snapd_client_new                       (void);

// Get information on an installed snap.
JsonObject    *cc_snapd_client_get_snap_sync             (CcSnapdClient      *client,
                                                          const gchar        *name,
                                                          GCancellable       *cancellable,
                                                          GError            **error);

// Get information on a snap change.
JsonObject    *cc_snapd_client_get_change_sync           (CcSnapdClient      *client,
                                                          const gchar        *change_id,
                                                          GCancellable       *cancellable,
                                                          GError            **error);

// Get the state of the snap interface connections.
gboolean       cc_snapd_client_get_all_connections_sync  (CcSnapdClient      *client,
                                                          JsonArray         **plugs,
                                                          JsonArray         **slots,
                                                          GCancellable       *cancellable,
                                                          GError            **error);

// Connect a plug to a slot. Returns the change ID to monitor for completion of this task.
gchar         *cc_snapd_client_connect_interface_sync    (CcSnapdClient      *client,
                                                          const gchar        *plug_snap,
                                                          const gchar        *plug_name,
                                                          const gchar        *slot_snap,
                                                          const gchar        *slot_name,
                                                          GCancellable       *cancellable,
                                                          GError            **error);

// Disconnect a plug to a slot. Returns the change ID to monitor for completion of this task.
gchar         *cc_snapd_client_disconnect_interface_sync (CcSnapdClient      *client,
                                                          const gchar        *plug_snap,
                                                          const gchar        *plug_name,
                                                          const gchar        *slot_snap,
                                                          const gchar        *slot_name,
                                                          GCancellable       *cancellable,
                                                          GError            **error);

G_END_DECLS
