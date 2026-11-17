/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Thomas Bechtold <thomasbechtold@jpberlin.de>
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
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
 */

#include <config.h>
#include <glib/gi18n.h>
#include <stdlib.h>

#include "shell/cc-object-storage.h"

#include "cc-network-panel.h"
#include "cc-network-resources.h"

#include <NetworkManager.h>

#include "cc-list-row.h"
#include "cc-net-proxy-page.h"
#include "net-device-bluetooth.h"
#include "net-device-ethernet.h"
#include "net-device-mobile.h"
#include "net-device-wifi.h"
#include "net-vpn.h"

#include "panel-common.h"

#include "network-dialogs.h"
#include "connection-editor/net-connection-editor.h"

#include <libmm-glib.h>

typedef enum {
        OPERATION_NULL,
        OPERATION_SHOW_DEVICE,
        OPERATION_CONNECT_MOBILE
} CmdlineOperation;

struct _CcNetworkPanel
{
        CcPanel           parent;

        GPtrArray        *bluetooth_devices;
        GPtrArray        *ethernet_devices;
        GPtrArray        *mobile_devices;
        GPtrArray        *vpns;
        GHashTable       *nm_device_to_device;

        NMClient         *client;
        MMManager        *modem_manager;
        gboolean          updating_device;

        /* widgets */
        GtkWidget        *box_bluetooth;
        GtkWidget        *box_vpn;
        GtkWidget        *box_wired;
        GtkWidget        *container_bluetooth;
        GtkWidget        *empty_listbox;
        GtkWidget        *proxy_row;
        GtkWidget        *save_button;
        GtkWidget        *vpn_stack;
        GtkWidget        *toolbar_view;

        /* wireless dialog stuff */
        CmdlineOperation  arg_operation;
        gchar            *arg_device;
        gchar            *arg_access_point;
        gboolean          operation_done;
};

enum {
        PROP_0,
        PROP_PARAMETERS
};

static void handle_argv (CcNetworkPanel *self);
static void device_managed_cb (CcNetworkPanel *self, GParamSpec *pspec, NMDevice *device);

CC_PANEL_REGISTER (CcNetworkPanel, cc_network_panel)

