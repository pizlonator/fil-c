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

#include "connection-editor/net-connection-editor.h"
#include "connection-editor/ce-page.h"

#include "net-device-ethernet.h"

struct _NetDeviceEthernet
{
        AdwPreferencesGroup parent;

        GtkListBox         *connection_list;
        GtkStack           *connection_stack;
        GtkButton          *details_button;
        GtkListBox         *details_listbox;
        AdwActionRow       *details_row;
        GtkSwitch          *device_off_switch;

        NMClient           *client;
        NMDevice           *device;
        gboolean            updating_device;
        GHashTable         *connections;
};

G_DEFINE_TYPE (NetDeviceEthernet, net_device_ethernet, ADW_TYPE_PREFERENCES_GROUP)

static void
add_details_row (GtkWidget *details, gint top, const gchar *heading, const gchar *value)
{
        GtkWidget *heading_label;
        GtkWidget *value_label;

        heading_label = gtk_label_new (heading);
        gtk_widget_add_css_class (heading_label, "dim-label");
        gtk_widget_set_halign (heading_label, GTK_ALIGN_END);
        gtk_widget_set_valign (heading_label, GTK_ALIGN_START);
        gtk_widget_set_hexpand (heading_label, TRUE);

        gtk_grid_attach (GTK_GRID (details), heading_label, 0, top, 1, 1);

        value_label = gtk_label_new (value);
        gtk_widget_set_halign (value_label, GTK_ALIGN_START);
        gtk_widget_set_hexpand (value_label, TRUE);
        gtk_label_set_selectable (GTK_LABEL (value_label), TRUE);

        gtk_label_set_mnemonic_widget (GTK_LABEL (heading_label), value_label);

        gtk_grid_attach (GTK_GRID (details), value_label, 1, top, 1, 1);
}

static gchar *
get_last_used_string (NMConnection *connection)
{
        g_autoptr(GDateTime) now = NULL;
        g_autoptr(GDateTime) then = NULL;
        gint days;
        GTimeSpan diff;
        guint64 timestamp;
        NMSettingConnection *s_con;

        s_con = nm_connection_get_setting_connection (connection);
        if (s_con == NULL)
                return NULL;
        timestamp = nm_setting_connection_get_timestamp (s_con);
        if (timestamp == 0)
                return g_strdup (_("never"));

        /* calculate the amount of time that has elapsed */
        now = g_date_time_new_now_utc ();
        then = g_date_time_new_from_unix_utc (timestamp);
        diff = g_date_time_difference  (now, then);
        days = diff / G_TIME_SPAN_DAY;
        if (days == 0)
                return g_strdup (_("today"));
        else if (days == 1)
                return g_strdup (_("yesterday"));
        else
                return g_strdup_printf (ngettext ("%i day ago", "%i days ago", days), days);
}

static void
add_details (GtkWidget *details, NMDevice *device, NMConnection *connection)
{
        NMIPConfig *ip4_config = NULL;
        NMIPConfig *ip6_config = NULL;
        const gchar *ip4_address = NULL;
        const gchar *ip4_route = NULL;
        g_autofree gchar *ip4_dns = NULL;
        g_autofree gchar *ip6_addresses = NULL;
        const gchar *ip6_route = NULL;
        g_autofree gchar *ip6_dns = NULL;
        gint i = 0;

        ip4_config = nm_device_get_ip4_config (device);
        if (ip4_config) {
                GPtrArray *addresses;

                addresses = nm_ip_config_get_addresses (ip4_config);
                if (addresses->len > 0)
                        ip4_address = nm_ip_address_get_address (g_ptr_array_index (addresses, 0));

                ip4_route = nm_ip_config_get_gateway (ip4_config);
                ip4_dns = g_strjoinv (" ", (char **) nm_ip_config_get_nameservers (ip4_config));
                if (!*ip4_dns)
                        ip4_dns = NULL;
        }
        ip6_config = nm_device_get_ip6_config (device);
        if (ip6_config) {
                ip6_addresses = net_device_get_ip6_addresses (ip6_config);
                ip6_route = nm_ip_config_get_gateway (ip6_config);
                ip6_dns = g_strjoinv (" ", (char **) nm_ip_config_get_nameservers (ip6_config));
                if (!*ip6_dns)
                        ip6_dns = NULL;
        }

        if (ip4_address && ip6_addresses) {
                add_details_row (details, i++, _("IPv4 Address"), ip4_address);
                gtk_widget_set_valign (details, GTK_ALIGN_START);
                add_details_row (details, i++, _("IPv6 Address"), ip6_addresses);
                gtk_widget_set_valign (details, GTK_ALIGN_START);
        } else if (ip4_address) {
                add_details_row (details, i++, _("IP Address"), ip4_address);
        } else if (ip6_addresses) {
                add_details_row (details, i++, _("IP Address"), ip6_addresses);
        }

        add_details_row (details, i++, _("Hardware Address"), nm_device_get_hw_address (device));

        if (ip4_route && ip6_route) {
                g_autofree gchar *ip_routes = g_strjoin ("\n", ip4_route, ip6_route, NULL);
                add_details_row (details, i++, _("Default Route"), ip_routes);
        } else if (ip4_route) {
                add_details_row (details, i++, _("Default Route"), ip4_route);
        } else if (ip6_route) {
                add_details_row (details, i++, _("Default Route"), ip6_route);
        }

        if (ip4_dns && ip6_dns) {
                add_details_row (details, i++, _("DNS4"), ip4_dns);
                add_details_row (details, i++, _("DNS6"), ip6_dns);
        } else if (ip4_dns) {
                add_details_row (details, i++, _("DNS"), ip4_dns);
        } else if (ip6_dns) {
                add_details_row (details, i++, _("DNS"), ip6_dns);
        }

        if (nm_device_get_state (device) != NM_DEVICE_STATE_ACTIVATED) {
                g_autofree gchar *last_used = NULL;
                last_used = get_last_used_string (connection);
                add_details_row (details, i++, _("Last used"), last_used);
        }
}

