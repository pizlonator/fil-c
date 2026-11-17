/*
 * Copyright © 2018 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <adwaita.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

#define CC_TYPE_WIFI_CONNECTION_LIST (cc_wifi_connection_list_get_type())

G_DECLARE_FINAL_TYPE (CcWifiConnectionList, cc_wifi_connection_list, CC, WIFI_CONNECTION_LIST, AdwBin)

CcWifiConnectionList *cc_wifi_connection_list_new (NMClient     *client,
                                                   NMDeviceWifi *device,
                                                   gboolean      hide_unavailable,
                                                   gboolean      show_aps,
                                                   gboolean      checkable,
                                                   gboolean      forgettable );


void                  cc_wifi_connection_list_freeze (CcWifiConnectionList  *list);
void                  cc_wifi_connection_list_thaw   (CcWifiConnectionList  *list);

GtkListBox           *cc_wifi_connection_list_get_list_box (CcWifiConnectionList *self);

gboolean              cc_wifi_connection_list_is_empty (CcWifiConnectionList *self);

G_END_DECLS
