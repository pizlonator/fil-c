/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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

#include <NetworkManager.h>

#include "ce-page.h"
#include "ce-page-details.h"

#include "../panel-common.h"

struct _CEPageDetails
{
        AdwBin parent;

        GtkCheckButton *all_user_check;
        GtkCheckButton *auto_connect_check;
        GtkLabel *dns4_heading_label;
        GtkLabel *dns4_label;
        GtkLabel *dns6_heading_label;
        GtkLabel *dns6_label;
        GtkButton *forget_button;
        GtkLabel *freq_heading_label;
        GtkLabel *freq_label;
        GtkLabel *ipv4_heading_label;
        GtkLabel *ipv4_label;
        GtkLabel *ipv6_heading_label;
        GtkLabel *ipv6_label;
        GtkLabel *last_used_heading_label;
        GtkLabel *last_used_label;
        GtkLabel *mac_heading_label;
        GtkLabel *mac_label;
        GtkCheckButton *restrict_data_check;
        GtkLabel *route_heading_label;
        GtkLabel *route_label;
        GtkLabel *security_heading_label;
        GtkLabel *security_label;
        GtkLabel *speed_heading_label;
        GtkLabel *speed_label;
        GtkLabel *strength_heading_label;
        GtkLabel *strength_label;

        NMConnection *connection;
        NMDevice *device;
        NMAccessPoint *ap;
        NetConnectionEditor *editor;
        gboolean is_new_connection;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageDetails, ce_page_details, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

static void
on_forget_cb (CEPageDetails *self,
              gchar         *response)
{
        if (g_strcmp0 (response, "forget") == 0)
                net_connection_editor_forget (self->editor);
}

static void
forget_cb (CEPageDetails *self)
{
        GtkWidget *dialog;
        GtkNative *native;
        g_autofree gchar *message = NULL;

        native = gtk_widget_get_native (GTK_WIDGET (self));
        /* Translators: "%s" is the user visible name of the network */
        message = g_strdup_printf (_("Saved details for “%s” will be permanently lost. This includes passwords and any network changes."),
                                   nm_connection_get_id (self->connection));
        dialog = adw_message_dialog_new (GTK_WINDOW (native),
                                        _("Forget Connection?"),
                                        message);

        adw_message_dialog_add_responses (ADW_MESSAGE_DIALOG (dialog),
                                          "cancel",  _("_Cancel"),
                                          "forget", _("Forget"),
                                          NULL);
        adw_message_dialog_set_response_appearance (ADW_MESSAGE_DIALOG (dialog),
                                                    "forget",
                                                    ADW_RESPONSE_DESTRUCTIVE);
        adw_message_dialog_set_default_response (ADW_MESSAGE_DIALOG (dialog), "cancel");
        adw_message_dialog_set_close_response (ADW_MESSAGE_DIALOG (dialog), "cancel");

        g_signal_connect_swapped (dialog, "response", G_CALLBACK (on_forget_cb), self);
        gtk_window_present (GTK_WINDOW (dialog));
}

static gchar *
get_ap_security_string (NMAccessPoint *ap)
{
        NM80211ApSecurityFlags wpa_flags, rsn_flags;
        NM80211ApFlags flags;
        GString *str;

        flags = nm_access_point_get_flags (ap);
        wpa_flags = nm_access_point_get_wpa_flags (ap);
        rsn_flags = nm_access_point_get_rsn_flags (ap);

        str = g_string_new ("");
        if ((flags & NM_802_11_AP_FLAGS_PRIVACY) &&
            (wpa_flags == NM_802_11_AP_SEC_NONE) &&
            (rsn_flags == NM_802_11_AP_SEC_NONE)) {
                /* TRANSLATORS: this WEP WiFi security */
                g_string_append_printf (str, "%s, ", _("WEP"));
        }
        if (wpa_flags != NM_802_11_AP_SEC_NONE) {
                /* TRANSLATORS: this WPA WiFi security */
                g_string_append_printf (str, "%s, ", _("WPA"));
        }
        if (rsn_flags != NM_802_11_AP_SEC_NONE) {
#if NM_CHECK_VERSION(1,20,6)
                if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_SAE) {
                        /* TRANSLATORS: this WPA3 WiFi security */
                        g_string_append_printf (str, "%s, ", _("WPA3"));
                }
#if NM_CHECK_VERSION(1,24,0)
		else if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_OWE) {
			/* TRANSLATORS: this Enhanced Open WiFi security */
                        g_string_append_printf (str, "%s, ", _("Enhanced Open"));
		}
#endif
#if NM_CHECK_VERSION(1,26,0)
		else if (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_OWE_TM) {
                        /* Connected to open OWE-TM network. */
		}
#endif
		else
#endif
		{
                        /* TRANSLATORS: this WPA WiFi security */
                        g_string_append_printf (str, "%s, ", _("WPA2"));
                }
        }
        if ((wpa_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X) ||
            (rsn_flags & NM_802_11_AP_SEC_KEY_MGMT_802_1X)) {
                /* TRANSLATORS: this Enterprise WiFi security */
                g_string_append_printf (str, "%s, ", _("Enterprise"));
        }
        if (str->len > 0)
                g_string_set_size (str, str->len - 2);
        else {
                g_string_append (str, C_("Wifi security", "None"));
        }
        return g_string_free (str, FALSE);
}