static void populate_ui (NetDeviceEthernet *self);

static gboolean
device_state_to_off_switch (NMDeviceState state)
{
        switch (state) {
                case NM_DEVICE_STATE_UNMANAGED:
                case NM_DEVICE_STATE_UNAVAILABLE:
                case NM_DEVICE_STATE_DISCONNECTED:
                case NM_DEVICE_STATE_DEACTIVATING:
                case NM_DEVICE_STATE_FAILED:
                        return FALSE;
                default:
                        return TRUE;
        }
}

static void
device_ethernet_refresh_ui (NetDeviceEthernet *self)
{
        NMDeviceState state;
        g_autofree gchar *speed_text = NULL;
        g_autofree gchar *status = NULL;

        state = nm_device_get_state (self->device);
        gtk_widget_set_sensitive (GTK_WIDGET (self->device_off_switch),
                                  state != NM_DEVICE_STATE_UNAVAILABLE
                                  && state != NM_DEVICE_STATE_UNMANAGED);
        self->updating_device = TRUE;
        gtk_switch_set_active (self->device_off_switch, device_state_to_off_switch (state));
        self->updating_device = FALSE;

        if (state != NM_DEVICE_STATE_UNAVAILABLE) {
                guint speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (self->device));
                if (speed > 0) {
                        /* Translators: the %'d is replaced by the network device speed with
                         * thousands separator, so do not change to %d */
                        speed_text = g_strdup_printf (_("%'d Mb/s"), speed);
                }
        }
        status = panel_device_status_to_localized_string (self->device, speed_text);
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (self->details_row), status);

        populate_ui (self);
}

static void
editor_done (NetDeviceEthernet *self)
{
        device_ethernet_refresh_ui (self);
}

static void
show_details (NetDeviceEthernet *self, GtkButton *button, const gchar *title)
{
        GtkWidget *row;
        NMConnection *connection;
        NetConnectionEditor *editor;

        row = g_object_get_data (G_OBJECT (button), "row");
        connection = NM_CONNECTION (g_object_get_data (G_OBJECT (row), "connection"));

        editor = net_connection_editor_new (connection, self->device, NULL, self->client);
        gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
        if (title)
                net_connection_editor_set_title (editor, title);
        g_signal_connect_object (editor, "done", G_CALLBACK (editor_done), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (editor));
}

static void
show_details_for_row (NetDeviceEthernet *self, GtkButton *button)
{
        show_details (self, button, NULL);
}

static void
details_button_clicked_cb (NetDeviceEthernet *self)
{
        /* Translators: This is used as the title of the connection
         * details window for ethernet, if there is only a single
         * profile. It is also used to display ethernet in the
         * device list.
         */
        show_details (self, self->details_button, _("Wired"));
}

