/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2012 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Aleksander Morgado <aleksander@gnu.org>
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
#include <libmm-glib.h>
#include <nma-mobile-providers.h>

#include "panel-common.h"
#include "network-dialogs.h"
#include "net-device-mobile.h"

static void nm_device_mobile_refresh_ui (NetDeviceMobile *self);

struct _NetDeviceMobile
{
        GtkBox        parent;

        GtkLabel     *device_label;
        GtkSwitch    *device_off_switch;
        GtkLabel     *dns4_heading_label;
        GtkLabel     *dns4_label;
        GtkLabel     *dns6_heading_label;
        GtkLabel     *dns6_label;
        GtkLabel     *imei_heading_label;
        GtkLabel     *imei_label;
        GtkLabel     *ipv4_heading_label;
        GtkLabel     *ipv4_label;
        GtkLabel     *ipv6_heading_label;
        GtkLabel     *ipv6_label;
        GtkListStore *mobile_connections_list_store;
        GtkComboBox  *network_combo;
        GtkLabel     *network_label;
        GtkButton    *options_button;
        GtkLabel     *provider_heading_label;
        GtkLabel     *provider_label;
        GtkLabel     *route_heading_label;
        GtkLabel     *route_label;
        GtkLabel     *status_label;

        NMClient     *client;
        NMDevice     *device;
        GDBusObject  *modem;
        GCancellable *cancellable;

        gboolean    updating_device;

        /* Old MM < 0.7 support */
        GDBusProxy *gsm_proxy;
        GDBusProxy *cdma_proxy;

        /* New MM >= 0.7 support */
        MMObject   *mm_object;

        NMAMobileProvidersDatabase *mpd;
};

enum {
        COLUMN_ID,
        COLUMN_TITLE,
        COLUMN_LAST
};

G_DEFINE_TYPE (NetDeviceMobile, net_device_mobile, GTK_TYPE_BOX)

static void
connection_activate_cb (GObject *source_object,
                        GAsyncResult *res,
                        gpointer user_data)
{
        g_autoptr(GError) error = NULL;

        if (!nm_client_activate_connection_finish (NM_CLIENT (source_object), res, &error)) {
                /* failed to activate */
                nm_device_mobile_refresh_ui (user_data);
        }
}

static void
network_combo_changed_cb (NetDeviceMobile *self)
{
        gboolean ret;
        g_autofree gchar *object_path = NULL;
        GtkTreeIter iter;
        GtkTreeModel *model;
        NMConnection *connection;

        if (self->updating_device)
                return;

        ret = gtk_combo_box_get_active_iter (self->network_combo, &iter);
        if (!ret)
                return;

        /* get entry */
        model = gtk_combo_box_get_model (self->network_combo);
        gtk_tree_model_get (model, &iter,
                            COLUMN_ID, &object_path,
                            -1);
        if (g_strcmp0 (object_path, NULL) == 0) {
                GtkNative *native = gtk_widget_get_native (GTK_WIDGET (self));
                cc_network_panel_connect_to_3g_network (GTK_WIDGET (native), self->client, self->device);
                return;
        }

        /* activate the connection */
        g_debug ("try to switch to connection %s", object_path);
        connection = (NMConnection*) nm_client_get_connection_by_path (self->client, object_path);
        if (connection != NULL) {
                nm_device_disconnect_async (self->device, NULL, NULL, NULL);
                nm_client_activate_connection_async (self->client,
                                                     connection,
                                                     self->device, NULL, NULL,
                                                     connection_activate_cb,
                                                     self);
                return;
        }
}

