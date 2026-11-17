/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager Connection editor -- Connection editor for NetworkManager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * (C) Copyright 2008 Red Hat, Inc.
 */

#ifndef _VPN_HELPERS_H_
#define _VPN_HELPERS_H_

#include <glib.h>
#include <gtk/gtk.h>
#include <NetworkManager.h>

GSList *vpn_get_plugins (void);

NMVpnEditorPlugin *vpn_get_plugin_by_service (const char *service);

typedef void (*VpnImportCallback) (NMConnection *connection, gpointer user_data);
void vpn_import (GtkWindow *parent, VpnImportCallback callback, gpointer user_data);

void vpn_export (NMConnection *connection);

#endif  /* _VPN_HELPERS_H_ */
