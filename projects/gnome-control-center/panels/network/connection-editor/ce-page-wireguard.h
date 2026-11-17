/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Nathan-J. Hirschauer <nathanhi@deepserve.info>
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

#include <gtk/gtk.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

G_DECLARE_FINAL_TYPE (CEPageWireguard, ce_page_wireguard, CE, PAGE_WIREGUARD, GtkBox)
G_DECLARE_FINAL_TYPE (WireguardPeer, wireguard_peer, WIREGUARD, PEER, GtkBox)

gchar *peer_allowed_ips_to_str (NMWireGuardPeer *peer);
WireguardPeer *add_nm_wg_peer_to_list (CEPageWireguard *self, NMWireGuardPeer *peer);

CEPageWireguard *ce_page_wireguard_new (NMConnection *connection);
WireguardPeer *wireguard_peer_new (CEPageWireguard *parent);

G_END_DECLS
