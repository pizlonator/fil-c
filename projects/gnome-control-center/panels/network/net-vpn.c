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

#include "config.h"

#include <glib-object.h>
#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "panel-common.h"

#include "net-vpn.h"

#include "connection-editor/net-connection-editor.h"

struct _NetVpn
{
        AdwActionRow             parent;

        GtkBox                  *box;
        GtkSwitch               *device_off_switch;

        NMClient                *client;
        NMConnection            *connection;
        NMActiveConnection      *active_connection;
        gboolean                 updating_device;
};

G_DEFINE_TYPE (NetVpn, net_vpn, ADW_TYPE_ACTION_ROW)

static void
nm_device_refresh_vpn_ui (NetVpn *self)
{
        g_autofree char *title_escaped = NULL;
        const GPtrArray *acs;
        NMActiveConnection *a;
        gint i;
        NMVpnConnectionState state;

        /* update title */
        title_escaped = g_markup_escape_text (nm_connection_get_id (self->connection),
                                              -1);
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self),
                                       title_escaped);

        if (self->active_connection) {
                g_signal_handlers_disconnect_by_func (self->active_connection,
                                                      nm_device_refresh_vpn_ui,
                                                      self);
                g_clear_object (&self->active_connection);
        }


        /* Default to disconnected if there is no active connection */
        state = NM_VPN_CONNECTION_STATE_DISCONNECTED;
        acs = nm_client_get_active_connections (self->client);
        if (acs != NULL) {
                const gchar *uuid;
                uuid = nm_connection_get_uuid (self->connection);

                for (i = 0; i < acs->len; i++) {
                        const gchar *auuid;

                        a = (NMActiveConnection*)acs->pdata[i];

                        auuid = nm_active_connection_get_uuid (a);
                        if (strcmp (auuid, uuid) == 0) {
                                if (NM_IS_VPN_CONNECTION (a))
                                        state = nm_vpn_connection_get_vpn_state (NM_VPN_CONNECTION (a));
                                else if (nm_is_wireguard_connection (a))
                                        state = nm_active_connection_get_state (a);
                                else {
                                        /* Unknown/Unhandled type */
                                        break;
                                }
                                self->active_connection = g_object_ref (a);
                                g_signal_connect_object (a, "notify::vpn-state",
                                                         G_CALLBACK (nm_device_refresh_vpn_ui),
                                                         self, G_CONNECT_SWAPPED);
                                break;
                        }
                }
        }

        self->updating_device = TRUE;
        gtk_switch_set_active (self->device_off_switch,
                               state != NM_VPN_CONNECTION_STATE_FAILED &&
                               state != NM_VPN_CONNECTION_STATE_DISCONNECTED);
        self->updating_device = FALSE;

        gtk_list_box_row_changed (GTK_LIST_BOX_ROW (self));
}

static void
nm_active_connections_changed (NetVpn *self)
{
        nm_device_refresh_vpn_ui (self);
}

static void
device_off_toggled (NetVpn *self)
{
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;

        if (self->updating_device)
                return;

        active = gtk_switch_get_active (self->device_off_switch);
        if (active) {
                nm_client_activate_connection_async (self->client,
                                                     self->connection, NULL, NULL,
                                                     NULL, NULL, NULL);
        } else {
                const gchar *uuid;

                uuid = nm_connection_get_uuid (self->connection);
                acs = nm_client_get_active_connections (self->client);
                for (i = 0; acs && i < acs->len; i++) {
                        a = (NMActiveConnection*)acs->pdata[i];
                        if (strcmp (nm_active_connection_get_uuid (a), uuid) == 0) {
                                nm_client_deactivate_connection_async (self->client, a, NULL, NULL, NULL);
                                break;
                        }
                }
        }
}

static void
editor_done (NetVpn *self)
{
        nm_device_refresh_vpn_ui (self);
}

static void
edit_connection (NetVpn *self)
{
        NetConnectionEditor *editor;

        editor = net_connection_editor_new (self->connection, NULL, NULL, self->client);
        gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
        net_connection_editor_set_title (editor, nm_connection_get_id (self->connection));

        g_signal_connect_object (editor, "done", G_CALLBACK (editor_done), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (editor));
}

static void
net_vpn_finalize (GObject *object)
{
        NetVpn *self = NET_VPN (object);

        if (self->active_connection)
                g_signal_handlers_disconnect_by_func (self->active_connection,
                                                      nm_device_refresh_vpn_ui,
                                                      self);

        g_clear_object (&self->active_connection);
        g_clear_object (&self->client);
        g_clear_object (&self->connection);

        G_OBJECT_CLASS (net_vpn_parent_class)->finalize (object);
}

static void
net_vpn_class_init (NetVpnClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = net_vpn_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-vpn.ui");

        gtk_widget_class_bind_template_child (widget_class, NetVpn, device_off_switch);

        gtk_widget_class_bind_template_callback (widget_class, device_off_toggled);
        gtk_widget_class_bind_template_callback (widget_class, edit_connection);
}

static void
net_vpn_init (NetVpn *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

NetVpn *
net_vpn_new (NMClient     *client,
             NMConnection *connection)
{
        NetVpn *self;

        self = g_object_new (net_vpn_get_type (), NULL);
        self->client = g_object_ref (client);
        self->connection = g_object_ref (connection);

        g_signal_connect_object (connection,
                                 NM_CONNECTION_CHANGED,
                                 G_CALLBACK (nm_device_refresh_vpn_ui),
                                 self, G_CONNECT_SWAPPED);

        nm_device_refresh_vpn_ui (self);

        g_signal_connect_object (client,
                                 "notify::active-connections",
                                 G_CALLBACK (nm_active_connections_changed),
                                 self, G_CONNECT_SWAPPED);

        return self;
}

NMConnection *
net_vpn_get_connection (NetVpn *self)
{
        g_return_val_if_fail (NET_IS_VPN (self), NULL);
        return self->connection;
}

gboolean
nm_is_wireguard_connection (NMActiveConnection *c) {
        const GPtrArray *devices;
        devices = nm_active_connection_get_devices (c);
        for (int j = 0; devices && j < devices->len; j++) {
                if (NM_IS_DEVICE_WIREGUARD (g_ptr_array_index (devices, j)))
                        return TRUE;
        }
        return FALSE;
}