static void
mobilebb_enabled_toggled (NetDeviceMobile *self)
{
        gboolean enabled = FALSE;

        if (nm_client_wwan_get_enabled (self->client)) {
                NMDeviceState state;

                state = nm_device_get_state (self->device);
                if (state == NM_DEVICE_STATE_UNKNOWN ||
                    state == NM_DEVICE_STATE_UNMANAGED ||
                    state == NM_DEVICE_STATE_UNAVAILABLE ||
                    state == NM_DEVICE_STATE_DISCONNECTED ||
                    state == NM_DEVICE_STATE_DEACTIVATING ||
                    state == NM_DEVICE_STATE_FAILED) {
                        enabled = FALSE;
                } else {
                        enabled = TRUE;
                }
        }

        self->updating_device = TRUE;
        gtk_switch_set_active (self->device_off_switch, enabled);
        self->updating_device = FALSE;
}

static void
device_add_device_connections (NetDeviceMobile *self,
                               NMDevice *nm_device,
                               GtkListStore *liststore,
                               GtkComboBox *combobox)
{
        GSList *list, *l;
        GtkTreeIter treeiter;
        NMActiveConnection *active_connection;
        NMConnection *connection;

        /* get the list of available connections for this device */
        list = net_device_get_valid_connections (self->client, nm_device);
        gtk_list_store_clear (liststore);
        active_connection = nm_device_get_active_connection (nm_device);
        for (l = list; l; l = g_slist_next (l)) {
                connection = NM_CONNECTION (l->data);
                gtk_list_store_append (liststore, &treeiter);
                gtk_list_store_set (liststore,
                                    &treeiter,
                                    COLUMN_ID, nm_connection_get_uuid (connection),
                                    COLUMN_TITLE, nm_connection_get_id (connection),
                                    -1);

                /* is this already activated? */
                if (active_connection != NULL &&
                    g_strcmp0 (nm_connection_get_uuid (connection),
                               nm_active_connection_get_uuid (active_connection)) == 0) {
                        self->updating_device = TRUE;
                        gtk_combo_box_set_active_iter (combobox, &treeiter);
                        self->updating_device = FALSE;
                }
        }

        /* add new connection entry */
        gtk_list_store_append (liststore, &treeiter);
        gtk_list_store_set (liststore,
                            &treeiter,
                            COLUMN_ID, NULL,
                            COLUMN_TITLE, _("Add new connection"),
                            -1);

        g_slist_free (list);
}

static void
device_mobile_refresh_equipment_id (NetDeviceMobile *self)
{
        const gchar *equipment_id = NULL;

        if (self->mm_object != NULL) {
                MMModem *modem;

                /* Modem interface should always be present */
                modem = mm_object_peek_modem (self->mm_object);
                equipment_id = mm_modem_get_equipment_identifier (modem);

                /* Set equipment ID */
                if (equipment_id != NULL) {
                        g_debug ("[%s] Equipment ID set to '%s'",
                                 mm_object_get_path (self->mm_object),
                                 equipment_id);
                }
        } else {
                /* Assume old MM handling */
                equipment_id = g_object_get_data (G_OBJECT (self),
                                                  "ControlCenter::EquipmentIdentifier");
        }

        gtk_label_set_label (self->imei_label, equipment_id);
        gtk_widget_set_visible (GTK_WIDGET (self->imei_heading_label), equipment_id != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->imei_label), equipment_id != NULL);
}

static gchar *
device_mobile_find_provider (NetDeviceMobile *self,
                             const gchar     *mccmnc,
                             guint32          sid)
{
        NMAMobileProvider *provider;
        GString *name = NULL;

        if (self->mpd == NULL) {
                g_autoptr(GError) error = NULL;

                /* Use defaults */
                self->mpd = nma_mobile_providers_database_new_sync (NULL, NULL, NULL, &error);
                if (self->mpd == NULL) {
                        g_debug ("Couldn't load mobile providers database: %s",
                                 error ? error->message : "");
                        return NULL;
                }
        }

        if (mccmnc != NULL) {
                provider = nma_mobile_providers_database_lookup_3gpp_mcc_mnc (self->mpd, mccmnc);
                if (provider != NULL)
                        name = g_string_new (nma_mobile_provider_get_name (provider));
        }

        if (sid != 0) {
                provider = nma_mobile_providers_database_lookup_cdma_sid (self->mpd, sid);
                if (provider != NULL) {
                        if (name == NULL)
                                name = g_string_new (nma_mobile_provider_get_name (provider));
                        else
                                g_string_append_printf (name, ", %s", nma_mobile_provider_get_name (provider));
                }
        }

        return (name != NULL ? g_string_free (name, FALSE) : NULL);
}

