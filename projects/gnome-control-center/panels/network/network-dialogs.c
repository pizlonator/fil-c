/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Giovanni Campagna <scampa.giovanni@gmail.com>
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
 *
 * Portions of this code were taken from network-manager-applet.
 * Copyright 2008 - 2011 Red Hat, Inc. 
 */

#include <NetworkManager.h>
#include <nma-wifi-dialog.h>
#include <nma-mobile-wizard.h>

#include "network-dialogs.h"

typedef struct {
        NMClient *client;
} WirelessDialogClosure;

typedef struct {
        NMClient *client;
        NMDevice *device;
} MobileDialogClosure;

static void
wireless_dialog_closure_closure_notify (gpointer data,
                                        GClosure *gclosure)
{
        WirelessDialogClosure *closure = data;

        g_clear_object (&closure->client);
        g_slice_free (WirelessDialogClosure, data);
}

static void
mobile_dialog_closure_free (gpointer data)
{
        MobileDialogClosure *closure = data;

        g_clear_object (&closure->client);
        g_clear_object (&closure->device);
        g_slice_free (MobileDialogClosure, data);
}

static gboolean
wifi_can_create_wifi_network (NMClient *client)
{
	NMClientPermissionResult perm;

	/* FIXME: check WIFI_SHARE_PROTECTED too, and make the wireless dialog
	 * handle the permissions as well so that admins can restrict open network
	 * creation separately from protected network creation.
	 */
	perm = nm_client_get_permission_result (client, NM_CLIENT_PERMISSION_WIFI_SHARE_OPEN);
	if (perm == NM_CLIENT_PERMISSION_RESULT_YES || perm == NM_CLIENT_PERMISSION_RESULT_AUTH)
	  return TRUE;

	return FALSE;
}

static void
activate_existing_cb (GObject *source_object,
                      GAsyncResult *res,
                      gpointer user_data)
{
        g_autoptr(GError) error = NULL;

        if (!nm_client_activate_connection_finish (NM_CLIENT (source_object), res, &error))
		g_warning ("Failed to activate connection: (%d) %s", error->code, error->message);
}

static void
activate_new_cb (GObject *source_object,
                 GAsyncResult *res,
                 gpointer user_data)
{
        g_autoptr(GError) error = NULL;

        if (!nm_client_add_and_activate_connection_finish (NM_CLIENT (source_object), res, &error))
		g_warning ("Failed to add new connection: (%d) %s", error->code, error->message);
}

static void
wireless_dialog_response_cb (GtkDialog *foo,
                             gint response,
                             gpointer user_data)
{
	NMAWifiDialog *dialog = NMA_WIFI_DIALOG (foo);
	WirelessDialogClosure *closure = user_data;
	g_autoptr(NMConnection) connection = NULL;
	NMConnection *fuzzy_match = NULL;
	NMDevice *device;
	NMAccessPoint *ap;
	const GPtrArray *all;
	guint i;

	if (response != GTK_RESPONSE_OK)
		goto done;

	/* nma_wifi_dialog_get_connection() returns a connection with the
	 * refcount incremented, so the caller must remember to unref it.
	 */
	connection = nma_wifi_dialog_get_connection (dialog, &device, &ap);
	g_assert (connection);
	g_assert (device);

	/* Find a similar connection and use that instead */
	all = nm_client_get_connections (closure->client);
	for (i = 0; i < all->len; i++) {
		if (nm_connection_compare (connection,
		                           NM_CONNECTION (g_ptr_array_index (all, i)),
		                           (NM_SETTING_COMPARE_FLAG_FUZZY | NM_SETTING_COMPARE_FLAG_IGNORE_ID))) {
			fuzzy_match = NM_CONNECTION (g_ptr_array_index (all, i));
			break;
		}
	}

	if (fuzzy_match) {
		nm_client_activate_connection_async (closure->client,
		                                     fuzzy_match,
		                                     device,
		                                     ap ? nm_object_get_path (NM_OBJECT (ap)) : NULL,
		                                     NULL,
		                                     activate_existing_cb,
		                                     NULL);
	} else {
		NMSetting *s_con;
		NMSettingWireless *s_wifi;
		const char *mode = NULL;

		/* Entirely new connection */

		/* Don't autoconnect adhoc networks by default for now */
		s_wifi = (NMSettingWireless *) nm_connection_get_setting (connection, NM_TYPE_SETTING_WIRELESS);
		if (s_wifi)
			mode = nm_setting_wireless_get_mode (s_wifi);
		if (g_strcmp0 (mode, "adhoc") == 0) {
			s_con = nm_connection_get_setting (connection, NM_TYPE_SETTING_CONNECTION);
			if (!s_con) {
				s_con = nm_setting_connection_new ();
				nm_connection_add_setting (connection, s_con);
			}
			g_object_set (G_OBJECT (s_con), NM_SETTING_CONNECTION_AUTOCONNECT, FALSE, NULL);
		}

		nm_client_add_and_activate_connection_async (closure->client,
		                                             connection,
		                                             device,
		                                             ap ? nm_object_get_path (NM_OBJECT (ap)) : NULL,
		                                             NULL,
		                                             activate_new_cb,
		                                             NULL);
	}

done:
	gtk_window_destroy (GTK_WINDOW (dialog));
}

