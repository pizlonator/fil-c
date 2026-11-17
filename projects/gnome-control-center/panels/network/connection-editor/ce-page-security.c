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
#include "ce-page-security.h"
#include "nma-ws.h"

struct _CEPageSecurity
{
        AdwBin parent;

        GtkBox      *box;
        GtkComboBox *security_combo;
        GtkLabel    *security_label;

        NMConnection *connection;
        const gchar  *security_setting;
        GtkSizeGroup *group;
        gboolean     adhoc;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageSecurity, ce_page_security, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

enum {
        S_NAME_COLUMN,
        S_SEC_COLUMN,
        S_ADHOC_VALID_COLUMN
};

static gboolean
find_proto (NMSettingWirelessSecurity *sec, const char *item)
{
        guint32 i;

        for (i = 0; i < nm_setting_wireless_security_get_num_protos (sec); i++) {
                if (!strcmp (item, nm_setting_wireless_security_get_proto (sec, i)))
                        return TRUE;
        }
        return FALSE;
}

static NMUtilsSecurityType
get_default_type_for_security (NMSettingWirelessSecurity *sec)
{
        const char *key_mgmt, *auth_alg;

        g_return_val_if_fail (sec != NULL, NMU_SEC_NONE);

        key_mgmt = nm_setting_wireless_security_get_key_mgmt (sec);
        auth_alg = nm_setting_wireless_security_get_auth_alg (sec);

        /* No IEEE 802.1x */
        if (!strcmp (key_mgmt, "none"))
                return NMU_SEC_STATIC_WEP;

        if (!strcmp (key_mgmt, "ieee8021x")) {
                if (auth_alg && !strcmp (auth_alg, "leap"))
                        return NMU_SEC_LEAP;
                return NMU_SEC_DYNAMIC_WEP;
        }

#if NM_CHECK_VERSION(1,24,0)
        if (!strcmp (key_mgmt, "owe")) {
                return NMU_SEC_OWE;
        }
#endif

#if NM_CHECK_VERSION(1,20,6)
        if (!strcmp (key_mgmt, "sae")) {
                return NMU_SEC_SAE;
        }
#endif

        if (   !strcmp (key_mgmt, "wpa-psk")) {
                if (find_proto (sec, "rsn"))
                        return NMU_SEC_WPA2_PSK;
                else if (find_proto (sec, "wpa"))
                        return NMU_SEC_WPA_PSK;
                else
                        return NMU_SEC_WPA_PSK;
        }

        if (!strcmp (key_mgmt, "wpa-eap")) {
                if (find_proto (sec, "rsn"))
                        return NMU_SEC_WPA2_ENTERPRISE;
                else if (find_proto (sec, "wpa"))
                        return NMU_SEC_WPA_ENTERPRISE;
                else
                        return NMU_SEC_WPA_ENTERPRISE;
        }

        return NMU_SEC_INVALID;
}

static NMAWs *
security_combo_get_active (CEPageSecurity *self)
{
        GtkTreeIter iter;
        GtkTreeModel *model;
        NMAWs *sec;

        model = gtk_combo_box_get_model (self->security_combo);
        if (!gtk_combo_box_get_active_iter (self->security_combo, &iter))
                return NULL;
        gtk_tree_model_get (model, &iter, S_SEC_COLUMN, &sec, -1);

        return sec;
}

static void
wsec_size_group_clear (GtkSizeGroup *group)
{
        GSList *children;
        GSList *iter;

        g_return_if_fail (group != NULL);

        children = gtk_size_group_get_widgets (group);
        for (iter = children; iter; iter = g_slist_next (iter))
                gtk_size_group_remove_widget (group, GTK_WIDGET (iter->data));
}

static void
security_combo_changed (CEPageSecurity *self)
{
        NMAWs *sec;
        GtkWidget *child;

        wsec_size_group_clear (self->group);

        while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->box))) != NULL)
                gtk_box_remove (self->box, child);

        sec = security_combo_get_active (self);
        if (sec) {
                if (gtk_widget_get_parent (GTK_WIDGET (sec)))
                        gtk_box_remove (self->box, GTK_WIDGET (sec));

                gtk_size_group_add_widget (self->group, GTK_WIDGET (self->security_label));
                nma_ws_add_to_size_group (sec, self->group);

                gtk_box_append (self->box, g_object_ref (GTK_WIDGET (sec)));
                g_object_unref (sec);
        }

        ce_page_changed (CE_PAGE (self));
}

static void
security_item_changed_cb (CEPageSecurity *self)
{
        ce_page_changed (CE_PAGE (self));
}