static void
update_last_used (CEPageDetails *self, NMConnection *connection)
{
        g_autofree gchar *last_used = NULL;
        g_autoptr(GDateTime) now = NULL;
        g_autoptr(GDateTime) then = NULL;
        gint days;
        GTimeSpan diff;
        guint64 timestamp;
        NMSettingConnection *s_con;

        s_con = nm_connection_get_setting_connection (connection);
        if (s_con == NULL)
                goto out;
        timestamp = nm_setting_connection_get_timestamp (s_con);
        if (timestamp == 0) {
                last_used = g_strdup (_("Never"));
                goto out;
        }

        /* calculate the amount of time that has elapsed */
        now = g_date_time_new_now_utc ();
        then = g_date_time_new_from_unix_utc (timestamp);

        diff = g_date_time_difference  (now, then);
        days = diff / G_TIME_SPAN_DAY;
        if (days == 0)
                last_used = g_strdup (_("Today"));
        else if (days == 1)
                last_used = g_strdup (_("Yesterday"));
        else
                last_used = g_strdup_printf (ngettext ("%i day ago", "%i days ago", days), days);
out:
        gtk_label_set_label (self->last_used_label, last_used);
        gtk_widget_set_visible (GTK_WIDGET (self->last_used_heading_label), last_used != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->last_used_label), last_used != NULL);
}

static void
all_user_changed (CEPageDetails *self)
{
        gboolean all_users;
        NMSettingConnection *sc;

        sc = nm_connection_get_setting_connection (self->connection);
        all_users = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->all_user_check));

        g_object_set (sc, "permissions", NULL, NULL);
        if (!all_users)
                nm_setting_connection_add_permission (sc, "user", g_get_user_name (), NULL);
}

static void
restrict_data_changed (CEPageDetails *self)
{
        NMSettingConnection *s_con;
        NMMetered metered;

        s_con = nm_connection_get_setting_connection (self->connection);

        if (gtk_check_button_get_active (GTK_CHECK_BUTTON (self->restrict_data_check)))
                metered = NM_METERED_YES;
        else
                metered = NM_METERED_NO;

        g_object_set (s_con, "metered", metered, NULL);
}