static void
show_wireless_dialog (GtkWidget        *toplevel,
		      NMClient         *client,
		      GtkWidget        *dialog)
{
        WirelessDialogClosure *closure;

        g_debug ("About to parent and show a network dialog");

        g_assert (GTK_IS_NATIVE (toplevel));
        g_object_set (G_OBJECT (dialog),
                      "modal", TRUE,
                      "transient-for", toplevel,
                      NULL);

        closure = g_slice_new (WirelessDialogClosure);
        closure->client = g_object_ref (client);
        g_signal_connect_data (dialog, "response",
                               G_CALLBACK (wireless_dialog_response_cb),
                               closure, wireless_dialog_closure_closure_notify, 0);

        g_object_bind_property (G_OBJECT (toplevel), "visible",
                                G_OBJECT (dialog), "visible",
                                G_BINDING_SYNC_CREATE);
}

void
cc_network_panel_create_wifi_network (GtkWidget        *toplevel,
				      NMClient         *client)
{
  if (wifi_can_create_wifi_network (client)) {
          show_wireless_dialog (toplevel, client,
                                nma_wifi_dialog_new_for_create (client));
  }
}

void
cc_network_panel_connect_to_hidden_network (GtkWidget        *toplevel,
                                            NMClient         *client)
{
        g_debug ("connect to hidden wifi");
        show_wireless_dialog (toplevel, client,
                              nma_wifi_dialog_new_for_hidden (client));
}

void
cc_network_panel_connect_to_8021x_network (GtkWidget        *toplevel,
                                           NMClient         *client,
                                           NMDevice         *device,
                                           const gchar      *arg_access_point)
{
	NMConnection *connection;
	NMSettingConnection *s_con;
	NMSettingWireless *s_wifi;
	NMSettingWirelessSecurity *s_wsec;
	NMSetting8021x *s_8021x;
	NM80211ApSecurityFlags wpa_flags, rsn_flags;
	GtkWidget *dialog;
	g_autofree gchar *uuid = NULL;
        NMAccessPoint *ap;

        g_debug ("connect to 8021x wifi");
        ap = nm_device_wifi_get_access_point_by_path (NM_DEVICE_WIFI (device), arg_access_point);
        if (ap == NULL) {
                g_warning ("didn't find access point with path %s", arg_access_point);
                return;
        }

        /* If the AP is WPA[2]-Enterprise then we need to set up a minimal 802.1x
	 * setting and ask the user for more information.
	 */
	rsn_flags = nm_access_point_get_rsn_flags (ap);
	wpa_flags = nm_access_point_get_wpa_flags (ap);
	if (!(rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)
	    && !(wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)) {
                g_warning ("Network panel loaded with connect-8021x-wifi but the "
                           "access point does not support 802.1x");
                return;
        }

        connection = nm_simple_connection_new ();

        /* Need a UUID for the "always ask" stuff in the Dialog of Doom */
        s_con = (NMSettingConnection *) nm_setting_connection_new ();
        uuid = nm_utils_uuid_generate ();
        g_object_set (s_con, NM_SETTING_CONNECTION_UUID, uuid, NULL);
        nm_connection_add_setting (connection, NM_SETTING (s_con));

        s_wifi = (NMSettingWireless *) nm_setting_wireless_new ();
        nm_connection_add_setting (connection, NM_SETTING (s_wifi));
        g_object_set (s_wifi,
                      NM_SETTING_WIRELESS_SSID, nm_access_point_get_ssid (ap),
                      NULL);

        s_wsec = (NMSettingWirelessSecurity *) nm_setting_wireless_security_new ();
        g_object_set (s_wsec, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "wpa-eap", NULL);
        nm_connection_add_setting (connection, NM_SETTING (s_wsec));

        s_8021x = (NMSetting8021x *) nm_setting_802_1x_new ();
        nm_setting_802_1x_add_eap_method (s_8021x, "ttls");
        g_object_set (s_8021x, NM_SETTING_802_1X_PHASE2_AUTH, "mschapv2", NULL);
        nm_connection_add_setting (connection, NM_SETTING (s_8021x));

        dialog = nma_wifi_dialog_new (client, connection, device, ap, FALSE);
        show_wireless_dialog (toplevel, client, dialog);
}