static void
device_mobile_refresh_operator_name (NetDeviceMobile *self)
{
        g_autofree gchar *operator_name = NULL;

        if (self->mm_object != NULL) {
                MMModem3gpp *modem_3gpp;
                MMModemCdma *modem_cdma;

                modem_3gpp = mm_object_peek_modem_3gpp (self->mm_object);
                modem_cdma = mm_object_peek_modem_cdma (self->mm_object);

                if (modem_3gpp != NULL) {
                        const gchar *operator_name_unsafe;

                        operator_name_unsafe = mm_modem_3gpp_get_operator_name (modem_3gpp);
                        if (operator_name_unsafe != NULL && operator_name_unsafe[0] != '\0')
                                operator_name = g_strescape (operator_name_unsafe, NULL);
                }

                /* If not directly given in the 3GPP interface, try to guess from
                 * MCCMNC/SID */
                if (operator_name == NULL) {
                        const gchar *mccmnc = NULL;
                        guint32 sid = 0;

                        if (modem_3gpp != NULL)
                                mccmnc = mm_modem_3gpp_get_operator_code (modem_3gpp);
                        if (modem_cdma != NULL)
                                sid = mm_modem_cdma_get_sid (modem_cdma);
                        operator_name = device_mobile_find_provider (self, mccmnc, sid);
                }

                /* Set operator name */
                if (operator_name != NULL) {
                        g_debug ("[%s] Operator name set to '%s'",
                                 mm_object_get_path (self->mm_object),
                                 operator_name);
                }

        } else {
                const gchar *gsm;
                const gchar *cdma;

                /* Assume old MM handling */
                gsm = g_object_get_data (G_OBJECT (self),
                                         "ControlCenter::OperatorNameGsm");
                cdma = g_object_get_data (G_OBJECT (self),
                                          "ControlCenter::OperatorNameCdma");

                if (gsm != NULL && cdma != NULL)
                        operator_name = g_strdup_printf ("%s, %s", gsm, cdma);
                else if (gsm != NULL)
                        operator_name = g_strdup (gsm);
                else if (cdma != NULL)
                        operator_name = g_strdup (cdma);
        }

        gtk_label_set_label (self->provider_label, operator_name);
        gtk_widget_set_visible (GTK_WIDGET (self->provider_heading_label), operator_name != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->provider_label), operator_name != NULL);
}