static void
update_restrict_data (CEPageDetails *self)
{
        NMSettingConnection *s_con;
        NMMetered metered;
        const gchar *type;

        s_con = nm_connection_get_setting_connection (self->connection);

        if (s_con == NULL)
                return;

        /* Disable for VPN; NetworkManager does not implement that yet (see
         * bug https://bugzilla.gnome.org/show_bug.cgi?id=792618) */
        type = nm_setting_connection_get_connection_type (s_con);
        if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME) ||
            g_str_equal (type, NM_SETTING_WIREGUARD_SETTING_NAME)) {
                gtk_widget_set_visible(GTK_WIDGET (self->restrict_data_check), FALSE);
                return;
        }

        metered = nm_setting_connection_get_metered (s_con);

        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->restrict_data_check),
                                     metered == NM_METERED_YES || metered == NM_METERED_GUESS_YES);

        g_signal_connect_object (self->restrict_data_check, "notify::active", G_CALLBACK (restrict_data_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->restrict_data_check, "notify::active", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
}

static void
connect_details_page (CEPageDetails *self)
{
        NMSettingConnection *sc;
        guint speed;
        NMDeviceWifiCapabilities wifi_caps;
        guint frequency;
        guint strength;
        NMDeviceState state;
        NMAccessPoint *active_ap;
        g_autofree gchar *speed_label = NULL;
        const gchar *type;
        const gchar *hw_address = NULL;
        g_autofree gchar *security_string = NULL;
        g_autofree gchar *freq_string = NULL;
        const gchar *strength_label;
        gboolean device_is_active;
        NMIPConfig *ipv4_config = NULL, *ipv6_config = NULL;
        gboolean have_ipv4_address = FALSE, have_ipv6_address = FALSE;
        gboolean have_dns4 = FALSE, have_dns6 = FALSE;
        const gchar *route4_text = NULL, *route6_text = NULL;

        sc = nm_connection_get_setting_connection (self->connection);
        type = nm_setting_connection_get_connection_type (sc);

        if (NM_IS_DEVICE_WIFI (self->device))
                active_ap = nm_device_wifi_get_active_access_point (NM_DEVICE_WIFI (self->device));
        else
                active_ap = NULL;
        frequency = active_ap ? nm_access_point_get_frequency (active_ap) : 0;

        state = self->device ? nm_device_get_state (self->device) : NM_DEVICE_STATE_DISCONNECTED;

        device_is_active = FALSE;
        speed = 0;
        wifi_caps = 0;
        if (active_ap && self->ap == active_ap && state != NM_DEVICE_STATE_UNAVAILABLE) {
                device_is_active = TRUE;
                if (NM_IS_DEVICE_WIFI (self->device)) {
                        speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (self->device)) / 1000;
                        wifi_caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (self->device));
                }
        } else if (self->device) {
                NMActiveConnection *ac;
                const gchar *p1, *p2;

                ac = nm_device_get_active_connection (self->device);
                p1 = ac ? nm_active_connection_get_uuid (ac) : NULL;
                p2 = nm_connection_get_uuid (self->connection);
                if (g_strcmp0 (p1, p2) == 0) {
                        device_is_active = TRUE;
                        if (NM_IS_DEVICE_WIFI (self->device)) {
                                speed = nm_device_wifi_get_bitrate (NM_DEVICE_WIFI (self->device)) / 1000;
                                wifi_caps = nm_device_wifi_get_capabilities (NM_DEVICE_WIFI (self->device));
                        } else if (NM_IS_DEVICE_ETHERNET (self->device))
                                speed = nm_device_ethernet_get_speed (NM_DEVICE_ETHERNET (self->device));
                }
        }

        if (speed > 0 && frequency > 0)
                speed_label = g_strdup_printf (_("%d Mb/s (%1.1f GHz)"), speed, (float) (frequency) / 1000.0);
        else if (speed > 0)
                speed_label = g_strdup_printf (_("%d Mb/s"), speed);
        gtk_label_set_label (self->speed_label, speed_label);
        gtk_widget_set_visible (GTK_WIDGET (self->speed_heading_label), speed_label != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->speed_label), speed_label != NULL);

        if (self->device)
            hw_address = nm_device_get_hw_address (self->device);

        gtk_label_set_label (self->mac_label, hw_address);
        gtk_widget_set_visible (GTK_WIDGET (self->mac_heading_label), hw_address != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->mac_label), hw_address != NULL);

        if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_VALID) {
                if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_2GHZ &&
                    wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_5GHZ)
                        freq_string = g_strdup (_("2.4 GHz / 5 GHz"));
                else if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_2GHZ)
                        freq_string = g_strdup (_("2.4 GHz"));
                else if (wifi_caps & NM_WIFI_DEVICE_CAP_FREQ_5GHZ)
                        freq_string = g_strdup (_("5 GHz"));
        }

        gtk_label_set_label (self->freq_label, freq_string);
        gtk_widget_set_visible (GTK_WIDGET (self->freq_heading_label), freq_string != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->freq_label), freq_string != NULL);

        if (device_is_active && active_ap)
                security_string = get_ap_security_string (active_ap);
        gtk_label_set_label (self->security_label, security_string);
        gtk_widget_set_visible (GTK_WIDGET (self->security_heading_label), security_string != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->security_label), security_string != NULL);

        strength = 0;
        if (self->ap != NULL)
                strength = nm_access_point_get_strength (self->ap);

        if (strength <= 0)
                strength_label = NULL;
        else if (strength < 20)
                strength_label = C_("Signal strength", "None");
        else if (strength < 40)
                strength_label = C_("Signal strength", "Weak");
        else if (strength < 50)
                strength_label = C_("Signal strength", "Ok");
        else if (strength < 80)
                strength_label = C_("Signal strength", "Good");
        else
                strength_label = C_("Signal strength", "Excellent");
        gtk_label_set_label (self->strength_label, strength_label);
        gtk_widget_set_visible (GTK_WIDGET (self->strength_heading_label), strength_label != NULL);
        gtk_widget_set_visible (GTK_WIDGET (self->strength_label), strength_label != NULL);

        if (device_is_active && self->device != NULL) {
                ipv4_config = nm_device_get_ip4_config (self->device);
                ipv6_config = nm_device_get_ip6_config (self->device);
        }

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

                route6_text = nm_ip_config_get_gateway (ipv6_config);
        } else {
                gtk_widget_set_visible (GTK_WIDGET (self->ipv6_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->ipv6_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->dns6_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->dns6_label), FALSE);
        }

        if (have_ipv4_address && have_ipv6_address) {
                gtk_label_set_label (self->ipv4_heading_label, _("IPv4 Address"));
                gtk_label_set_label (self->ipv6_heading_label, _("IPv6 Address"));
        } else {
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

        if (!device_is_active && self->connection && !self->is_new_connection)
                update_last_used (self, self->connection);
        else {
                gtk_widget_set_visible (GTK_WIDGET (self->last_used_heading_label), FALSE);
                gtk_widget_set_visible (GTK_WIDGET (self->last_used_label), FALSE);
        }

        /* Auto connect check */
        if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME) ||
            g_str_equal(type, NM_SETTING_WIREGUARD_SETTING_NAME)) {
                gtk_widget_set_visible (GTK_WIDGET (self->auto_connect_check), FALSE);
        } else {
                g_object_bind_property (sc, "autoconnect",
                                        self->auto_connect_check, "active",
                                        G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
                g_signal_connect_object (self->auto_connect_check, "toggled", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        }

        /* All users check */
        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->all_user_check),
                                     nm_setting_connection_get_num_permissions (sc) == 0);
        g_signal_connect_object (self->all_user_check, "toggled", G_CALLBACK (all_user_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->all_user_check, "toggled", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        /* Restrict Data check */
        update_restrict_data (self);

        /* Forget button */
        if (!self->is_new_connection) {
                g_signal_connect_object (self->forget_button, "clicked", G_CALLBACK (forget_cb), self, G_CONNECT_SWAPPED);
                
                if (g_str_equal (type, NM_SETTING_WIRELESS_SETTING_NAME))
                        gtk_button_set_label (self->forget_button, _("Forget Connection…"));
                else if (g_str_equal (type, NM_SETTING_WIRED_SETTING_NAME))
                        gtk_button_set_label (self->forget_button, _("Remove Connection Profile…"));
                else if (g_str_equal (type, NM_SETTING_BLUETOOTH_SETTING_NAME))
                        gtk_button_set_label (self->forget_button, _("Remove Connection…"));
                else if (g_str_equal (type, NM_SETTING_VPN_SETTING_NAME) ||
                        g_str_equal (type, NM_SETTING_WIREGUARD_SETTING_NAME))
                        gtk_button_set_label (self->forget_button, _("Remove VPN…"));
                else
                        gtk_widget_set_visible (GTK_WIDGET (self->forget_button), FALSE);
        } else {
                gtk_widget_set_visible (GTK_WIDGET (self->forget_button), FALSE);
        }
}

static void
ce_page_details_dispose (GObject *object)
{
        CEPageDetails *self = CE_PAGE_DETAILS (object);

        g_clear_object (&self->connection);

        G_OBJECT_CLASS (ce_page_details_parent_class)->dispose (object);
}

static const gchar *
ce_page_details_get_title (CEPage *page)
{
        return _("Details");
}

static void
ce_page_details_init (CEPageDetails *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_details_class_init (CEPageDetailsClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = ce_page_details_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/details-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, all_user_check);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, auto_connect_check);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, dns4_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, dns4_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, dns6_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, dns6_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, forget_button);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, freq_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, freq_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, ipv4_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, ipv4_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, ipv6_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, ipv6_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, last_used_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, last_used_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, mac_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, mac_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, restrict_data_check);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, route_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, route_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, security_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, security_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, speed_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, speed_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, strength_heading_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageDetails, strength_label);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_title = ce_page_details_get_title;
}

CEPageDetails *
ce_page_details_new (NMConnection        *connection,
                     NMDevice            *device,
                     NMAccessPoint       *ap,
                     NetConnectionEditor *editor,
                     gboolean            is_new_connection)
{
        CEPageDetails *self;

        self = CE_PAGE_DETAILS (g_object_new (ce_page_details_get_type (), NULL));

        self->connection = g_object_ref (connection);
        self->editor = editor;
        self->device = device;
        self->ap = ap;
        self->is_new_connection = is_new_connection;

        connect_details_page (self);

        return self;
}