static void
connect_3g (NMConnection *connection,
            gboolean canceled,
            gpointer user_data)
{
        MobileDialogClosure *closure = user_data;

	if (canceled == FALSE) {
		g_return_if_fail (connection != NULL);

		/* Ask NM to add the new connection and activate it; NM will fill in the
		 * missing details based on the specific object and the device.
		 */
		nm_client_add_and_activate_connection_async (closure->client,
							     connection,
							     closure->device,
							     "/",
							     NULL,
							     activate_new_cb,
							     NULL);
	}

        mobile_dialog_closure_free (closure);
}

static void
cdma_mobile_wizard_done (NMAMobileWizard *wizard,
                         gboolean canceled,
                         NMAMobileWizardAccessMethod *method,
                         gpointer user_data)
{
	NMConnection *connection = NULL;

	if (!canceled && method) {
		NMSetting *setting;
		g_autofree gchar *uuid = NULL;
		g_autofree gchar *id = NULL;

		if (method->devtype != NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) {
			g_warning ("Unexpected device type (not CDMA).");
			canceled = TRUE;
			goto done;
		}

		connection = nm_simple_connection_new ();

		setting = nm_setting_cdma_new ();
		g_object_set (setting,
		              NM_SETTING_CDMA_NUMBER, "#777",
		              NM_SETTING_CDMA_USERNAME, method->username,
		              NM_SETTING_CDMA_PASSWORD, method->password,
		              NULL);
		nm_connection_add_setting (connection, setting);

		/* Serial setting */
		setting = nm_setting_serial_new ();
		g_object_set (setting,
		              NM_SETTING_SERIAL_BAUD, 115200,
		              NM_SETTING_SERIAL_BITS, 8,
		              NM_SETTING_SERIAL_PARITY, 'n',
		              NM_SETTING_SERIAL_STOPBITS, 1,
		              NULL);
		nm_connection_add_setting (connection, setting);

		nm_connection_add_setting (connection, nm_setting_ppp_new ());

		setting = nm_setting_connection_new ();
		if (method->plan_name)
			id = g_strdup_printf ("%s %s", method->provider_name, method->plan_name);
		else
			id = g_strdup_printf ("%s connection", method->provider_name);
		uuid = nm_utils_uuid_generate ();
		g_object_set (setting,
		              NM_SETTING_CONNECTION_ID, id,
		              NM_SETTING_CONNECTION_TYPE, NM_SETTING_CDMA_SETTING_NAME,
		              NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
		              NM_SETTING_CONNECTION_UUID, uuid,
		              NULL);
		nm_connection_add_setting (connection, setting);
	}

 done:
        connect_3g (connection, canceled, user_data);
        nma_mobile_wizard_destroy (wizard);
}