static void
nm_device_mobile_refresh_ui (NetDeviceMobile *self)
{
        gboolean is_connected;
        NMDeviceModemCapabilities caps;
        g_autofree gchar *status = NULL;
        NMIPConfig *ipv4_config = NULL, *ipv6_config = NULL;
        gboolean have_ipv4_address = FALSE, have_ipv6_address = FALSE;
        gboolean have_dns4 = FALSE, have_dns6 = FALSE;
        const gchar *route4_text = NULL, *route6_text = NULL;

        /* set up the device on/off switch */
        gtk_widget_set_visible (GTK_WIDGET (self->device_off_switch), TRUE);
        mobilebb_enabled_toggled (self);

        /* set device state, with status */
        status = panel_device_status_to_localized_string (self->device, NULL);
        gtk_label_set_label (self->status_label, status);

        /* sensitive for other connection types if the device is currently connected */
        is_connected = net_device_get_find_connection (self->client, self->device) != NULL;
        gtk_widget_set_sensitive (GTK_WIDGET (self->options_button), is_connected);

        caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (self->device));
        if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
            (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) ||
            (caps & NM_DEVICE_MODEM_CAPABILITY_LTE)) {
                device_mobile_refresh_operator_name (self);
                device_mobile_refresh_equipment_id (self);
        }

        /* add possible connections to device */
        device_add_device_connections (self,
                                       self->device,
                                       self->mobile_connections_list_store,
                                       self->network_combo);

        ipv4_config = nm_device_get_ip4_config (self->device);
        if (ipv4_config != NULL) {
                GPtrArray *addresses;
                const gchar *ipv4_text = NULL;
                g_autofree gchar *ip4_dns = NULL;

                addresses = nm_ip_config_get_addresses (ipv4_config);
                if (addresses->len > 0)
                        ipv4_text = nm_ip_address_get_address (g_ptr_array_index (addresses, 0));
                gtk_label_set_label (self->ipv4_label, ipv4_text);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv4_heading_label), ipv4_text != NULL);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv4_label), ipv4_text != NULL);
                have_ipv4_address = ipv4_text != NULL;

                ip4_dns = g_strjoinv (" ", (char **) nm_ip_config_get_nameservers (ipv4_config));
                if (!*ip4_dns)
                        ip4_dns = NULL;
                gtk_label_set_label (self->dns4_label, ip4_dns);
                gtk_widget_set_visible (GTK_WIDGET (self->dns4_heading_label), ip4_dns != NULL);
                gtk_widget_set_visible (GTK_WIDGET (self->dns4_label), ip4_dns != NULL);
                have_dns4 = ip4_dns != NULL;

                route4_text = nm_ip_config_get_gateway (ipv4_config);
        } else {
                gtk_widget_set_visible (GTK_WIDGET (self->ipv4_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv4_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->dns4_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->dns4_label), FALSE);
        }

        ipv6_config = nm_device_get_ip6_config (self->device);
        if (ipv6_config != NULL) {
                g_autofree gchar *ipv6_text = NULL;
                g_autofree gchar *ip6_dns = NULL;

                ipv6_text = net_device_get_ip6_addresses (ipv6_config);
                gtk_label_set_label (self->ipv6_label, ipv6_text);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv6_heading_label), ipv6_text != NULL);
                gtk_widget_set_valign (GTK_WIDGET (self->ipv6_heading_label), GTK_ALIGN_START);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv6_label), ipv6_text != NULL);
                have_ipv6_address = ipv6_text != NULL;

                ip6_dns = g_strjoinv (" ", (char **) nm_ip_config_get_nameservers (ipv6_config));
                if (!*ip6_dns)
                        ip6_dns = NULL;
                gtk_label_set_label (self->dns6_label, ip6_dns);
                gtk_widget_set_visible (GTK_WIDGET (self->dns6_heading_label), ip6_dns != NULL);
                gtk_widget_set_visible (GTK_WIDGET (self->dns6_label), ip6_dns != NULL);
                have_dns6 = ip6_dns != NULL;

                route6_text =  nm_ip_config_get_gateway (ipv6_config);
        } else {
                gtk_widget_set_visible (GTK_WIDGET (self->ipv6_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv6_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->dns6_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->dns6_label), FALSE);
        }

        if (have_ipv4_address && have_ipv6_address) {
                gtk_label_set_label (self->ipv4_heading_label, _("IPv4 Address"));
                gtk_label_set_label (self->ipv6_heading_label, _("IPv6 Address"));
        }
        else {
                gtk_label_set_label (self->ipv4_heading_label, _("IP Address"));
                gtk_label_set_label (self->ipv6_heading_label, _("IP Address"));
        }

        if (have_dns4 && have_dns6) {
                gtk_label_set_label (self->dns4_heading_label, _("DNS4"));
                gtk_label_set_label (self->dns6_heading_label, _("DNS6"));
        } else {
                gtk_label_set_label (self->dns4_heading_label, _("DNS"));
                gtk_label_set_label (self->dns6_heading_label, _("DNS"));
        }

        if (route4_text != NULL || route6_text != NULL) {
                g_autofree const gchar *routes_text = NULL;

                if (route4_text == NULL) {
                        routes_text = g_strdup (route6_text);
                } else if (route6_text == NULL) {
                        routes_text = g_strdup (route4_text);
                } else {
                        routes_text = g_strjoin ("\n", route4_text, route6_text, NULL);
                }
                gtk_label_set_label (self->route_label, routes_text);
                gtk_widget_set_visible (GTK_WIDGET (self->route_heading_label), routes_text != NULL);
                gtk_widget_set_valign (GTK_WIDGET (self->route_heading_label), GTK_ALIGN_START);
                gtk_widget_set_visible (GTK_WIDGET (self->route_label), routes_text != NULL);
        } else {
                gtk_widget_set_visible (GTK_WIDGET (self->route_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->route_label), FALSE);
        }
}

