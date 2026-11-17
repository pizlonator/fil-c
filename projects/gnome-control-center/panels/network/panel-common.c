/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2012 Thomas Bechtold <thomasbechtold@jpberlin.de>
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

#include <glib/gi18n.h>

#include "panel-common.h"

static const gchar *
device_state_to_localized_string (NMDeviceState state)
{
        const gchar *value = NULL;

        switch (state) {
        case NM_DEVICE_STATE_UNKNOWN:
                /* TRANSLATORS: device status */
                value = _("Status unknown");
                break;
        case NM_DEVICE_STATE_UNMANAGED:
                /* TRANSLATORS: device status */
                value = _("Unmanaged");
                break;
        case NM_DEVICE_STATE_UNAVAILABLE:
                /* TRANSLATORS: device status */
                value = _("Unavailable");
                break;
        case NM_DEVICE_STATE_DISCONNECTED:
                value = NULL;
                break;
        case NM_DEVICE_STATE_PREPARE:
        case NM_DEVICE_STATE_CONFIG:
        case NM_DEVICE_STATE_IP_CONFIG:
        case NM_DEVICE_STATE_IP_CHECK:
                /* TRANSLATORS: device status */
                value = _("Connecting");
                break;
        case NM_DEVICE_STATE_NEED_AUTH:
                /* TRANSLATORS: device status */
                value = _("Authentication required");
                break;
        case NM_DEVICE_STATE_ACTIVATED:
                /* TRANSLATORS: device status */
                value = _("Connected");
                break;
        case NM_DEVICE_STATE_DEACTIVATING:
                /* TRANSLATORS: device status */
                value = _("Disconnecting");
                break;
        case NM_DEVICE_STATE_FAILED:
                /* TRANSLATORS: device status */
                value = _("Connection failed");
                break;
        default:
                /* TRANSLATORS: device status */
                value = _("Status unknown (missing)");
                break;
        }
        return value;
}

