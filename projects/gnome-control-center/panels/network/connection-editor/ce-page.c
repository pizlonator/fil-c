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

#include <string.h>

#include <net/if_arp.h>
#include <netinet/ether.h>

#include <NetworkManager.h>

#include <glib/gi18n.h>

#include "ce-page.h"


G_DEFINE_INTERFACE (CEPage, ce_page, G_TYPE_OBJECT)

enum {
        CHANGED,
        INITIALIZED,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

gboolean
ce_page_validate (CEPage *self, NMConnection *connection, GError **error)
{
        g_return_val_if_fail (CE_IS_PAGE (self), FALSE);
        g_return_val_if_fail (NM_IS_CONNECTION (connection), FALSE);

        if (CE_PAGE_GET_IFACE (self)->validate)
                return CE_PAGE_GET_IFACE (self)->validate (self, connection, error);

        return TRUE;
}

const char *
ce_page_get_title (CEPage *self)
{
        g_return_val_if_fail (CE_IS_PAGE (self), NULL);

        return CE_PAGE_GET_IFACE (self)->get_title (self);
}

void
ce_page_changed (CEPage *self)
{
        g_return_if_fail (CE_IS_PAGE (self));

        g_signal_emit (self, signals[CHANGED], 0);
}

static void
ce_page_default_init (CEPageInterface *iface)
{
        signals[CHANGED] =
                g_signal_new ("changed",
                              G_TYPE_FROM_INTERFACE (iface),
                              G_SIGNAL_RUN_FIRST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__VOID,
                              G_TYPE_NONE, 0);

        signals[INITIALIZED] =
                g_signal_new ("initialized",
                              G_TYPE_FROM_INTERFACE (iface),
                              G_SIGNAL_RUN_FIRST,
                              0,
                              NULL, NULL,
                              g_cclosure_marshal_VOID__POINTER,
                              G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void
emit_initialized (CEPage *self,
                  GError *error)
{
        g_signal_emit (self, signals[INITIALIZED], 0, error);
        g_clear_error (&error);
}

void
ce_page_complete_init (CEPage       *self,
                       NMConnection *connection,
                       const gchar  *setting_name,
                       GVariant     *secrets,
                       GError       *error)
{
	g_autoptr(GError) update_error = NULL;
	g_autoptr(GVariant) setting_dict = NULL;
	gboolean ignore_error = FALSE;

	g_return_if_fail (self != NULL);
	g_return_if_fail (CE_IS_PAGE (self));

	if (error) {
		ignore_error = g_error_matches (error, NM_CONNECTION_ERROR, NM_CONNECTION_ERROR_SETTING_NOT_FOUND) ||
			g_error_matches (error, NM_SECRET_AGENT_ERROR, NM_SECRET_AGENT_ERROR_NO_SECRETS);
	}

	/* Ignore missing settings errors */
	if (error && !ignore_error) {
		emit_initialized (self, error);
		return;
	} else if (!setting_name || !secrets || g_variant_n_children (secrets) == 0) {
		/* Success, no secrets */
		emit_initialized (self, NULL);
		return;
	}

	g_assert (setting_name);
	g_assert (secrets);

	setting_dict = g_variant_lookup_value (secrets, setting_name, NM_VARIANT_TYPE_SETTING);
	if (!setting_dict) {
		/* Success, no secrets */
		emit_initialized (self, NULL);
		return;
	}

	/* Update the connection with the new secrets */
	if (!nm_connection_update_secrets (connection,
                                           setting_name,
                                           secrets,
                                           &update_error))
                g_warning ("Couldn't update secrets: %s", update_error->message);

	emit_initialized (self, NULL);
}

gchar **
ce_page_get_mac_list (NMClient    *client,
                      GType        device_type,
                      const gchar *mac_property)
{
        const GPtrArray *devices;
        GPtrArray *macs;
        int i;

        macs = g_ptr_array_new ();
        devices = nm_client_get_devices (client);
        for (i = 0; devices && (i < devices->len); i++) {
                NMDevice *dev = g_ptr_array_index (devices, i);
                const char *iface;
                g_autofree gchar *mac = NULL;
                g_autofree gchar *item = NULL;

                if (!G_TYPE_CHECK_INSTANCE_TYPE (dev, device_type))
                        continue;

                g_object_get (G_OBJECT (dev), mac_property, &mac, NULL);
                iface = nm_device_get_iface (NM_DEVICE (dev));
                item = g_strdup_printf ("%s (%s)", mac, iface);
                g_ptr_array_add (macs, g_steal_pointer (&item));
        }

        g_ptr_array_add (macs, NULL);
        return (char **)g_ptr_array_free (macs, FALSE);
}

void
ce_page_setup_mac_combo (GtkComboBoxText  *combo,
                         const gchar      *current_mac,
                         gchar           **mac_list)
{
        gchar **m, *active_mac = NULL;
        gint current_mac_len;
        GtkWidget *entry;

        if (current_mac)
                current_mac_len = strlen (current_mac);
        else
                current_mac_len = -1;

        for (m= mac_list; m && *m; m++) {
                gtk_combo_box_text_append_text (combo, *m);
                if (current_mac &&
                    g_ascii_strncasecmp (*m, current_mac, current_mac_len) == 0
                    && ((*m)[current_mac_len] == '\0' || (*m)[current_mac_len] == ' '))
                        active_mac = *m;
        }

        if (current_mac) {
                if (!active_mac) {
                        gtk_combo_box_text_prepend_text (combo, current_mac);
                }

                entry = gtk_combo_box_get_child (GTK_COMBO_BOX (combo));
                if (entry)
                        gtk_editable_set_text (GTK_EDITABLE (entry), active_mac ? active_mac : current_mac);
        }
}

gchar *
ce_page_trim_address (const gchar *addr)
{
        char *space;

        if (!addr || *addr == '\0')
                return NULL;

        space = strchr (addr, ' ');
        if (space != NULL)
                return g_strndup (addr, space - addr);
        return g_strdup (addr);
}

void
ce_page_setup_cloned_mac_combo (GtkComboBoxText *combo, const char *current, gboolean is_wifi)
{
       GtkWidget *entry;
       static const char *entries_wired[][2] = { { "preserve",  N_("Preserve") },
                                                 { "permanent", N_("Permanent") },
                                                 { "random",    N_("Random") },
                                                 { "stable",    N_("Stable") } };
       static const char *entries_wifi[][2] = { { "preserve",    N_("Preserve") },
                                                { "permanent",   N_("Permanent") },
                                                { "random",      N_("Random") },
                                                { "stable",      N_("Stable") },
#if NM_CHECK_VERSION(1, 46, 0)
                                                { "stable-ssid", N_("Stable per SSID") }
#endif
                                              };
       int i, active = -1;
       const char *(*entries)[2] = is_wifi ? entries_wifi : entries_wired;
       gsize n_entries = is_wifi ? G_N_ELEMENTS (entries_wifi) : G_N_ELEMENTS (entries_wired);

       gtk_widget_set_tooltip_text (GTK_WIDGET (combo),
               _("The MAC address entered here will be used as hardware address for "
                 "the network device this connection is activated on. This feature is "
                 "known as MAC cloning or spoofing. Example: 00:11:22:33:44:55"));

       gtk_combo_box_text_remove_all (combo);

       for (i = 0; i < n_entries; i++) {
               gtk_combo_box_text_append (combo, entries[i][0], _(entries[i][1]));
               if (g_strcmp0 (current, entries[i][0]) == 0)
                       active = i;
       }

       if (active != -1) {
               gtk_combo_box_set_active (GTK_COMBO_BOX (combo), active);
       } else if (current && current[0]) {
               entry = gtk_combo_box_get_child (GTK_COMBO_BOX (combo));
               g_assert (entry);
               gtk_editable_set_text (GTK_EDITABLE (entry), current);
       }
}

char *
ce_page_cloned_mac_get (GtkComboBoxText *combo)
{
       g_autofree gchar *active_text = NULL;
       const char *id;

       id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (combo));
       if (id)
               return g_strdup (id);

       active_text = gtk_combo_box_text_get_active_text (combo);

       if (active_text[0] == '\0')
               return NULL;

       return g_steal_pointer (&active_text);
}

gboolean
ce_page_address_is_valid (const gchar *addr)
{
        guint8 invalid_addr[4][ETH_ALEN] = {
                {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
                {0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
                {0x44, 0x44, 0x44, 0x44, 0x44, 0x44},
                {0x00, 0x30, 0xb4, 0x00, 0x00, 0x00}, /* prism54 dummy MAC */
        };
        guint8 addr_bin[ETH_ALEN];
        g_autofree gchar *trimmed_addr = NULL;
        guint i;

        if (!addr || *addr == '\0')
                return TRUE;

        trimmed_addr = ce_page_trim_address (addr);

        if (!nm_utils_hwaddr_valid (trimmed_addr, -1))
                return FALSE;

        if (!nm_utils_hwaddr_aton (trimmed_addr, addr_bin, ETH_ALEN))
                return FALSE;

        /* Check for multicast address */
        if ((((guint8 *) addr_bin)[0]) & 0x01)
                return FALSE;

        for (i = 0; i < G_N_ELEMENTS (invalid_addr); i++) {
                if (nm_utils_hwaddr_matches (addr_bin, ETH_ALEN, invalid_addr[i], ETH_ALEN))
                        return FALSE;
        }

        return TRUE;
}

gboolean
ce_page_cloned_mac_combo_valid (GtkComboBoxText *combo)
{
       g_autofree gchar *active_text = NULL;

       if (gtk_combo_box_get_active (GTK_COMBO_BOX (combo)) != -1)
               return TRUE;

       active_text = gtk_combo_box_text_get_active_text (combo);

       return active_text[0] == '\0' || ce_page_address_is_valid (active_text);
}

const gchar *
ce_page_get_security_setting (CEPage *self)
{
        if (CE_PAGE_GET_IFACE (self)->get_security_setting)
                return CE_PAGE_GET_IFACE (self)->get_security_setting (self);

        return NULL;
}

gint
ce_get_property_default (NMSetting *setting, const gchar *property_name)
{
        GParamSpec *spec;
        GValue value = { 0, };

        spec = g_object_class_find_property (G_OBJECT_GET_CLASS (setting), property_name);
        g_return_val_if_fail (spec != NULL, -1);

        g_value_init (&value, spec->value_type);
        g_param_value_set_default (spec, &value);

        if (G_VALUE_HOLDS_CHAR (&value))
                return (int) g_value_get_schar (&value);
        else if (G_VALUE_HOLDS_INT (&value))
                return g_value_get_int (&value);
        else if (G_VALUE_HOLDS_INT64 (&value))
                return (int) g_value_get_int64 (&value);
        else if (G_VALUE_HOLDS_LONG (&value))
                return (int) g_value_get_long (&value);
        else if (G_VALUE_HOLDS_UINT (&value))
                return (int) g_value_get_uint (&value);
        else if (G_VALUE_HOLDS_UINT64 (&value))
                return (int) g_value_get_uint64 (&value);
        else if (G_VALUE_HOLDS_ULONG (&value))
                return (int) g_value_get_ulong (&value);
        else if (G_VALUE_HOLDS_UCHAR (&value))
                return (int) g_value_get_uchar (&value);
        g_return_val_if_fail (FALSE, 0);
        return 0;
}

gchar *
ce_page_get_next_available_name (const GPtrArray *connections,
                                 NameFormat format,
                                 const gchar *type_name)
{
        GSList *names = NULL, *l;
        gchar *cname = NULL;
        gint i = 0;
        guint con_idx;

        for (con_idx = 0; con_idx < connections->len; con_idx++) {
                NMConnection *connection = g_ptr_array_index (connections, con_idx);
                const gchar *id;

                id = nm_connection_get_id (connection);
                g_assert (id);
                names = g_slist_append (names, (gpointer) id);
        }

        /* Find the next available unique connection name */
        while (!cname && (i++ < 10000)) {
                g_autofree gchar *temp = NULL;
                gboolean found = FALSE;

                switch (format) {
                        case NAME_FORMAT_TYPE:
                                temp = g_strdup_printf ("%s %d", type_name, i);
                                break;
                        case NAME_FORMAT_PROFILE:
                                temp = g_strdup_printf (_("Profile %d"), i);
                                break;
                        default:
                                g_assert_not_reached ();
                }

                for (l = names; l; l = l->next) {
                        if (!strcmp (l->data, temp)) {
                                found = TRUE;
                                break;
                        }
                }
                if (!found)
                        cname = g_steal_pointer (&temp);
        }
        g_slist_free (names);

        return cname;
}