static void
cc_network_panel_get_property (GObject    *object,
                               guint       property_id,
                               GValue     *value,
                               GParamSpec *pspec)
{
        switch (property_id) {
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static CmdlineOperation
cmdline_operation_from_string (const gchar *string)
{
        if (g_strcmp0 (string, "connect-3g") == 0)
                return OPERATION_CONNECT_MOBILE;
        if (g_strcmp0 (string, "show-device") == 0)
                return OPERATION_SHOW_DEVICE;

        g_warning ("Invalid additional argument %s", string);
        return OPERATION_NULL;
}

static void
reset_command_line_args (CcNetworkPanel *self)
{
	self->arg_operation = OPERATION_NULL;
	g_clear_pointer (&self->arg_device, g_free);
	g_clear_pointer (&self->arg_access_point, g_free);
}

static gboolean
verify_argv (CcNetworkPanel *self,
	     const char    **args)
{
	switch (self->arg_operation) {
	case OPERATION_CONNECT_MOBILE:
	case OPERATION_SHOW_DEVICE:
		if (self->arg_device == NULL) {
			g_warning ("Operation %s requires an object path", args[0]);
		        return FALSE;
                }
	default:
		return TRUE;
	}
}

static GPtrArray *
variant_av_to_string_array (GVariant *array)
{
        GVariantIter iter;
        GVariant *v;
        GPtrArray *strv;
        gsize count;
        count = g_variant_iter_init (&iter, array);
        strv = g_ptr_array_sized_new (count + 1);
        while (g_variant_iter_next (&iter, "v", &v)) {
                g_ptr_array_add (strv, (gpointer)g_variant_get_string (v, NULL));
                g_variant_unref (v);
        }
        g_ptr_array_add (strv, NULL); /* NULL-terminate the strv data array */
        return strv;
}

static void
cc_network_panel_set_property (GObject      *object,
                               guint         property_id,
                               const GValue *value,
                               GParamSpec   *pspec)
{
        CcNetworkPanel *self = CC_NETWORK_PANEL (object);

        switch (property_id) {
        case PROP_PARAMETERS: {
                GVariant *parameters;

                reset_command_line_args (self);

                parameters = g_value_get_variant (value);
                if (parameters) {
                        g_autoptr(GPtrArray) array = NULL;
                        const gchar **args;
                        array = variant_av_to_string_array (parameters);
                        args = (const gchar **) array->pdata;

                        g_debug ("Invoked with operation %s", args[0]);

                        if (args[0])
                                self->arg_operation = cmdline_operation_from_string (args[0]);
                        if (args[0] && args[1])
                                self->arg_device = g_strdup (args[1]);
                        if (args[0] && args[1] && args[2])
                                self->arg_access_point = g_strdup (args[2]);

                        if (verify_argv (self, (const char **) args) == FALSE) {
                                reset_command_line_args (self);
                                return;
                        }
                        g_debug ("Calling handle_argv() after setting property");
                        handle_argv (self);
                }
                break;
        }
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
}

static void
cc_network_panel_dispose (GObject *object)
{
        CcNetworkPanel *self = CC_NETWORK_PANEL (object);

        g_clear_object (&self->client);
        g_clear_object (&self->modem_manager);

        g_clear_pointer (&self->bluetooth_devices, g_ptr_array_unref);
        g_clear_pointer (&self->ethernet_devices, g_ptr_array_unref);
        g_clear_pointer (&self->mobile_devices, g_ptr_array_unref);
        g_clear_pointer (&self->vpns, g_ptr_array_unref);
        g_clear_pointer (&self->nm_device_to_device, g_hash_table_destroy);

        G_OBJECT_CLASS (cc_network_panel_parent_class)->dispose (object);
}

static void
cc_network_panel_finalize (GObject *object)
{
        CcNetworkPanel *self = CC_NETWORK_PANEL (object);

        reset_command_line_args (self);

        G_OBJECT_CLASS (cc_network_panel_parent_class)->finalize (object);
}

static const char *
cc_network_panel_get_help_uri (CcPanel *panel)
{
	return "help:gnome-help/net";
}

static void
panel_refresh_device_titles (CcNetworkPanel *self)
{
        g_autoptr(GPtrArray) ndarray = NULL;
        g_autoptr(GPtrArray) nmdarray = NULL;
        GtkWidget **devices;
        NMDevice **nm_devices;
        g_auto(GStrv) titles = NULL;
        guint i, num_devices;

        ndarray = g_ptr_array_new ();
        nmdarray = g_ptr_array_new ();
        for (i = 0; i < self->bluetooth_devices->len; i++) {
                NetDeviceBluetooth *device = g_ptr_array_index (self->bluetooth_devices, i);
                g_ptr_array_add (ndarray, device);
                g_ptr_array_add (nmdarray, net_device_bluetooth_get_device (device));
        }
        for (i = 0; i < self->ethernet_devices->len; i++) {
                NetDeviceEthernet *device = g_ptr_array_index (self->ethernet_devices, i);
                g_ptr_array_add (ndarray, device);
                g_ptr_array_add (nmdarray, net_device_ethernet_get_device (device));
        }
        for (i = 0; i < self->mobile_devices->len; i++) {
                NetDeviceMobile *device = g_ptr_array_index (self->mobile_devices, i);
                g_ptr_array_add (ndarray, device);
                g_ptr_array_add (nmdarray, net_device_mobile_get_device (device));
        }

        if (ndarray->len == 0)
                return;

        devices = (GtkWidget **)ndarray->pdata;
        nm_devices = (NMDevice **)nmdarray->pdata;
        num_devices = ndarray->len;

        titles = nm_device_disambiguate_names (nm_devices, num_devices);
        for (i = 0; i < num_devices; i++) {
                if (NM_IS_DEVICE_BT (nm_devices[i]))
                        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (devices[i]), nm_device_bt_get_name (NM_DEVICE_BT (nm_devices[i])));
                else if (NET_IS_DEVICE_ETHERNET (devices[i]))
                        adw_preferences_group_set_title (ADW_PREFERENCES_GROUP (devices[i]), titles[i]);
                else if (NET_IS_DEVICE_MOBILE (devices[i]))
                        net_device_mobile_set_title (NET_DEVICE_MOBILE (devices[i]), titles[i]);
        }
}

static gboolean
handle_argv_for_device (CcNetworkPanel *self,
			NMDevice       *device)
{
        GtkWidget *toplevel = cc_shell_get_toplevel (cc_panel_get_shell (CC_PANEL (self)));

        if (self->arg_operation == OPERATION_NULL)
                return TRUE;

        if (g_strcmp0 (nm_object_get_path (NM_OBJECT (device)), self->arg_device) == 0) {
                if (self->arg_operation == OPERATION_CONNECT_MOBILE) {
                        cc_network_panel_connect_to_3g_network (toplevel, self->client, device);

                        reset_command_line_args (self); /* done */
                        return TRUE;
                } else if (self->arg_operation == OPERATION_SHOW_DEVICE) {
                        reset_command_line_args (self); /* done */
                        return TRUE;
                }
        }

        return FALSE;
}

static gboolean
handle_argv_for_connection (CcNetworkPanel *self,
                            NMConnection   *connection)
{
        if (self->arg_operation == OPERATION_NULL)
                return TRUE;
        if (self->arg_operation != OPERATION_SHOW_DEVICE)
                return FALSE;

        if (g_strcmp0 (nm_connection_get_path (connection), self->arg_device) == 0) {
                reset_command_line_args (self);
                return TRUE;
        }

        return FALSE;
}


static void
handle_argv (CcNetworkPanel *self)
{
        gint i;

        if (self->arg_operation == OPERATION_NULL)
                return;

        for (i = 0; i < self->bluetooth_devices->len; i++) {
                NetDeviceBluetooth *device = g_ptr_array_index (self->bluetooth_devices, i);
                if (handle_argv_for_device (self, net_device_bluetooth_get_device (device)))
                        return;
        }
        for (i = 0; i < self->ethernet_devices->len; i++) {
                NetDeviceEthernet *device = g_ptr_array_index (self->ethernet_devices, i);
                if (handle_argv_for_device (self, net_device_ethernet_get_device (device)))
                        return;
        }
        for (i = 0; i < self->mobile_devices->len; i++) {
                NetDeviceMobile *device = g_ptr_array_index (self->mobile_devices, i);
                if (handle_argv_for_device (self, net_device_mobile_get_device (device)))
                        return;
        }
        for (i = 0; i < self->vpns->len; i++) {
                NetVpn *vpn = g_ptr_array_index (self->vpns, i);
                if (handle_argv_for_connection (self, net_vpn_get_connection (vpn)))
                        return;
        }

        g_debug ("Could not handle argv operation, no matching device yet?");
}

static void
update_vpn_section (CcNetworkPanel *self)
{
        gtk_stack_set_visible_child (GTK_STACK (self->vpn_stack),
                                     self->vpns->len == 0 ? self->empty_listbox : self->box_vpn);
}

static void
update_bluetooth_section (CcNetworkPanel *self)
{
        gtk_widget_set_visible (self->container_bluetooth, self->bluetooth_devices->len > 0);
}

static gboolean
wwan_panel_supports_modem (GDBusObject *object)
{
        MMObject *mm_object;
        MMModem *modem;
        MMModemCapability capability, supported_capabilities;

        g_assert (G_IS_DBUS_OBJECT (object));

        supported_capabilities = MM_MODEM_CAPABILITY_GSM_UMTS | MM_MODEM_CAPABILITY_LTE;
#if MM_CHECK_VERSION (1,14,0)
        supported_capabilities |= MM_MODEM_CAPABILITY_5GNR;
#endif

        mm_object = MM_OBJECT (object);
        modem = mm_object_get_modem (mm_object);
        capability = mm_modem_get_current_capabilities (modem);

        return capability & supported_capabilities;
}

static void
panel_add_device (CcNetworkPanel *self, NMDevice *device)
{
        NMDeviceType type;
        NetDeviceEthernet *device_ethernet;
        NetDeviceMobile *device_mobile;
        NetDeviceBluetooth *device_bluetooth;
        g_autoptr(GDBusObject) modem_object = NULL;

        /* does already exist */
        if (g_hash_table_lookup (self->nm_device_to_device, device) != NULL)
                return;

        type = nm_device_get_device_type (device);

        g_debug ("device %s type %i path %s",
                 nm_device_get_udi (device), type, nm_object_get_path (NM_OBJECT (device)));

        /* map the NMDeviceType to the GType, or ignore */
        switch (type) {
        case NM_DEVICE_TYPE_ETHERNET:
        case NM_DEVICE_TYPE_INFINIBAND:
                device_ethernet = net_device_ethernet_new (self->client, device);
                gtk_box_append (GTK_BOX (self->box_wired), GTK_WIDGET (device_ethernet));
                g_ptr_array_add (self->ethernet_devices, device_ethernet);
                g_hash_table_insert (self->nm_device_to_device, device, device_ethernet);
                break;
        case NM_DEVICE_TYPE_MODEM:
                if (g_str_has_prefix (nm_device_get_udi (device), "/org/freedesktop/ModemManager1/Modem/")) {
                        if (self->modem_manager == NULL) {
                                g_warning ("Cannot grab information for modem at %s: No ModemManager support",
                                           nm_device_get_udi (device));
                                return;
                        }

                        modem_object = g_dbus_object_manager_get_object (G_DBUS_OBJECT_MANAGER (self->modem_manager),
                                                                         nm_device_get_udi (device));
                        if (modem_object == NULL) {
                                g_warning ("Cannot grab information for modem at %s: Not found",
                                           nm_device_get_udi (device));
                                return;
                        }

                        /* This will be handled by cellular panel */
                        if (wwan_panel_supports_modem (modem_object))
                                return;
                }

                device_mobile = net_device_mobile_new (self->client, device, modem_object);
                gtk_box_append (GTK_BOX (self->box_wired), GTK_WIDGET (device_mobile));
                g_ptr_array_add (self->mobile_devices, device_mobile);
                g_hash_table_insert (self->nm_device_to_device, device, device_mobile);
                break;
        case NM_DEVICE_TYPE_BT:
                device_bluetooth = net_device_bluetooth_new (self->client, device);
                gtk_list_box_append (GTK_LIST_BOX (self->box_bluetooth), GTK_WIDGET (device_bluetooth));
                g_ptr_array_add (self->bluetooth_devices, device_bluetooth);
                g_hash_table_insert (self->nm_device_to_device, device, device_bluetooth);

                /* Update the device_bluetooth section if we're adding a bluetooth
                 * device. This is a temporary solution though, for these will
                 * be handled by the future Mobile Broadband panel */
                update_bluetooth_section (self);
                break;

        /* For Wi-Fi and VPN we handle connections separately; we correctly manage
         * them, but not here.
         */
        case NM_DEVICE_TYPE_WIFI:
        case NM_DEVICE_TYPE_TUN:
        /* And the rest we simply cannot deal with currently. */
        default:
                return;
        }
}

static void
panel_remove_device (CcNetworkPanel *self, NMDevice *device)
{
        GtkWidget *net_device;

        net_device = g_hash_table_lookup (self->nm_device_to_device, device);
        if (net_device == NULL)
                return;

        g_ptr_array_remove (self->bluetooth_devices, net_device);
        g_ptr_array_remove (self->ethernet_devices, net_device);
        g_ptr_array_remove (self->mobile_devices, net_device);
        g_hash_table_remove (self->nm_device_to_device, device);

        if (nm_device_get_device_type (device) == NM_DEVICE_TYPE_BT)
                gtk_list_box_remove (GTK_LIST_BOX (self->box_bluetooth), net_device);
        else
                gtk_box_remove (GTK_BOX (gtk_widget_get_parent (net_device)), net_device);

        /* update vpn widgets */
        update_vpn_section (self);

        /* update device_bluetooth widgets */
        update_bluetooth_section (self);
}

static void
connection_state_changed (CcNetworkPanel *self)
{
}

static void
active_connections_changed (CcNetworkPanel *self)
{
        const GPtrArray *connections;
        int i, j;

        g_debug ("Active connections changed:");
        connections = nm_client_get_active_connections (self->client);
        for (i = 0; connections && (i < connections->len); i++) {
                NMActiveConnection *connection;
                const GPtrArray *devices;

                connection = g_ptr_array_index (connections, i);
                g_debug ("    %s", nm_object_get_path (NM_OBJECT (connection)));
                devices = nm_active_connection_get_devices (connection);
                for (j = 0; devices && j < devices->len; j++)
                        g_debug ("           %s", nm_device_get_udi (g_ptr_array_index (devices, j)));

                if (nm_is_wireguard_connection (connection))
                        g_debug ("           WireGuard connection: %s", nm_active_connection_get_id(connection));

                if (NM_IS_VPN_CONNECTION (connection))
                        g_debug ("           VPN base connection: %s", nm_active_connection_get_specific_object_path (connection));

                if (g_object_get_data (G_OBJECT (connection), "has-state-changed-handler") == NULL) {
                        g_signal_connect_object (connection, "notify::state",
                                                 G_CALLBACK (connection_state_changed), self, G_CONNECT_SWAPPED);
                        g_object_set_data (G_OBJECT (connection), "has-state-changed-handler", GINT_TO_POINTER (TRUE));
                }
        }
}

static void
device_managed_cb (CcNetworkPanel *self, GParamSpec *pspec, NMDevice *device)
{
        if (!nm_device_get_managed (device))
                return;

        panel_add_device (self, device);
        panel_refresh_device_titles (self);
}

static void
device_added_cb (CcNetworkPanel *self, NMDevice *device)
{
        g_debug ("New device added");

        if (nm_device_get_managed (device))
                device_managed_cb (self, NULL, device);
        else
                g_signal_connect_object (device, "notify::managed", G_CALLBACK (device_managed_cb), self, G_CONNECT_SWAPPED);
}

static void
device_removed_cb (CcNetworkPanel *self, NMDevice *device)
{
        g_debug ("Device removed");
        panel_remove_device (self, device);
        panel_refresh_device_titles (self);

        g_signal_handlers_disconnect_by_func (device,
                                              G_CALLBACK (device_managed_cb),
                                              self);
}

static void
manager_running (CcNetworkPanel *self)
{
        const GPtrArray *devices;
        int i;

        /* clear all devices we added */
        if (!nm_client_get_nm_running (self->client)) {
                g_debug ("NM disappeared");
                goto out;
        }

        g_debug ("coldplugging devices");
        devices = nm_client_get_devices (self->client);
        if (devices == NULL) {
                g_debug ("No devices to add");
                return;
        }
        for (i = 0; i < devices->len; i++) {
                NMDevice *device = g_ptr_array_index (devices, i);
                device_added_cb (self, device);
        }
out:
        panel_refresh_device_titles (self);

        g_debug ("Calling handle_argv() after cold-plugging devices");
        handle_argv (self);
}

static gint
sort_vpns_func (GtkListBoxRow *a,
                GtkListBoxRow *b,
                gpointer user_data)
{
        NetVpn *vpn_a = NET_VPN (a);
        NetVpn *vpn_b = NET_VPN (b);

        return g_utf8_collate (nm_connection_get_id (net_vpn_get_connection (vpn_a)),
                               nm_connection_get_id (net_vpn_get_connection (vpn_b)));
}

static void
panel_add_vpn_device (CcNetworkPanel *self, NMConnection *connection)
{
        NetVpn *net_vpn;
        guint i;

        /* does already exist */
        for (i = 0; i < self->vpns->len; i++) {
                net_vpn = g_ptr_array_index (self->vpns, i);
                if (net_vpn_get_connection (net_vpn) == connection)
                        return;
        }

        net_vpn = net_vpn_new (self->client, connection);
        gtk_list_box_append (GTK_LIST_BOX (self->box_vpn), GTK_WIDGET (net_vpn));

        /* store in the devices array */
        g_ptr_array_add (self->vpns, net_vpn);

        /* update vpn widgets */
        update_vpn_section (self);
}

static void
add_connection (CcNetworkPanel *self, NMConnection *connection)
{
        NMSettingConnection *s_con;
        const gchar *type;

        s_con = NM_SETTING_CONNECTION (nm_connection_get_setting (connection,
                                                                  NM_TYPE_SETTING_CONNECTION));
        type = nm_setting_connection_get_connection_type (s_con);
        if (g_strcmp0 (type, "vpn") != 0 && g_strcmp0 (type, "wireguard") != 0)
                return;

        /* Don't add the libvirtd bridge to the UI */
        if (g_strcmp0 (nm_setting_connection_get_interface_name (s_con), "virbr0") == 0)
                return;

        g_debug ("add %s/%s remote connection: %s",
                 type, g_type_name_from_instance ((GTypeInstance*)connection),
                 nm_connection_get_path (connection));
        panel_add_vpn_device (self, connection);
}

static void
client_connection_removed_cb (CcNetworkPanel *self, NMConnection *connection)
{
        guint i;

        for (i = 0; i < self->vpns->len; i++) {
                NetVpn *vpn = g_ptr_array_index (self->vpns, i);
                if (net_vpn_get_connection (vpn) == connection) {
                        g_ptr_array_remove (self->vpns, vpn);
                        gtk_list_box_remove (GTK_LIST_BOX (self->box_vpn), GTK_WIDGET (vpn));
                        update_vpn_section (self);
                        return;
                }
        }
}

static void
panel_check_network_manager_version (CcNetworkPanel *self)
{
        const gchar *version;

        /* parse running version */
        version = nm_client_get_version (self->client);
        if (version == NULL) {
                GtkWidget *status_page;

                status_page = adw_status_page_new ();
                adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (self->toolbar_view), status_page);

                adw_status_page_set_icon_name (ADW_STATUS_PAGE (status_page), "network-error-symbolic");
                adw_status_page_set_title (ADW_STATUS_PAGE (status_page), _("Network Unavailable"));
                adw_status_page_set_description (ADW_STATUS_PAGE (status_page),
                                                 _("An error has occurred and network cannot be used."
                                                   "\n Error details: NetworkManager not running."));

        } else {
                manager_running (self);
        }
}

static void
create_connection_cb (CcNetworkPanel *self)
{
        NetConnectionEditor *editor;

        editor = net_connection_editor_new (NULL, NULL, NULL, self->client);
        gtk_window_set_transient_for (GTK_WINDOW (editor),
                                      GTK_WINDOW (gtk_widget_get_native (GTK_WIDGET (self))));
        gtk_window_present (GTK_WINDOW (editor));
}

static void
cc_network_panel_map (GtkWidget *widget)
{
        GTK_WIDGET_CLASS (cc_network_panel_parent_class)->map (widget);

        /* is the user compiling against a new version, but not running
         * the daemon? */
        panel_check_network_manager_version (CC_NETWORK_PANEL (widget));
}


static void
cc_network_panel_class_init (CcNetworkPanelClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	panel_class->get_help_uri = cc_network_panel_get_help_uri;

        widget_class->map = cc_network_panel_map;

        object_class->get_property = cc_network_panel_get_property;
        object_class->set_property = cc_network_panel_set_property;
        object_class->dispose = cc_network_panel_dispose;
        object_class->finalize = cc_network_panel_finalize;

        g_object_class_override_property (object_class, PROP_PARAMETERS, "parameters");

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/cc-network-panel.ui");

        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, box_bluetooth);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, box_vpn);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, box_wired);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, container_bluetooth);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, empty_listbox);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, proxy_row);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, vpn_stack);
        gtk_widget_class_bind_template_child (widget_class, CcNetworkPanel, toolbar_view);

        gtk_widget_class_bind_template_callback (widget_class, create_connection_cb);

        g_type_ensure (CC_TYPE_LIST_ROW);
        g_type_ensure (CC_TYPE_NET_PROXY_PAGE);
}