static void
add_row (NetDeviceEthernet *self, NMConnection *connection)
{
        GtkWidget *row;
        GtkWidget *widget;
        GtkWidget *box;
        GtkWidget *details;
        NMActiveConnection *aconn;
        gboolean active;

        active = FALSE;

        aconn = nm_device_get_active_connection (self->device);
        if (aconn) {
                const gchar *uuid1, *uuid2;
                uuid1 = nm_active_connection_get_uuid (aconn);
                uuid2 = nm_connection_get_uuid (connection);
                active = g_strcmp0 (uuid1, uuid2) == 0;
        }

        row = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
        box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_append (GTK_BOX (row), box);
        widget = gtk_label_new (nm_connection_get_id (connection));
        gtk_widget_set_margin_start (widget, 12);
        gtk_widget_set_margin_end (widget, 12);
        gtk_widget_set_margin_top (widget, 8);
        gtk_widget_set_margin_bottom (widget, 8);
        gtk_box_append (GTK_BOX (box), widget);

        if (active) {
                widget = gtk_image_new_from_icon_name ("object-select-symbolic");
                gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
                gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
                gtk_box_append (GTK_BOX (box), widget);

                details = gtk_grid_new ();
                gtk_grid_set_row_spacing (GTK_GRID (details), 10);
                gtk_grid_set_column_spacing (GTK_GRID (details), 10);

                gtk_box_append (GTK_BOX (row), details);

                add_details (details, self->device, connection);
        }

        /* filler */
        widget = gtk_label_new ("");
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_box_append (GTK_BOX (box), widget);

        widget = gtk_button_new_from_icon_name ("emblem-system-symbolic");
        gtk_widget_set_margin_start (widget, 12);
        gtk_widget_set_margin_end (widget, 12);
        gtk_widget_set_margin_top (widget, 8);
        gtk_widget_set_margin_bottom (widget, 8);
        gtk_widget_set_halign (widget, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (widget, GTK_ALIGN_CENTER);
        gtk_accessible_update_property (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_PROPERTY_LABEL, _("Options…"),
                                        -1);
        gtk_box_append (GTK_BOX (box), widget);
        g_object_set_data (G_OBJECT (widget), "edit", widget);
        g_object_set_data (G_OBJECT (widget), "row", row);
        g_signal_connect_object (widget, "clicked", G_CALLBACK (show_details_for_row), self, G_CONNECT_SWAPPED);

        g_object_set_data (G_OBJECT (row), "connection", connection);

        gtk_list_box_append (self->connection_list, row);
}

static void
connection_removed (NetDeviceEthernet  *self, NMRemoteConnection *connection)
{
        if (g_hash_table_remove (self->connections, connection))
                device_ethernet_refresh_ui (self);
}

static void
populate_ui (NetDeviceEthernet *self)
{
        GSList *connections, *l;
        NMConnection *connection;
        GtkWidget *child;
        gint n_connections;

        while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->connection_list))) != NULL)
               gtk_list_box_remove (self->connection_list, child);

        connections = net_device_get_valid_connections (self->client, self->device);
        for (l = connections; l; l = l->next) {
                NMConnection *connection = l->data;
                if (!g_hash_table_contains (self->connections, connection)) {
                        g_hash_table_add (self->connections, connection);
                }
        }
        n_connections = g_slist_length (connections);

        if (n_connections > 1) {
                for (l = connections; l; l = l->next) {
                        NMConnection *connection = l->data;
                        add_row (self, connection);
                }
                gtk_stack_set_visible_child (self->connection_stack,
                                             GTK_WIDGET (self->connection_list));
        } else if (n_connections == 1) {
                connection = connections->data;
                gtk_stack_set_visible_child (self->connection_stack,
                                             GTK_WIDGET (self->details_listbox));
                g_object_set_data (G_OBJECT (self->details_button), "row", self->details_button);
                g_object_set_data (G_OBJECT (self->details_button), "connection", connection);

        }

        gtk_widget_set_visible (GTK_WIDGET (self->connection_stack), n_connections >= 1);

        g_slist_free (connections);
}

static void
client_connection_added_cb (NetDeviceEthernet  *self)
{
        device_ethernet_refresh_ui (self);
}

