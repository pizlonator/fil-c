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
#include <net/if_arp.h>

#include "ce-page.h"
#include "ce-page-wifi.h"
#include "ui-helpers.h"

struct _CEPageWifi
{
        AdwBin parent;

        GtkComboBoxText *bssid_combo;
        GtkComboBoxText *cloned_mac_combo;
        GtkComboBoxText *mac_combo;
        GtkEntry        *ssid_entry;

        NMClient          *client;
        NMSettingWireless *setting;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageWifi, ce_page_wifi, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

static void
connect_wifi_page (CEPageWifi *self)
{
        GBytes *ssid;
        g_autofree gchar *utf8_ssid = NULL;
        GPtrArray *bssid_array;
        gchar **bssid_list;
        const char *s_bssid_str;
        gchar **mac_list;
        const gchar *s_mac_str;
        const gchar *cloned_mac;
        gint i;

        ssid = nm_setting_wireless_get_ssid (self->setting);
        if (ssid)
                utf8_ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
        else
                utf8_ssid = g_strdup ("");
        gtk_editable_set_text (GTK_EDITABLE (self->ssid_entry), utf8_ssid);

        g_signal_connect_object (self->ssid_entry, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        bssid_array = g_ptr_array_new ();
        for (i = 0; i < nm_setting_wireless_get_num_seen_bssids (self->setting); i++) {
                g_ptr_array_add (bssid_array, g_strdup (nm_setting_wireless_get_seen_bssid (self->setting, i)));
        }
        g_ptr_array_add (bssid_array, NULL);
        bssid_list = (gchar **) g_ptr_array_free (bssid_array, FALSE);
        s_bssid_str = nm_setting_wireless_get_bssid (self->setting);
        ce_page_setup_mac_combo (self->bssid_combo, s_bssid_str, bssid_list);
        g_strfreev (bssid_list);
        g_signal_connect_object (self->bssid_combo, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        mac_list = ce_page_get_mac_list (self->client, NM_TYPE_DEVICE_WIFI,
                                         NM_DEVICE_WIFI_PERMANENT_HW_ADDRESS);
        s_mac_str = nm_setting_wireless_get_mac_address (self->setting);
        ce_page_setup_mac_combo (self->mac_combo, s_mac_str, mac_list);
        g_strfreev (mac_list);
        g_signal_connect_object (self->mac_combo, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        cloned_mac = nm_setting_wireless_get_cloned_mac_address (self->setting);
        ce_page_setup_cloned_mac_combo (self->cloned_mac_combo, cloned_mac, TRUE);
        g_signal_connect_object (self->cloned_mac_combo, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
}

static void
ui_to_setting (CEPageWifi *self)
{
        g_autoptr(GBytes) ssid = NULL;
        const gchar *utf8_ssid, *bssid;
        GtkWidget *entry;
        g_autofree gchar *device_mac = NULL;
        g_autofree gchar *cloned_mac = NULL;

        utf8_ssid = gtk_editable_get_text (GTK_EDITABLE (self->ssid_entry));
        if (!utf8_ssid || !*utf8_ssid)
                ssid = NULL;
        else {
                ssid = g_bytes_new_static (utf8_ssid, strlen (utf8_ssid));
        }
        entry = gtk_combo_box_get_child (GTK_COMBO_BOX (self->bssid_combo));
        bssid = gtk_editable_get_text (GTK_EDITABLE (entry));
        if (*bssid == '\0')
                bssid = NULL;
        entry = gtk_combo_box_get_child (GTK_COMBO_BOX (self->mac_combo));
        device_mac = ce_page_trim_address (gtk_editable_get_text (GTK_EDITABLE (entry)));
        cloned_mac = ce_page_cloned_mac_get (self->cloned_mac_combo);

        g_object_set (self->setting,
                      NM_SETTING_WIRELESS_SSID, ssid,
                      NM_SETTING_WIRELESS_BSSID, bssid,
                      NM_SETTING_WIRELESS_MAC_ADDRESS, device_mac,
                      NM_SETTING_WIRELESS_CLONED_MAC_ADDRESS, cloned_mac,
                      NULL);
}

static const gchar *
ce_page_wifi_get_title (CEPage *page)
{
        return _("Identity");
}

static gboolean
ce_page_wifi_class_validate (CEPage        *parent,
                             NMConnection  *connection,
                             GError       **error)
{
        CEPageWifi *self = (CEPageWifi *) parent;
        GtkWidget *entry;
        gboolean ret = TRUE;

        entry = gtk_combo_box_get_child (GTK_COMBO_BOX (self->bssid_combo));
        if (!ce_page_address_is_valid (gtk_editable_get_text (GTK_EDITABLE (entry)))) {
                widget_set_error (entry);
                ret = FALSE;
        } else {
                widget_unset_error (entry);
        }

        entry = gtk_combo_box_get_child (GTK_COMBO_BOX (self->mac_combo));
        if (!ce_page_address_is_valid (gtk_editable_get_text (GTK_EDITABLE (entry)))) {
                widget_set_error (entry);
                ret = FALSE;
        } else {
                widget_unset_error (entry);
        }

        if (!ce_page_cloned_mac_combo_valid (self->cloned_mac_combo)) {
                widget_set_error (gtk_combo_box_get_child (GTK_COMBO_BOX (self->cloned_mac_combo)));
                ret = FALSE;
        } else {
                widget_unset_error (gtk_combo_box_get_child (GTK_COMBO_BOX (self->cloned_mac_combo)));
        }

        if (!ret)
                return ret;

        ui_to_setting (CE_PAGE_WIFI (self));

        return ret;
}

static void
ce_page_wifi_init (CEPageWifi *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_wifi_class_init (CEPageWifiClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/wifi-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageWifi, bssid_combo);
        gtk_widget_class_bind_template_child (widget_class, CEPageWifi, cloned_mac_combo);
        gtk_widget_class_bind_template_child (widget_class, CEPageWifi, mac_combo);
        gtk_widget_class_bind_template_child (widget_class, CEPageWifi, ssid_entry);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_title = ce_page_wifi_get_title;
        iface->validate = ce_page_wifi_class_validate;
}

CEPageWifi *
ce_page_wifi_new (NMConnection     *connection,
                  NMClient         *client)
{
        CEPageWifi *self;

        self = CE_PAGE_WIFI (g_object_new (ce_page_wifi_get_type (), NULL));

        self->client = client;
        self->setting = nm_connection_get_setting_wireless (connection);

        connect_wifi_page (self);

        return self;
}