static void
cc_network_panel_init (CcNetworkPanel *self)
{
        g_autoptr(GDBusConnection) system_bus = NULL;
        g_autoptr(GError) error = NULL;
        const GPtrArray *connections;
        guint i;

        g_resources_register (cc_network_get_resource ());

        gtk_widget_init_template (GTK_WIDGET (self));

        self->bluetooth_devices = g_ptr_array_new ();
        self->ethernet_devices = g_ptr_array_new ();
        self->mobile_devices = g_ptr_array_new ();
        self->vpns = g_ptr_array_new ();
        self->nm_device_to_device = g_hash_table_new (g_direct_hash, g_direct_equal);

        gtk_list_box_set_sort_func (GTK_LIST_BOX (self->box_vpn), sort_vpns_func, NULL, NULL);

        /* Create and store a NMClient instance if it doesn't exist yet */
        if (!cc_object_storage_has_object (CC_OBJECT_NMCLIENT)) {
                g_autoptr(NMClient) client = nm_client_new (NULL, NULL);
                cc_object_storage_add_object (CC_OBJECT_NMCLIENT, client);
        }

        /* use NetworkManager client */
        self->client = cc_object_storage_get_object (CC_OBJECT_NMCLIENT);

        g_signal_connect_object (self->client, "notify::nm-running" ,
                                 G_CALLBACK (manager_running), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->client, "notify::active-connections",
                                 G_CALLBACK (active_connections_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->client, "device-added",
                                 G_CALLBACK (device_added_cb), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->client, "device-removed",
                                 G_CALLBACK (device_removed_cb), self, G_CONNECT_SWAPPED);

        /* Setup ModemManager client */
        system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
        if (system_bus == NULL) {
                g_warning ("Error connecting to system D-Bus: %s",
                           error->message);
        } else {
                self->modem_manager = mm_manager_new_sync (system_bus,
                                                            G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                                                            NULL,
                                                            &error);
                if (self->modem_manager == NULL)
                        g_warning ("Error connecting to ModemManager: %s",
                                   error->message);
        }

        /* add remote settings such as VPN settings as virtual devices */
        g_signal_connect_object (self->client, NM_CLIENT_CONNECTION_ADDED,
                                 G_CALLBACK (add_connection), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->client, NM_CLIENT_CONNECTION_REMOVED,
                                 G_CALLBACK (client_connection_removed_cb), self, G_CONNECT_SWAPPED);

        /* Cold-plug existing connections */
        connections = nm_client_get_connections (self->client);
        if (connections) {
                for (i = 0; i < connections->len; i++)
                        add_connection (self, connections->pdata[i]);
        }

        g_debug ("Calling handle_argv() after cold-plugging connections");
        handle_argv (self);
}
