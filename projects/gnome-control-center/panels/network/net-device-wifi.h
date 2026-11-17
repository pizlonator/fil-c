/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <shell/cc-panel.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (NetDeviceWifi, net_device_wifi, NET, DEVICE_WIFI, AdwBin)

NetDeviceWifi *net_device_wifi_new               (CcPanel       *panel,
                                                  NMClient      *client,
                                                  NMDevice      *device);

NMDevice      *net_device_wifi_get_device        (NetDeviceWifi *device);

void           net_device_wifi_set_title         (NetDeviceWifi *device,
                                                  const gchar   *title);

GtkWidget     *net_device_wifi_get_header_widget (NetDeviceWifi *device);

GtkWidget     *net_device_wifi_get_title_widget  (NetDeviceWifi *device);

void           net_device_wifi_turn_off_hotspot  (NetDeviceWifi *self);

G_END_DECLS