static const gchar *
device_state_reason_to_localized_string (NMDevice *device)
{
        const gchar *value = NULL;
        NMDeviceStateReason state_reason;

        /* This only covers NMDeviceStateReasons that explain why a connection
         * failed / can't be attempted, and aren't redundant with the state
         * (eg, NM_DEVICE_STATE_REASON_CARRIER).
         */

        state_reason = nm_device_get_state_reason (device);
        switch (state_reason) {
        case NM_DEVICE_STATE_REASON_CONFIG_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Configuration failed");
                break;
        case NM_DEVICE_STATE_REASON_IP_CONFIG_UNAVAILABLE:
                /* TRANSLATORS: device status reason */
                value = _("IP configuration failed");
                break;
        case NM_DEVICE_STATE_REASON_IP_CONFIG_EXPIRED:
                /* TRANSLATORS: device status reason */
                value = _("IP configuration expired");
                break;
        case NM_DEVICE_STATE_REASON_NO_SECRETS:
                /* TRANSLATORS: device status reason */
                value = _("Secrets were required, but not provided");
                break;
        case NM_DEVICE_STATE_REASON_SUPPLICANT_DISCONNECT:
                /* TRANSLATORS: device status reason */
                value = _("802.1x supplicant disconnected");
                break;
        case NM_DEVICE_STATE_REASON_SUPPLICANT_CONFIG_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("802.1x supplicant configuration failed");
                break;
        case NM_DEVICE_STATE_REASON_SUPPLICANT_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("802.1x supplicant failed");
                break;
        case NM_DEVICE_STATE_REASON_SUPPLICANT_TIMEOUT:
                /* TRANSLATORS: device status reason */
                value = _("802.1x supplicant took too long to authenticate");
                break;
        case NM_DEVICE_STATE_REASON_PPP_START_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("PPP service failed to start");
                break;
        case NM_DEVICE_STATE_REASON_PPP_DISCONNECT:
                /* TRANSLATORS: device status reason */
                value = _("PPP service disconnected");
                break;
        case NM_DEVICE_STATE_REASON_PPP_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("PPP failed");
                break;
        case NM_DEVICE_STATE_REASON_DHCP_START_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("DHCP client failed to start");
                break;
        case NM_DEVICE_STATE_REASON_DHCP_ERROR:
                /* TRANSLATORS: device status reason */
                value = _("DHCP client error");
                break;
        case NM_DEVICE_STATE_REASON_DHCP_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("DHCP client failed");
                break;
        case NM_DEVICE_STATE_REASON_SHARED_START_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Shared connection service failed to start");
                break;
        case NM_DEVICE_STATE_REASON_SHARED_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Shared connection service failed");
                break;
        case NM_DEVICE_STATE_REASON_AUTOIP_START_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("AutoIP service failed to start");
                break;
        case NM_DEVICE_STATE_REASON_AUTOIP_ERROR:
                /* TRANSLATORS: device status reason */
                value = _("AutoIP service error");
                break;
        case NM_DEVICE_STATE_REASON_AUTOIP_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("AutoIP service failed");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_BUSY:
                /* TRANSLATORS: device status reason */
                value = _("Line busy");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_NO_DIAL_TONE:
                /* TRANSLATORS: device status reason */
                value = _("No dial tone");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_NO_CARRIER:
                /* TRANSLATORS: device status reason */
                value = _("No carrier could be established");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_DIAL_TIMEOUT:
                /* TRANSLATORS: device status reason */
                value = _("Dialing request timed out");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_DIAL_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Dialing attempt failed");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_INIT_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Modem initialization failed");
                break;
        case NM_DEVICE_STATE_REASON_GSM_APN_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Failed to select the specified APN");
                break;
        case NM_DEVICE_STATE_REASON_GSM_REGISTRATION_NOT_SEARCHING:
                /* TRANSLATORS: device status reason */
                value = _("Not searching for networks");
                break;
        case NM_DEVICE_STATE_REASON_GSM_REGISTRATION_DENIED:
                /* TRANSLATORS: device status reason */
                value = _("Network registration denied");
                break;
        case NM_DEVICE_STATE_REASON_GSM_REGISTRATION_TIMEOUT:
                /* TRANSLATORS: device status reason */
                value = _("Network registration timed out");
                break;
        case NM_DEVICE_STATE_REASON_GSM_REGISTRATION_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Failed to register with the requested network");
                break;
        case NM_DEVICE_STATE_REASON_GSM_PIN_CHECK_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("PIN check failed");
                break;
        case NM_DEVICE_STATE_REASON_FIRMWARE_MISSING:
                /* TRANSLATORS: device status reason */
                value = _("Firmware for the device may be missing");
                break;
        case NM_DEVICE_STATE_REASON_CONNECTION_REMOVED:
                /* TRANSLATORS: device status reason */
                value = _("Connection disappeared");
                break;
        case NM_DEVICE_STATE_REASON_CONNECTION_ASSUMED:
                /* TRANSLATORS: device status reason */
                value = _("Existing connection was assumed");
                break;
        case NM_DEVICE_STATE_REASON_MODEM_NOT_FOUND:
                /* TRANSLATORS: device status reason */
                value = _("Modem not found");
                break;
        case NM_DEVICE_STATE_REASON_BT_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Bluetooth connection failed");
                break;
        case NM_DEVICE_STATE_REASON_GSM_SIM_NOT_INSERTED:
                /* TRANSLATORS: device status reason */
                value = _("SIM Card not inserted");
                break;
        case NM_DEVICE_STATE_REASON_GSM_SIM_PIN_REQUIRED:
                /* TRANSLATORS: device status reason */
                value = _("SIM Pin required");
                break;
        case NM_DEVICE_STATE_REASON_GSM_SIM_PUK_REQUIRED:
                /* TRANSLATORS: device status reason */
                value = _("SIM Puk required");
                break;
        case NM_DEVICE_STATE_REASON_GSM_SIM_WRONG:
                /* TRANSLATORS: device status reason */
                value = _("SIM wrong");
                break;
        case NM_DEVICE_STATE_REASON_DEPENDENCY_FAILED:
                /* TRANSLATORS: device status reason */
                value = _("Connection dependency failed");
                break;
        default:
                /* no StateReason to show */
                value = "";
                break;
        }

        return value;
}

static gchar *
get_mac_address_of_connection (NMConnection *connection)
{
        if (!connection)
                return NULL;

        /* check the connection type */
        if (nm_connection_is_type (connection,
                                   NM_SETTING_WIRELESS_SETTING_NAME)) {
                /* check wireless settings */
                NMSettingWireless *s_wireless = nm_connection_get_setting_wireless (connection);
                if (!s_wireless)
                        return NULL;
                return g_strdup (nm_setting_wireless_get_mac_address (s_wireless));
        } else if (nm_connection_is_type (connection,
                                          NM_SETTING_WIRED_SETTING_NAME)) {
                /* check wired settings */
                NMSettingWired *s_wired = nm_connection_get_setting_wired (connection);
                if (!s_wired)
                        return NULL;
                return g_strdup (nm_setting_wired_get_mac_address (s_wired));
        }
        /* no MAC address found */
        return NULL;
}

/* returns TRUE if both MACs are equal */
static gboolean
compare_mac_device_with_mac_connection (NMDevice *device,
                                        NMConnection *connection)
{
        const gchar *mac_dev = NULL;
        g_autofree gchar *mac_conn = NULL;

        mac_dev = nm_device_get_hw_address (device);
        if (mac_dev == NULL)
                return FALSE;

        mac_conn = get_mac_address_of_connection (connection);
        if (mac_conn == NULL)
                return FALSE;

        /* compare both MACs */
        return g_strcmp0 (mac_dev, mac_conn) == 0;
}