static void
add_security_item (CEPageSecurity   *self,
                   NMAWs            *sec,
                   GtkListStore     *model,
                   GtkTreeIter      *iter,
                   const char       *text,
                   gboolean          adhoc_valid)
{
        if (G_IS_INITIALLY_UNOWNED (sec))
                g_object_ref_sink (sec);
        g_signal_connect_object (sec, "ws-changed", G_CALLBACK (security_item_changed_cb), self, G_CONNECT_SWAPPED);
        gtk_list_store_append (model, iter);
        gtk_list_store_set (model, iter,
                            S_NAME_COLUMN, text,
                            S_SEC_COLUMN, sec,
                            S_ADHOC_VALID_COLUMN, adhoc_valid,
                            -1);
        g_object_unref (sec);
}

static void
set_sensitive (GtkCellLayout *cell_layout,
               GtkCellRenderer *cell,
               GtkTreeModel *tree_model,
               GtkTreeIter *iter,
               gpointer data)
{
        gboolean *adhoc = data;
        gboolean sensitive = TRUE, adhoc_valid = TRUE;

        gtk_tree_model_get (tree_model, iter, S_ADHOC_VALID_COLUMN, &adhoc_valid, -1);
        if (*adhoc && !adhoc_valid)
                sensitive = FALSE;

        g_object_set (cell, "sensitive", sensitive, NULL);
}