static void
gsm_mobile_wizard_done (NMAMobileWizard *wizard,
                        gboolean canceled,
                        NMAMobileWizardAccessMethod *method,
                        gpointer user_data)
{
	NMConnection *connection = NULL;

	if (!canceled && method) {
		NMSetting *setting;
		g_autofree gchar *uuid = NULL;
		g_autofree gchar *id = NULL;

		if (method->devtype != NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) {
			g_warning ("Unexpected device type (not GSM).");
			canceled = TRUE;
			goto done;
		}

		connection = nm_simple_connection_new ();

		setting = nm_setting_gsm_new ();
		g_object_set (setting,
		              NM_SETTING_GSM_NUMBER, "*99#",
		              NM_SETTING_GSM_USERNAME, method->username,
		              NM_SETTING_GSM_PASSWORD, method->password,
		              NM_SETTING_GSM_APN, method->gsm_apn,
		              NULL);
		nm_connection_add_setting (connection, setting);

		/* Serial setting */
		setting = nm_setting_serial_new ();
		g_object_set (setting,
		              NM_SETTING_SERIAL_BAUD, 115200,
		              NM_SETTING_SERIAL_BITS, 8,
		              NM_SETTING_SERIAL_PARITY, 'n',
		              NM_SETTING_SERIAL_STOPBITS, 1,
		              NULL);
		nm_connection_add_setting (connection, setting);

		nm_connection_add_setting (connection, nm_setting_ppp_new ());

		setting = nm_setting_connection_new ();
		if (method->plan_name)
			id = g_strdup_printf ("%s %s", method->provider_name, method->plan_name);
		else
			id = g_strdup_printf ("%s connection", method->provider_name);
		uuid = nm_utils_uuid_generate ();
		g_object_set (setting,
		              NM_SETTING_CONNECTION_ID, id,
		              NM_SETTING_CONNECTION_TYPE, NM_SETTING_GSM_SETTING_NAME,
		              NM_SETTING_CONNECTION_AUTOCONNECT, FALSE,
		              NM_SETTING_CONNECTION_UUID, uuid,
		              NULL);
		nm_connection_add_setting (connection, setting);
	}

done:
	connect_3g (connection, canceled, user_data);
        nma_mobile_wizard_destroy (wizard);
}

static void
toplevel_shown (GtkWindow       *toplevel,
                GParamSpec      *pspec,
                NMAMobileWizard *wizard)
{
        gboolean visible = FALSE;

        g_object_get (G_OBJECT (toplevel), "visible", &visible, NULL);
        if (visible)
                nma_mobile_wizard_present (wizard);
}

static gboolean
show_wizard_idle_cb (NMAMobileWizard *wizard)
{
        nma_mobile_wizard_present (wizard);
        return FALSE;
}

void
cc_network_panel_connect_to_3g_network (GtkWidget        *toplevel,
                                        NMClient         *client,
                                        NMDevice         *device)
{
        MobileDialogClosure *closure;
        NMAMobileWizard *wizard;
	NMDeviceModemCapabilities caps;
        gboolean visible = FALSE;

        g_debug ("connect to 3g");
        if (!NM_IS_DEVICE_MODEM (device)) {
                g_warning ("Network panel loaded with connect-3g but the selected device"
                           " is not a modem");
                return;
        }

        closure = g_slice_new (MobileDialogClosure);
        closure->client = g_object_ref (client);
        closure->device = g_object_ref (device);

	caps = nm_device_modem_get_current_capabilities (NM_DEVICE_MODEM (device));
	if (caps & NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS) {
                wizard = nma_mobile_wizard_new (GTK_WINDOW (toplevel), NULL, NM_DEVICE_MODEM_CAPABILITY_GSM_UMTS, FALSE,
                                                gsm_mobile_wizard_done, closure);
		if (wizard == NULL) {
			g_warning ("failed to construct GSM wizard");
			return;
		}
	} else if (caps & NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO) {
		wizard = nma_mobile_wizard_new (GTK_WINDOW (toplevel), NULL, NM_DEVICE_MODEM_CAPABILITY_CDMA_EVDO, FALSE,
                                                cdma_mobile_wizard_done, closure);
		if (wizard == NULL) {
			g_warning ("failed to construct CDMA wizard");
			return;
		}
        } else {
                g_warning ("Network panel loaded with connect-3g but the selected device"
                           " does not support GSM or CDMA");
                mobile_dialog_closure_free (closure);
                return;
        }

        g_object_get (G_OBJECT (toplevel), "visible", &visible, NULL);
        if (visible) {
                g_debug ("Scheduling showing the Mobile wizard");
                g_idle_add ((GSourceFunc) show_wizard_idle_cb, wizard);
        } else {
                g_debug ("Will show wizard a bit later, toplevel is not visible");
                g_signal_connect (G_OBJECT (toplevel), "notify::visible",
                                  G_CALLBACK (toplevel_shown), wizard);
        }
}