static void
add_profile_button_clicked_cb (NetDeviceEthernet *self)
{
        NMConnection *connection;
        NMSettingConnection *sc;
        g_autofree gchar *uuid = NULL;
        g_autofree gchar *id = NULL;
        NetConnectionEditor *editor;
        const GPtrArray *connections;
        const char *iface;

        connection = nm_simple_connection_new ();
        sc = NM_SETTING_CONNECTION (nm_setting_connection_new ());
        nm_connection_add_setting (connection, NM_SETTING (sc));

        uuid = nm_utils_uuid_generate ();

        connections = nm_client_get_connections (self->client);
        id = ce_page_get_next_available_name (connections, NAME_FORMAT_PROFILE, NULL);

        g_object_set (sc,
                      NM_SETTING_CONNECTION_UUID, uuid,
                      NM_SETTING_CONNECTION_ID, id,
                      NM_SETTING_CONNECTION_TYPE, NM_SETTING_WIRED_SETTING_NAME,
                      NM_SETTING_CONNECTION_AUTOCONNECT, TRUE,
                      NULL);

        iface = nm_device_get_iface (self->device);
        g_object_set (sc,
                      NM_SETTING_CONNECTION_INTERFACE_NAME, iface,
                      NULL);

        nm_connection_add_setting (connection, nm_setting_wired_new ());

        editor = net_connection_editor_new (connection, self->device, NULL, self->client);
        gtk_window_set_transient_for (GTK_WINDOW (editor), GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
        g_signal_connect_object (editor, "done", G_CALLBACK (editor_done), self, G_CONNECT_SWAPPED);
        gtk_window_present (GTK_WINDOW (editor));
}

static void
device_off_switch_changed_cb (NetDeviceEthernet *self)
{
        NMConnection *connection;

        if (self->updating_device)
                return;

        if (gtk_switch_get_active (self->device_off_switch)) {
                connection = net_device_get_find_connection (self->client, self->device);
                if (connection != NULL) {
                        nm_client_activate_connection_async (self->client,
                                                             connection,
                                                             self->device,
                                                             NULL, NULL, NULL, NULL);
                }
        } else {
                nm_device_disconnect_async (self->device, NULL, NULL, NULL);
        }
}

static void
connection_list_row_activated_cb (NetDeviceEthernet *self, GtkListBoxRow *row)
{
        NMConnection *connection;
        GtkWidget *child;

        if (!NM_IS_DEVICE_ETHERNET (self->device) ||
            !nm_device_ethernet_get_carrier (NM_DEVICE_ETHERNET (self->device)))
                return;

        child = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row));
        connection = NM_CONNECTION (g_object_get_data (G_OBJECT (child), "connection"));

        nm_client_activate_connection_async (self->client,
                                             connection,
                                             self->device,
                                             NULL, NULL, NULL, NULL);
}

static void
device_ethernet_finalize (GObject *object)
{
        NetDeviceEthernet *self = NET_DEVICE_ETHERNET (object);

        g_clear_object (&self->client);
        g_clear_object (&self->device);
        g_hash_table_destroy (self->connections);

        G_OBJECT_CLASS (net_device_ethernet_parent_class)->finalize (object);
}

static void
net_device_ethernet_class_init (NetDeviceEthernetClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->finalize = device_ethernet_finalize;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-ethernet.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, connection_list);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, connection_stack);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, details_button);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, details_listbox);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, details_row);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceEthernet, device_off_switch);

        gtk_widget_class_bind_template_callback (widget_class, connection_list_row_activated_cb);
        gtk_widget_class_bind_template_callback (widget_class, device_off_switch_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, details_button_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, add_profile_button_clicked_cb);
}

static void
net_device_ethernet_init (NetDeviceEthernet *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));

        self->connections = g_hash_table_new (NULL, NULL);
}

NetDeviceEthernet *
net_device_ethernet_new (NMClient *client, NMDevice *device)
{
        NetDeviceEthernet *self;

        self = g_object_new (net_device_ethernet_get_type (), NULL);
        self->client = g_object_ref (client);
        self->device = g_object_ref (device);

        g_signal_connect_object (client, NM_CLIENT_CONNECTION_ADDED,
                                 G_CALLBACK (client_connection_added_cb), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (client, NM_CLIENT_CONNECTION_REMOVED,
                                 G_CALLBACK (connection_removed), self, G_CONNECT_SWAPPED);

        g_signal_connect_object (device, "state-changed", G_CALLBACK (device_ethernet_refresh_ui), self, G_CONNECT_SWAPPED);

        device_ethernet_refresh_ui (self);

        return self;
}

NMDevice *
net_device_ethernet_get_device (NetDeviceEthernet *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_ETHERNET (self), NULL);
        return self->device;
}