static void
finish_setup (CEPageSecurity *self)
{
        NMSettingWireless *sw;
        NMSettingWirelessSecurity *sws;
        gboolean is_adhoc = FALSE;
        g_autoptr(GtkListStore) sec_model = NULL;
        GtkTreeIter iter;
        const gchar *mode;
        guint32 dev_caps = 0;
        NMUtilsSecurityType default_type = NMU_SEC_NONE;
        int active = -1;
        int item = 0;
        GtkCellRenderer *renderer;

        sw = nm_connection_get_setting_wireless (self->connection);
        g_assert (sw);

        self->group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

        dev_caps =   NM_WIFI_DEVICE_CAP_CIPHER_WEP40
                   | NM_WIFI_DEVICE_CAP_CIPHER_WEP104
                   | NM_WIFI_DEVICE_CAP_CIPHER_TKIP
                   | NM_WIFI_DEVICE_CAP_CIPHER_CCMP
                   | NM_WIFI_DEVICE_CAP_WPA
                   | NM_WIFI_DEVICE_CAP_RSN;

        mode = nm_setting_wireless_get_mode (sw);
        if (mode && !strcmp (mode, "adhoc"))
                is_adhoc = TRUE;
        self->adhoc = is_adhoc;

        sws = nm_connection_get_setting_wireless_security (self->connection);
        if (sws)
                default_type = get_default_type_for_security (sws);

        sec_model = gtk_list_store_new (3, G_TYPE_STRING, NMA_TYPE_WS, G_TYPE_BOOLEAN);

        if (nm_utils_security_valid (NMU_SEC_NONE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                gtk_list_store_insert_with_values (sec_model, &iter, -1,
                                                   S_NAME_COLUMN, C_("Wi-Fi/Ethernet security", "None"),
                                                   S_ADHOC_VALID_COLUMN, TRUE,
                                                   -1);
                if (default_type == NMU_SEC_NONE)
                        active = item;
                item++;
        }

#if NM_CHECK_VERSION(1,24,0)
        if (nm_utils_security_valid (NMU_SEC_OWE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
		gtk_list_store_insert_with_values (sec_model, &iter, -1,
                                                   S_NAME_COLUMN, _("Enhanced Open"),
                                                   S_ADHOC_VALID_COLUMN, FALSE,
                                                   -1);
		if (active < 0 && default_type == NMU_SEC_OWE)
			active = item;
		item++;
        }
#endif

        if (nm_utils_security_valid (NMU_SEC_STATIC_WEP, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                NMAWsWepKey *ws_wep;
                NMWepKeyType wep_type = NM_WEP_KEY_TYPE_KEY;

                if (default_type == NMU_SEC_STATIC_WEP) {
                        sws = nm_connection_get_setting_wireless_security (self->connection);
                        if (sws)
                                wep_type = nm_setting_wireless_security_get_wep_key_type (sws);
                        if (wep_type == NM_WEP_KEY_TYPE_UNKNOWN)
                                wep_type = NM_WEP_KEY_TYPE_KEY;
                }

                ws_wep = nma_ws_wep_key_new (self->connection, NM_WEP_KEY_TYPE_KEY, FALSE, FALSE);
                if (ws_wep) {
                        add_security_item (self, NMA_WS (ws_wep), sec_model,
                                           &iter, _("WEP 40/128-bit Key (Hex or ASCII)"),
                                           TRUE);
                        if ((active < 0) && (default_type == NMU_SEC_STATIC_WEP) && (wep_type == NM_WEP_KEY_TYPE_KEY))
                                active = item;
                        item++;
                }

                ws_wep = nma_ws_wep_key_new (self->connection, NM_WEP_KEY_TYPE_PASSPHRASE, FALSE, FALSE);
                if (ws_wep) {
                        add_security_item (self, NMA_WS (ws_wep), sec_model,
                                           &iter, _("WEP 128-bit Passphrase"), TRUE);
                        if ((active < 0) && (default_type == NMU_SEC_STATIC_WEP) && (wep_type == NM_WEP_KEY_TYPE_PASSPHRASE))
                                active = item;
                        item++;
                }
        }

        if (nm_utils_security_valid (NMU_SEC_LEAP, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                NMAWsLeap *ws_leap;

                ws_leap = nma_ws_leap_new (self->connection, FALSE);
                if (ws_leap) {
                        add_security_item (self, NMA_WS (ws_leap), sec_model,
                                           &iter, _("LEAP"), FALSE);
                        if ((active < 0) && (default_type == NMU_SEC_LEAP))
                                active = item;
                        item++;
                }
        }

        if (nm_utils_security_valid (NMU_SEC_DYNAMIC_WEP, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                NMAWsDynamicWep *ws_dynamic_wep;

                ws_dynamic_wep = nma_ws_dynamic_wep_new (self->connection, TRUE, FALSE);
                if (ws_dynamic_wep) {
                        add_security_item (self, NMA_WS (ws_dynamic_wep), sec_model,
                                           &iter, _("Dynamic WEP (802.1x)"), FALSE);
                        if ((active < 0) && (default_type == NMU_SEC_DYNAMIC_WEP))
                                active = item;
                        item++;
                }
        }

        if (nm_utils_security_valid (NMU_SEC_WPA_PSK, dev_caps, FALSE, is_adhoc, 0, 0, 0) ||
            nm_utils_security_valid (NMU_SEC_WPA2_PSK, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                NMAWsWpaPsk *ws_wpa_psk;

                ws_wpa_psk = nma_ws_wpa_psk_new (self->connection, FALSE);
                if (ws_wpa_psk) {
                        add_security_item (self, NMA_WS (ws_wpa_psk), sec_model,
                                           &iter, _("WPA & WPA2 Personal"), FALSE);
                        if ((active < 0) && ((default_type == NMU_SEC_WPA_PSK) || (default_type == NMU_SEC_WPA2_PSK)))
                                active = item;
                        item++;
                }
        }

        if (nm_utils_security_valid (NMU_SEC_WPA_ENTERPRISE, dev_caps, FALSE, is_adhoc, 0, 0, 0) ||
            nm_utils_security_valid (NMU_SEC_WPA2_ENTERPRISE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                NMAWsWpaEap *ws_wpa_eap;

                ws_wpa_eap = nma_ws_wpa_eap_new (self->connection, TRUE, FALSE, NULL);
                if (ws_wpa_eap) {
                        add_security_item (self, NMA_WS (ws_wpa_eap), sec_model,
                                           &iter, _("WPA & WPA2 Enterprise"), FALSE);
                        if ((active < 0) && ((default_type == NMU_SEC_WPA_ENTERPRISE) || (default_type == NMU_SEC_WPA2_ENTERPRISE)))
                                active = item;
                        item++;
                }
        }

#if NM_CHECK_VERSION(1,20,6)
        if (nm_utils_security_valid (NMU_SEC_SAE, dev_caps, FALSE, is_adhoc, 0, 0, 0)) {
                NMAWsSae *ws_sae;

                ws_sae = nma_ws_sae_new (self->connection, FALSE);
                if (ws_sae) {
                        add_security_item (self, NMA_WS (ws_sae), sec_model,
                                           &iter, _("WPA3 Personal"), FALSE);
                        if ((active < 0) && ((default_type == NMU_SEC_SAE)))
                                active = item;
                        item++;
                }
        }
#endif

        gtk_combo_box_set_model (self->security_combo, GTK_TREE_MODEL (sec_model));
        gtk_cell_layout_clear (GTK_CELL_LAYOUT (self->security_combo));

        renderer = gtk_cell_renderer_text_new ();
        gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (self->security_combo), renderer, TRUE);
        gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (self->security_combo), renderer, "text", S_NAME_COLUMN, NULL);
        gtk_cell_layout_set_cell_data_func (GTK_CELL_LAYOUT (self->security_combo), renderer, set_sensitive, &self->adhoc, NULL);

        gtk_combo_box_set_active (self->security_combo, active < 0 ? 0 : (guint32) active);

        security_combo_changed (self);
        g_signal_connect_object (self->security_combo, "changed",
                                 G_CALLBACK (security_combo_changed), self, G_CONNECT_SWAPPED);
}

static void
ce_page_security_dispose (GObject *object)
{
        CEPageSecurity *self = CE_PAGE_SECURITY (object);

        g_clear_object (&self->connection);
        g_clear_object (&self->group);

        G_OBJECT_CLASS (ce_page_security_parent_class)->dispose (object);
}

static const gchar *
ce_page_security_get_security_setting (CEPage *page)
{
        return CE_PAGE_SECURITY (page)->security_setting;
}

static const gchar *
ce_page_security_get_title (CEPage *page)
{
        return _("Security");
}

static gboolean
ce_page_security_validate (CEPage        *page,
                           NMConnection  *connection,
                           GError       **error)
{
        CEPageSecurity *self = CE_PAGE_SECURITY (page);
        NMSettingWireless *sw;
        NMAWs *sec;
        gboolean valid = FALSE;
        const char *mode;

        sw = nm_connection_get_setting_wireless (connection);

        mode = nm_setting_wireless_get_mode (sw);
        if (g_strcmp0 (mode, NM_SETTING_WIRELESS_MODE_ADHOC) == 0)
                CE_PAGE_SECURITY (self)->adhoc = TRUE;
        else
                CE_PAGE_SECURITY (self)->adhoc = FALSE;

        sec = security_combo_get_active (CE_PAGE_SECURITY (self));
        if (sec) {
                GBytes *ssid = nm_setting_wireless_get_ssid (sw);

                if (ssid) {
                        /* FIXME: get failed property and error out of wifi security objects */
                        valid = nma_ws_validate (sec, error);
                        if (valid)
                                nma_ws_fill_connection (sec, connection);
                } else {
                        g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_MISSING_SETTING, "Missing SSID");
                        valid = FALSE;
                }

                if (self->adhoc) {
                        if (!nma_ws_adhoc_compatible (sec)) {
                                if (valid)
                                        g_set_error (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_INVALID_SETTING, "Security not compatible with Ad-Hoc mode");
                                valid = FALSE;
                        }
                }
                g_object_unref (sec);
        } else {

    		if (gtk_combo_box_get_active ((CE_PAGE_SECURITY (self))->security_combo) == 0) {
    			/* No security, unencrypted */
    			nm_connection_remove_setting (connection, NM_TYPE_SETTING_WIRELESS_SECURITY);
    			nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
    			valid = TRUE;
    		} else {
    			/* owe case:
    			 * fill the connection manually until libnma implements OWE wireless security
    			 */
    			NMSetting *sws;

    			sws = nm_setting_wireless_security_new ();
    			g_object_set (sws, NM_SETTING_WIRELESS_SECURITY_KEY_MGMT, "owe", NULL);
    			nm_connection_add_setting (connection, sws);
    			nm_connection_remove_setting (connection, NM_TYPE_SETTING_802_1X);
    			valid = TRUE;
    		}

        }

        return valid;
}

static void
ce_page_security_init (CEPageSecurity *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_security_class_init (CEPageSecurityClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        object_class->dispose = ce_page_security_dispose;

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/security-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageSecurity, box);
        gtk_widget_class_bind_template_child (widget_class, CEPageSecurity, security_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageSecurity, security_combo);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_security_setting = ce_page_security_get_security_setting;
        iface->get_title = ce_page_security_get_title;
        iface->validate = ce_page_security_validate;
}

CEPageSecurity *
ce_page_security_new (NMConnection *connection)
{
        CEPageSecurity *self;
        NMUtilsSecurityType default_type = NMU_SEC_NONE;
        NMSettingWirelessSecurity *sws;

        self = CE_PAGE_SECURITY (g_object_new (ce_page_security_get_type (), NULL));

        self->connection = g_object_ref (connection);

        sws = nm_connection_get_setting_wireless_security (connection);
        if (sws)
                default_type = get_default_type_for_security (sws);

        if (default_type == NMU_SEC_STATIC_WEP ||
            default_type == NMU_SEC_LEAP ||
            default_type == NMU_SEC_WPA_PSK ||
#if NM_CHECK_VERSION(1,20,6)
	    default_type == NMU_SEC_SAE ||
#endif
#if NM_CHECK_VERSION(1,24,0)
	    default_type == NMU_SEC_OWE ||
#endif
            default_type == NMU_SEC_WPA2_PSK) {
                self->security_setting = NM_SETTING_WIRELESS_SECURITY_SETTING_NAME;
        }

        if (default_type == NMU_SEC_DYNAMIC_WEP ||
            default_type == NMU_SEC_WPA_ENTERPRISE ||
            default_type == NMU_SEC_WPA2_ENTERPRISE) {
                self->security_setting = NM_SETTING_802_1X_SETTING_NAME;
        }

        g_signal_connect (self, "initialized", G_CALLBACK (finish_setup), NULL);

        return self;
}