static void
device_off_switch_changed_cb (NetDeviceMobile *self)
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
options_button_clicked_cb (NetDeviceMobile *self)
{
        const gchar *uuid;
        g_autofree gchar *cmdline = NULL;
        g_autoptr(GError) error = NULL;
        NMConnection *connection;

        connection = net_device_get_find_connection (self->client, self->device);
        uuid = nm_connection_get_uuid (connection);
        cmdline = g_strdup_printf ("nm-connection-editor --edit %s", uuid);
        g_debug ("Launching '%s'\n", cmdline);
        if (!g_spawn_command_line_async (cmdline, &error))
                g_warning ("Failed to launch nm-connection-editor: %s", error->message);
}

static void
device_mobile_device_got_modem_manager_cb (GObject *source_object,
                                           GAsyncResult *res,
                                           gpointer user_data)
{
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) result = NULL;
        g_autoptr(GDBusProxy) proxy = NULL;
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;

        proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (!proxy) {
                g_warning ("Error creating ModemManager proxy: %s",
                           error->message);
                return;
        }

        /* get the IMEI */
        result = g_dbus_proxy_get_cached_property (proxy,
                                                   "EquipmentIdentifier");

        /* save */
        if (result)
                g_object_set_data_full (G_OBJECT (self),
                                        "ControlCenter::EquipmentIdentifier",
                                        g_variant_dup_string (result, NULL),
                                        g_free);

        device_mobile_refresh_equipment_id (self);
}

static void
device_mobile_save_operator_name (NetDeviceMobile *self,
                                  const gchar     *field,
                                  const gchar     *operator_name)
{
        gchar *operator_name_safe = NULL;

        if (operator_name != NULL && operator_name[0] != '\0')
                operator_name_safe = g_strescape (operator_name, NULL);

        /* save */
        g_object_set_data_full (G_OBJECT (self),
                                field,
                                operator_name_safe,
                                g_free);
        /* refresh */
        device_mobile_refresh_operator_name (self);
}

static void
device_mobile_get_registration_info_cb (GObject      *source_object,
                                        GAsyncResult *res,
                                        gpointer      user_data)
{
        g_autofree gchar *operator_code = NULL;
        g_autoptr(GError) error = NULL;
        guint registration_status;
        g_autoptr(GVariant) result = NULL;
        g_autofree gchar *operator_name = NULL;
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting registration info: %s\n",
                           error->message);
                return;
        }

        /* get values */
        g_variant_get (result, "((uss))",
                       &registration_status,
                       &operator_code,
                       &operator_name);

        /* If none give, try to guess it */
        if (operator_name == NULL || operator_name[0] == '\0') {
                g_free (operator_name);
                operator_name = device_mobile_find_provider (self, operator_code, 0);
        }

        /* save and refresh */
        device_mobile_save_operator_name (self,
                                          "ControlCenter::OperatorNameGsm",
                                          operator_name);
}

static void
device_mobile_gsm_signal_cb (NetDeviceMobile *self,
                             const gchar     *sender_name,
                             const gchar     *signal_name,
                             GVariant        *parameters)
{
        guint registration_status = 0;
        g_autofree gchar *operator_code = NULL;
        g_autofree gchar *operator_name = NULL;

        if (!g_str_equal (signal_name, "RegistrationInfo"))
                return;

        g_variant_get (parameters,
                       "(uss)",
                       &registration_status,
                       &operator_code,
                       &operator_name);

        /* If none given, try to guess it */
        if (operator_name == NULL || operator_name[0] == '\0') {
                g_free (operator_name);
                operator_name = device_mobile_find_provider (self, operator_code, 0);
        }

        /* save and refresh */
        device_mobile_save_operator_name (self,
                                          "ControlCenter::OperatorNameGsm",
                                          operator_name);
}

