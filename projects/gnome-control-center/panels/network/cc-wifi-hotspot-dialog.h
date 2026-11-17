/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* wifi-hotspot-dialog.h
 *
 * Copyright 2019 Purism SPC
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

#define CC_TYPE_WIFI_HOTSPOT_DIALOG (cc_wifi_hotspot_dialog_get_type())
G_DECLARE_FINAL_TYPE (CcWifiHotspotDialog, cc_wifi_hotspot_dialog, CC, WIFI_HOTSPOT_DIALOG, GtkDialog)

CcWifiHotspotDialog *cc_wifi_hotspot_dialog_new            (GtkWindow           *parent_window);
void                 cc_wifi_hotspot_dialog_set_hostname   (CcWifiHotspotDialog *self,
                                                            const gchar         *host_name);
void                 cc_wifi_hotspot_dialog_set_device     (CcWifiHotspotDialog *self,
                                                            NMDeviceWifi        *device);
NMConnection        *cc_wifi_hotspot_dialog_get_connection (CcWifiHotspotDialog *self);
void                 cc_wifi_hotspot_dialog_set_connection (CcWifiHotspotDialog *self,
                                                            NMConnection        *connection);

G_END_DECLS