gchar *
panel_device_status_to_localized_string (NMDevice *nm_device,
                                         const gchar *speed)
{
        NMDeviceState state;
        GString *string;
        const gchar *state_str = NULL, *reason_str = NULL;

        string = g_string_new (NULL);

        state = nm_device_get_state (nm_device);
        if (state == NM_DEVICE_STATE_UNAVAILABLE) {
                if (nm_device_get_firmware_missing (nm_device)) {
                        /* TRANSLATORS: device status */
                        state_str = _("Firmware missing");
                } else if (NM_IS_DEVICE_ETHERNET (nm_device) &&
                           !nm_device_ethernet_get_carrier (NM_DEVICE_ETHERNET (nm_device))) {
                        /* TRANSLATORS: device status */
                        state_str = _("Cable unplugged");
                }
        }
        if (!state_str)
                state_str = device_state_to_localized_string (state);
        if (state_str)
                g_string_append (string, state_str);

        if (state > NM_DEVICE_STATE_UNAVAILABLE && speed) {
                if (string->len)
                        g_string_append (string, " - ");
                g_string_append (string, speed);
        } else if (state == NM_DEVICE_STATE_UNAVAILABLE ||
                   state == NM_DEVICE_STATE_DISCONNECTED ||
                   state == NM_DEVICE_STATE_DEACTIVATING ||
                   state == NM_DEVICE_STATE_FAILED) {
                reason_str = device_state_reason_to_localized_string (nm_device);
                if (*reason_str) {
                        if (string->len)
                                g_string_append (string, " - ");
                        g_string_append (string, reason_str);
                }
        }

        return g_string_free (string, FALSE);
}

NMConnection *
net_device_get_find_connection (NMClient *client, NMDevice *device)
{
        GSList *list, *iterator;
        NMConnection *connection = NULL;
        NMActiveConnection *ac;

        /* is the device available in a active connection? */
        ac = nm_device_get_active_connection (device);
        if (ac)
                return (NMConnection*) nm_active_connection_get_connection (ac);

        /* not found in active connections - check all available connections */
        list = net_device_get_valid_connections (client, device);
        if (list != NULL) {
                /* if list has only one connection, use this connection */
                if (g_slist_length (list) == 1) {
                        connection = list->data;
                        goto out;
                }

                /* is there connection with the MAC address of the device? */
                for (iterator = list; iterator; iterator = iterator->next) {
                        connection = iterator->data;
                        if (compare_mac_device_with_mac_connection (device,
                                                                    connection)) {
                                goto out;
                        }
                }
        }

        /* no connection found for the given device */
        connection = NULL;
out:
        g_slist_free (list);
        return connection;
}

GSList *
net_device_get_valid_connections (NMClient *client, NMDevice *device)
{
        GSList *valid;
        NMConnection *connection;
        NMSettingConnection *s_con;
        NMActiveConnection *active_connection;
        const char *active_uuid;
        const GPtrArray *all;
        GPtrArray *filtered;
        guint i;

        all = nm_client_get_connections (client);
        filtered = nm_device_filter_connections (device, all);

        active_connection = nm_device_get_active_connection (device);
        active_uuid = active_connection ? nm_active_connection_get_uuid (active_connection) : NULL;

        valid = NULL;
        for (i = 0; i < filtered->len; i++) {
                connection = g_ptr_array_index (filtered, i);
                s_con = nm_connection_get_setting_connection (connection);
                if (!s_con)
                        continue;

                if (nm_setting_connection_get_master (s_con) &&
                    g_strcmp0 (nm_setting_connection_get_uuid (s_con), active_uuid) != 0)
                        continue;

                valid = g_slist_prepend (valid, connection);
        }
        g_ptr_array_free (filtered, FALSE);

        return g_slist_reverse (valid);
}

gchar *
net_device_get_ip6_addresses (NMIPConfig *ipv6_config)
{
        g_autoptr(GPtrArray) ipv6 = NULL;
        GPtrArray *addresses;

        addresses = nm_ip_config_get_addresses (ipv6_config);
        if (addresses->len == 0) {
                return NULL;
        }
        ipv6 = g_ptr_array_sized_new (addresses->len + 1);

        for (int i = 0; i < addresses->len; i++) {
                g_ptr_array_add (ipv6, (char *) nm_ip_address_get_address (g_ptr_array_index (addresses, i)));
        }
        g_ptr_array_add (ipv6, NULL);
        return g_strjoinv ("\n", (char **) ipv6->pdata);
}
