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
#include <net/if_arp.h>
#include <NetworkManager.h>

#include "ce-page.h"
#include "ce-page-ethernet.h"
#include "ui-helpers.h"

struct _CEPageEthernet
{
        AdwBin parent;

        GtkComboBoxText *cloned_mac_combo;
        GtkComboBoxText *mac_combo;
        GtkSpinButton   *mtu_spin;
        GtkWidget       *mtu_label;
        GtkEntry        *name_entry;

        NMClient *client;
        NMSettingConnection *setting_connection;
        NMSettingWired *setting_wired;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageEthernet, ce_page_ethernet, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

static void
mtu_changed (CEPageEthernet *self)
{
        gtk_widget_set_visible (self->mtu_label,
                                gtk_spin_button_get_value_as_int (self->mtu_spin) != 0);
}

static void
mtu_output_cb (CEPageEthernet *self)
{
        gint defvalue;
        gint val;
        g_autofree gchar *buf = NULL;

        val = gtk_spin_button_get_value_as_int (self->mtu_spin);
        defvalue = ce_get_property_default (NM_SETTING (self->setting_wired), NM_SETTING_WIRED_MTU);
        if (val == defvalue)
                buf = g_strdup (_("automatic"));
        else
                buf = g_strdup_printf ("%d", val);

        if (strcmp (buf, gtk_editable_get_text (GTK_EDITABLE (self->mtu_spin))))
                gtk_editable_set_text (GTK_EDITABLE (self->mtu_spin), buf);
}

static void
connect_ethernet_page (CEPageEthernet *self)
{
        NMSettingWired *setting = self->setting_wired;
        char **mac_list;
        const char *s_mac_str;
        const gchar *name;
        const gchar *cloned_mac;

        name = nm_setting_connection_get_id (self->setting_connection);
        gtk_editable_set_text (GTK_EDITABLE (self->name_entry), name);

        /* Device MAC address */
        mac_list = ce_page_get_mac_list (self->client, NM_TYPE_DEVICE_ETHERNET,
                                         NM_DEVICE_ETHERNET_PERMANENT_HW_ADDRESS);
        s_mac_str = nm_setting_wired_get_mac_address (setting);
        ce_page_setup_mac_combo (self->mac_combo, s_mac_str, mac_list);
        g_strfreev (mac_list);
        g_signal_connect_object (self->mac_combo, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        /* Cloned MAC address */
        cloned_mac = nm_setting_wired_get_cloned_mac_address (setting);
        ce_page_setup_cloned_mac_combo (self->cloned_mac_combo, cloned_mac, FALSE);
        g_signal_connect_object (self->cloned_mac_combo, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        /* MTU */
        g_signal_connect_object (self->mtu_spin, "output", G_CALLBACK (mtu_output_cb), self, G_CONNECT_SWAPPED);
        gtk_spin_button_set_value (self->mtu_spin, (gdouble) nm_setting_wired_get_mtu (setting));
        g_signal_connect_object (self->mtu_spin, "value-changed", G_CALLBACK (mtu_changed), self, G_CONNECT_SWAPPED);
        mtu_changed (self);

        g_signal_connect_object (self->name_entry, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->mtu_spin, "value-changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
}

static void
ui_to_setting (CEPageEthernet *self)
{
        g_autofree gchar *device_mac = NULL;
        g_autofree gchar *cloned_mac = NULL;
        const gchar *text;
        GtkWidget *entry;

        entry = gtk_combo_box_get_child (GTK_COMBO_BOX (self->mac_combo));
        if (entry) {
                text = gtk_editable_get_text (GTK_EDITABLE (entry));
                device_mac = ce_page_trim_address (text);
        }

        cloned_mac = ce_page_cloned_mac_get (self->cloned_mac_combo);

        g_object_set (self->setting_wired,
                      NM_SETTING_WIRED_MAC_ADDRESS, device_mac,
                      NM_SETTING_WIRED_CLONED_MAC_ADDRESS, cloned_mac,
                      NM_SETTING_WIRED_MTU, (guint32) gtk_spin_button_get_value_as_int (self->mtu_spin),
                      NULL);

        g_object_set (self->setting_connection,
                      NM_SETTING_CONNECTION_ID, gtk_editable_get_text (GTK_EDITABLE (self->name_entry)),
                      NULL);
}

static const gchar *
ce_page_ethernet_get_title (CEPage *page)
{
        return _("Identity");
}

static gboolean
ce_page_ethernet_validate (CEPage        *page,
                           NMConnection  *connection,
                           GError       **error)
{
        CEPageEthernet *self = CE_PAGE_ETHERNET (page);
        GtkWidget *entry;
        gboolean ret = TRUE;

        entry = gtk_combo_box_get_child (GTK_COMBO_BOX (self->mac_combo));
        if (entry) {
                if (!ce_page_address_is_valid (gtk_editable_get_text (GTK_EDITABLE (entry)))) {
                        widget_set_error (entry);
                        ret = FALSE;
                } else {
                        widget_unset_error (entry);
                }
        }

        if (!ce_page_cloned_mac_combo_valid (self->cloned_mac_combo)) {
                widget_set_error (gtk_combo_box_get_child (GTK_COMBO_BOX (self->cloned_mac_combo)));
                ret = FALSE;
        } else {
                widget_unset_error (gtk_combo_box_get_child (GTK_COMBO_BOX (self->cloned_mac_combo)));
        }

        if (!ret)
                return ret;

        ui_to_setting (self);

        return nm_setting_verify (NM_SETTING (self->setting_connection), NULL, error) &&
               nm_setting_verify (NM_SETTING (self->setting_wired), NULL, error);
}

static void
ce_page_ethernet_init (CEPageEthernet *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_ethernet_class_init (CEPageEthernetClass *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/ethernet-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageEthernet, cloned_mac_combo);
        gtk_widget_class_bind_template_child (widget_class, CEPageEthernet, mac_combo);
        gtk_widget_class_bind_template_child (widget_class, CEPageEthernet, mtu_spin);
        gtk_widget_class_bind_template_child (widget_class, CEPageEthernet, mtu_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageEthernet, name_entry);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_title = ce_page_ethernet_get_title;
        iface->validate = ce_page_ethernet_validate;
}

CEPageEthernet *
ce_page_ethernet_new (NMConnection     *connection,
                      NMClient         *client)
{
        CEPageEthernet *self;

        self = CE_PAGE_ETHERNET (g_object_new (ce_page_ethernet_get_type (), NULL));

        self->client = client;
        self->setting_connection = nm_connection_get_setting_connection (connection);
        self->setting_wired = nm_connection_get_setting_wired (connection);

        connect_ethernet_page (self);

        return self;
}
