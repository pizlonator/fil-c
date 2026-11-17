/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Red Hat, Inc.
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

#include "connection-editor/net-connection-editor.h"

#include "net-device-bluetooth.h"

struct _NetDeviceBluetooth
{
        AdwActionRow  parent;

        GtkSwitch    *device_off_switch;
        GtkButton    *options_button;

        NMClient     *client;
        NMDevice     *device;
        gboolean      updating_device;
};

G_DEFINE_TYPE (NetDeviceBluetooth, net_device_bluetooth, ADW_TYPE_ACTION_ROW)

static void
update_off_switch_from_device_state (GtkSwitch *sw,
                                     NMDeviceState state,
                                     NetDeviceBluetooth *self)
{
        self->updating_device = TRUE;
        switch (state) {
                case NM_DEVICE_STATE_UNMANAGED:
                case NM_DEVICE_STATE_UNAVAILABLE:
                case NM_DEVICE_STATE_DISCONNECTED:
                case NM_DEVICE_STATE_DEACTIVATING:
                case NM_DEVICE_STATE_FAILED:
                        gtk_switch_set_active (sw, FALSE);
                        break;
                default:
                        gtk_switch_set_active (sw, TRUE);
                        break;
        }
        self->updating_device = FALSE;
}

static void
nm_device_bluetooth_refresh_ui (NetDeviceBluetooth *self)
{
        NMDeviceState state;
        g_autofree gchar *path = NULL;

        /* set up the device on/off switch */
        state = nm_device_get_state (self->device);
        gtk_widget_set_visible (GTK_WIDGET (self->device_off_switch),
                                state != NM_DEVICE_STATE_UNAVAILABLE
                                && state != NM_DEVICE_STATE_UNMANAGED);
        update_off_switch_from_device_state (self->device_off_switch, state, self);
}

static void
device_off_switch_changed_cb (NetDeviceBluetooth *self)
{
        const GPtrArray *acs;
        gboolean active;
        gint i;
        NMActiveConnection *a;
        NMConnection *connection;

        if (self->updating_device)
                return;

        connection = net_device_get_find_connection (self->client, self->device);
        if (connection == NULL)
                return;

        active = gtk_switch_get_active (self->device_off_switch);
        if (active) {
                nm_client_activate_connection_async (self->client,
                                                     connection,
                                                     self->device,
                                                     NULL, NULL, NULL, NULL);
        } else {
                const gchar *uuid;

                uuid = nm_connection_get_uuid (connection);
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
editor_done (NetDeviceBluetooth *self)
{
        nm_device_bluetooth_refresh_ui (self);
}

static void
options_button_clicked_cb (NetDeviceBluetooth *self)
{
        NMConnection *connection;
        NetConnectionEditor *editor;

        connection = net_device_get_find_connection (self->client, self->device);

        editor = net_connection_editor_new (connection, self->device, NULL, self->client);
        gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
        net_connection_editor_set_title (editor, _("Bluetooth"));
        g_signal_connect_object (editor, "done", G_CALLBACK (editor_done), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (editor));
}

static void
net_device_bluetooth_finalize (GObject *object)
{
        NetDeviceBluetooth *self = NET_DEVICE_BLUETOOTH (object);

        g_clear_object (&self->client);
        g_clear_object (&self->device);

        G_OBJECT_CLASS (net_device_bluetooth_parent_class)->finalize (object);
}

static void
net_device_bluetooth_class_init (NetDeviceBluetoothClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = net_device_bluetooth_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-bluetooth.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceBluetooth, device_off_switch);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceBluetooth, options_button);

        gtk_widget_class_bind_template_callback (widget_class, device_off_switch_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, options_button_clicked_cb);

}

static void
net_device_bluetooth_init (NetDeviceBluetooth *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

NetDeviceBluetooth *
net_device_bluetooth_new (NMClient *client, NMDevice *device)
{
        NetDeviceBluetooth *self;

        self = g_object_new (net_device_bluetooth_get_type (), NULL);
        self->client = g_object_ref (client);
        self->device = g_object_ref (device);

        g_signal_connect_object (device, "state-changed", G_CALLBACK (nm_device_bluetooth_refresh_ui), self, G_CONNECT_SWAPPED);

        nm_device_bluetooth_refresh_ui (self);

        return self;
}

NMDevice *
net_device_bluetooth_get_device (NetDeviceBluetooth *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_BLUETOOTH (self), NULL);
        return self->device;
}