static void
device_mobile_device_got_modem_manager_gsm_cb (GObject      *source_object,
                                               GAsyncResult *res,
                                               gpointer      user_data)
{
        g_autoptr(GError) error = NULL;
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;

        self->gsm_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (self->gsm_proxy == NULL) {
                g_warning ("Error creating ModemManager GSM proxy: %s\n",
                           error->message);
                return;
        }

        /* Setup value updates */
        g_signal_connect_object (self->gsm_proxy,
                                 "g-signal",
                                 G_CALLBACK (device_mobile_gsm_signal_cb),
                                 self,
                                 G_CONNECT_SWAPPED);

        /* Load initial value */
        g_dbus_proxy_call (self->gsm_proxy,
                           "GetRegistrationInfo",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           device_mobile_get_registration_info_cb,
                           self);
}

static void
device_mobile_get_serving_system_cb (GObject      *source_object,
                                     GAsyncResult *res,
                                     gpointer      user_data)
{
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;
        g_autoptr(GVariant) result = NULL;
        g_autoptr(GError) error = NULL;

        guint32 band_class;
        g_autofree gchar *band = NULL;
        guint32 sid;
        gchar *operator_name;

        result = g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
        if (result == NULL) {
                g_warning ("Error getting serving system: %s\n",
                           error->message);
                return;
        }

        /* get values */
        g_variant_get (result, "((usu))",
                       &band_class,
                       &band,
                       &sid);

        operator_name = device_mobile_find_provider (self, NULL, sid);

        /* save and refresh */
        device_mobile_save_operator_name (self,
                                          "ControlCenter::OperatorNameCdma",
                                          operator_name);
}

static void
device_mobile_device_got_modem_manager_cdma_cb (GObject      *source_object,
                                                GAsyncResult *res,
                                                gpointer      user_data)
{
        g_autoptr(GError) error = NULL;
        NetDeviceMobile *self = (NetDeviceMobile *)user_data;

        self->cdma_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
        if (self->cdma_proxy == NULL) {
                g_warning ("Error creating ModemManager CDMA proxy: %s\n",
                           error->message);
                return;
        }

        /* Load initial value */
        g_dbus_proxy_call (self->cdma_proxy,
                           "GetServingSystem",
                           NULL,
                           G_DBUS_CALL_FLAGS_NONE,
                           -1,
                           NULL,
                           device_mobile_get_serving_system_cb,
                           self);
}

static void
operator_name_updated (NetDeviceMobile *self)
{
        device_mobile_refresh_operator_name (self);
}

static void
net_device_mobile_dispose (GObject *object)
{
        NetDeviceMobile *self = NET_DEVICE_MOBILE (object);

        g_cancellable_cancel (self->cancellable);

        g_clear_object (&self->client);
        g_clear_object (&self->device);
        g_clear_object (&self->modem);
        g_clear_object (&self->cancellable);
        g_clear_object (&self->gsm_proxy);
        g_clear_object (&self->cdma_proxy);
        g_clear_object (&self->mm_object);
        g_clear_object (&self->mpd);

        G_OBJECT_CLASS (net_device_mobile_parent_class)->dispose (object);
}

static void
net_device_mobile_class_init (NetDeviceMobileClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = net_device_mobile_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/network-mobile.ui");

        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, device_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, device_off_switch);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, dns4_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, dns4_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, dns6_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, dns6_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, imei_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, imei_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, ipv4_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, ipv4_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, ipv6_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, ipv6_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, mobile_connections_list_store);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, network_combo);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, network_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, options_button);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, provider_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, provider_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, route_heading_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, route_label);
        gtk_widget_class_bind_template_child (widget_class, NetDeviceMobile, status_label);

        gtk_widget_class_bind_template_callback (widget_class, device_off_switch_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, network_combo_changed_cb);
        gtk_widget_class_bind_template_callback (widget_class, options_button_clicked_cb);
}

static void
net_device_mobile_init (NetDeviceMobile *self)
{
        g_autofree gchar *path = NULL;

        gtk_widget_init_template (GTK_WIDGET (self));

        self->cancellable = g_cancellable_new ();

        path = g_find_program_in_path ("nm-connection-editor");
        gtk_widget_set_visible (GTK_WIDGET (self->options_button), path != NULL);
}

NetDeviceMobile *
net_device_mobile_new (NMClient *client, NMDevice *device, GDBusObject *modem)
{
        NetDeviceMobile *self;
        NMDeviceModemCapabilities caps;

        self = g_object_new (net_device_mobile_get_type (), NULL);
        self->client = g_object_ref (client);
        self->device = g_object_ref (device);

        g_signal_connect_object (device, "state-changed", G_CALLBACK (nm_device_mobile_refresh_ui), self, G_CONNECT_SWAPPED);

        if (modem != NULL)  {
                MMModem3gpp *modem_3gpp;

                self->modem = g_object_ref (modem);

                /* Load equipment ID initially */
                device_mobile_refresh_equipment_id (self);

                /* Follow changes in operator name and load initial values */
                modem_3gpp = mm_object_peek_modem_3gpp (self->mm_object);
                if (modem_3gpp != NULL) {
                        g_signal_connect_object (modem_3gpp,
                                                 "notify::operator-name",
                                                 G_CALLBACK (operator_name_updated),
                                                 self,
                                                 G_CONNECT_SWAPPED);
                        device_mobile_refresh_operator_name (self);
                }
        }

        caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));

        /* Only load proxies if we have broadband modems of the OLD ModemManager interface */
        if (g_str_has_prefix (nm_device_get_udi (device), "/org/freedesktop/ModemManager/") &&
            ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
             (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) ||
             (caps & NM_DEVICE_MODEM_CAPABILITY_LTE))) {
                g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                          G_DBUS_PROXY_FLAGS_NONE,
                                          NULL,
                                          "org.freedesktop.ModemManager",
                                          nm_device_get_udi (device),
                                          "org.freedesktop.ModemManager.Modem",
                                          self->cancellable,
                                          device_mobile_device_got_modem_manager_cb,
                                          self);

                if ((caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) ||
                    (caps & NM_DEVICE_MODEM_CAPABILITY_LTE)) {
                        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  "org.freedesktop.ModemManager",
                                                  nm_device_get_udi (device),
                                                  "org.freedesktop.ModemManager.Modem.Gsm.Network",
                                                  self->cancellable,
                                                  device_mobile_device_got_modem_manager_gsm_cb,
                                                  self);
                }

                if (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) {
                        g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                                                  G_DBUS_PROXY_FLAGS_NONE,
                                                  NULL,
                                                  "org.freedesktop.ModemManager",
                                                  nm_device_get_udi (device),
                                                  "org.freedesktop.ModemManager.Modem.Cdma",
                                                  self->cancellable,
                                                  device_mobile_device_got_modem_manager_cdma_cb,
                                                  self);
                }
        }

        g_signal_connect_object (client, "notify::wwan-enabled",
                                 G_CALLBACK (mobilebb_enabled_toggled),
                                 self, G_CONNECT_SWAPPED);
        nm_device_mobile_refresh_ui (self);

        return self;
}

NMDevice *
net_device_mobile_get_device (NetDeviceMobile *self)
{
        g_return_val_if_fail (NET_IS_DEVICE_MOBILE (self), NULL);
        return self->device;
}

void
net_device_mobile_set_title (NetDeviceMobile *self, const gchar *title)
{
        g_return_if_fail (NET_IS_DEVICE_MOBILE (self));
        gtk_label_set_label (self->device_label, title);
}
