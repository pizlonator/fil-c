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

#include <errno.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <glib/gi18n.h>
#include <NetworkManager.h>

#include "ce-ip-address-entry.h"
#include "ce-page.h"
#include "ce-page-ip6.h"
#include "ui-helpers.h"

static void ensure_empty_address_row (CEPageIP6 *self);
static void ensure_empty_routes_row (CEPageIP6 *self);


struct _CEPageIP6
{
        AdwBin            parent;

        GtkBox            *address_box;
        GtkLabel          *address_address_label;
        GtkLabel          *address_prefix_label;
        GtkLabel          *address_gateway_label;
        GtkSizeGroup      *address_sizegroup;
        GtkLabel          *auto_dns_label;
        GtkSwitch         *auto_dns_switch;
        GtkLabel          *auto_routes_label;
        GtkSwitch         *auto_routes_switch;
        GtkBox            *content_box;
        GtkCheckButton    *disabled_radio;
        GtkBox            *dns_box;
        GtkEntry          *dns_entry;
        GtkGrid           *main_box;
        GtkCheckButton    *never_default_check;
        GtkBox            *routes_box;
        GtkBox            *route_config_box;
        GtkLabel          *routes_address_label;
        GtkLabel          *routes_prefix_label;
        GtkLabel          *routes_gateway_label;
        GtkLabel          *routes_metric_label;
        GtkSizeGroup      *routes_address_sizegroup;
        GtkSizeGroup      *routes_prefix_sizegroup;
        GtkSizeGroup      *routes_gateway_sizegroup;
        GtkSizeGroup      *routes_metric_sizegroup;
        GtkSizeGroup      *routes_sizegroup;
        GtkCheckButton    *shared_radio;

        NMSettingIPConfig *setting;

        GtkWidget       *address_list;
        GtkWidget       *routes_list;

        GActionGroup      *method_group;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageIP6, ce_page_ip6, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

enum {
        METHOD_COL_NAME,
        METHOD_COL_METHOD
};

static void
sync_dns_entry_warning (CEPageIP6 *self)
{
        g_autoptr(GVariant) method_variant = NULL;
        const gchar *method;

        method_variant = g_action_group_get_action_state (self->method_group, "ip6method");
        method = g_variant_get_string (method_variant, NULL);

        if (gtk_entry_get_text_length (self->dns_entry) &&
            gtk_switch_get_active (self->auto_dns_switch) &&
            (g_strcmp0 (method, "automatic") == 0 || g_strcmp0 (method, "dhcp") == 0)) {
                gtk_entry_set_icon_from_icon_name (self->dns_entry, GTK_ENTRY_ICON_SECONDARY, "dialog-warning-symbolic");
                gtk_entry_set_icon_tooltip_text (self->dns_entry, GTK_ENTRY_ICON_SECONDARY, _("Automatic DNS is enabled. Did you intend to disable Automatic DNS?"));
                gtk_widget_add_css_class (GTK_WIDGET (self->dns_entry), "warning");
        } else {
                gtk_entry_set_icon_from_icon_name (self->dns_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
                gtk_entry_set_icon_tooltip_text (self->dns_entry, GTK_ENTRY_ICON_SECONDARY, NULL);
                gtk_widget_remove_css_class (GTK_WIDGET (self->dns_entry), "warning");
        }
}

static void
method_changed (CEPageIP6 *self)
{
        gboolean addr_enabled;
        gboolean dns_enabled;
        gboolean routes_enabled;
        gboolean auto_dns_enabled;
        gboolean auto_routes_enabled;
        g_autoptr(GVariant) method_variant = NULL;
        const gchar *method;

        method_variant = g_action_group_get_action_state (self->method_group, "ip6method");
        method = g_variant_get_string (method_variant, NULL);

        if (g_str_equal (method, "disabled") ||
            g_str_equal (method, "shared")) {
                addr_enabled = FALSE;
                dns_enabled = FALSE;
                routes_enabled = FALSE;
                auto_dns_enabled = FALSE;
                auto_routes_enabled = FALSE;
        } else {
                addr_enabled = g_str_equal (method, "manual");
                dns_enabled = !g_str_equal (method, "local");
                routes_enabled = !g_str_equal (method, "local");
                auto_dns_enabled = g_str_equal (method, "automatic") || g_str_equal (method, "dhcp");
                auto_routes_enabled = g_str_equal (method, "automatic");
        }

        gtk_widget_set_visible (GTK_WIDGET (self->address_box), addr_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->dns_box), dns_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->routes_box), routes_enabled);

        gtk_widget_set_sensitive (GTK_WIDGET (self->auto_dns_label), auto_dns_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->auto_dns_switch), auto_dns_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->auto_routes_label), auto_routes_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->auto_routes_switch), auto_routes_enabled);

        sync_dns_entry_warning (self);

        ce_page_changed (CE_PAGE (self));
}

static void
update_row_sensitivity (CEPageIP6 *self, GtkWidget *list)
{
        GtkWidget *child;
        gint rows = 0, i = 0;

        for (child = gtk_widget_get_first_child (GTK_WIDGET (list));
             child;
             child = gtk_widget_get_next_sibling (child)) {
                GtkWidget *button;

                button = GTK_WIDGET (g_object_get_data (G_OBJECT (child), "delete-button"));
                if (button != NULL)
                        rows++;
        }
        for (child = gtk_widget_get_first_child (GTK_WIDGET (list));
             child;
             child = gtk_widget_get_next_sibling (child)) {
                GtkWidget *button;

                button = GTK_WIDGET (g_object_get_data (G_OBJECT (child), "delete-button"));
                if (button != NULL)
                        gtk_widget_set_sensitive (button, rows > 1 && ++i < rows);
        }
}

static void
remove_row (CEPageIP6 *self, GtkButton *button)
{
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *list;

        row_box = gtk_widget_get_parent (GTK_WIDGET (button));
        row = gtk_widget_get_parent (row_box);
        list = gtk_widget_get_parent (row);

        gtk_list_box_remove (GTK_LIST_BOX (list), row);

        ce_page_changed (CE_PAGE (self));

        update_row_sensitivity (self, list);
}

static gboolean
validate_row (GtkWidget *row)
{
        GtkWidget *child;
        GtkWidget *box;
        gboolean valid;

        valid = FALSE;
        box = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row));

        for (child = gtk_widget_get_first_child (box);
             child;
             child = gtk_widget_get_next_sibling (child)) {
                if (!GTK_IS_ENTRY (child))
                        continue;

                valid = valid || gtk_entry_get_text_length (GTK_ENTRY (child)) > 0;
        }

        return valid;
}

static void
add_address_row (CEPageIP6   *self,
                 const gchar *address,
                 const gchar *network,
                 const gchar *gateway)
{
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *widget;
        GtkWidget *delete_button;

        row = gtk_list_box_row_new ();
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

        row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class (row_box, "linked");

        widget = GTK_WIDGET (ce_ip_address_entry_new (AF_INET6));
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_address_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "address", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), address);
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->address_address_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_address_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "prefix", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), network);
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->address_prefix_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        widget = GTK_WIDGET (ce_ip_address_entry_new (AF_INET6));
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_address_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), gateway ? gateway : "");
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->address_gateway_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        delete_button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
        gtk_widget_set_sensitive (delete_button, FALSE);
        g_signal_connect_object (delete_button, "clicked", G_CALLBACK (remove_row), self, G_CONNECT_SWAPPED);
        gtk_accessible_update_property (GTK_ACCESSIBLE (delete_button),
                                        GTK_ACCESSIBLE_PROPERTY_LABEL, _("Delete Address"),
                                        -1);
        gtk_box_append (GTK_BOX (row_box), delete_button);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        gtk_size_group_add_widget (self->address_sizegroup, delete_button);

        gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), row_box);
        gtk_list_box_append (GTK_LIST_BOX (self->address_list), row);

        update_row_sensitivity (self, self->address_list);
}

static void
ensure_empty_address_row (CEPageIP6 *self)
{
        GtkWidget *child = gtk_widget_get_last_child (self->address_list);

        /* Add the last, stub row if needed*/
        if (!child || validate_row (child))
                add_address_row (self, "", "", "");
}

static void
add_address_box (CEPageIP6 *self)
{
        GtkWidget *list;
        gint i;

        self->address_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_box_append (self->address_box, list);

        for (i = 0; i < nm_setting_ip_config_get_num_addresses (self->setting); i++) {
                NMIPAddress *addr;
                g_autofree gchar *netmask = NULL;

                addr = nm_setting_ip_config_get_address (self->setting, i);
                netmask = g_strdup_printf ("%u", nm_ip_address_get_prefix (addr));
                add_address_row (self, nm_ip_address_get_address (addr), netmask,
                                 i == 0 ? nm_setting_ip_config_get_gateway (self->setting) : NULL);
        }
        if (nm_setting_ip_config_get_num_addresses (self->setting) == 0)
                ensure_empty_address_row (self);
}

static void
add_dns_section (CEPageIP6 *self)
{
        GString *string;
        gint i;

        gtk_switch_set_active (self->auto_dns_switch, !nm_setting_ip_config_get_ignore_auto_dns (self->setting));
        g_signal_connect_object (self->auto_dns_switch, "notify::active", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->auto_dns_switch, "notify::active", G_CALLBACK (sync_dns_entry_warning), self, G_CONNECT_SWAPPED);

        string = g_string_new ("");

        for (i = 0; i < nm_setting_ip_config_get_num_dns (self->setting); i++) {
                const char *address;

                address = nm_setting_ip_config_get_dns (self->setting, i);

                if (i > 0)
                        g_string_append (string, ", ");

                g_string_append (string, address);

        }

        gtk_editable_set_text (GTK_EDITABLE (self->dns_entry), string->str);

        g_signal_connect_object (self->dns_entry, "notify::text", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (self->dns_entry, "notify::text", G_CALLBACK (sync_dns_entry_warning), self, G_CONNECT_SWAPPED);

        sync_dns_entry_warning (self);

        g_string_free (string, TRUE);
}

static void
add_route_row (CEPageIP6   *self,
               const gchar *address,
               const gchar *prefix,
               const gchar *gateway,
               const gchar *metric)
{
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *widget;
        GtkWidget *delete_button;

        row = gtk_list_box_row_new ();
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

        row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class (row_box, "linked");

        widget = GTK_WIDGET (ce_ip_address_entry_new (AF_INET6));
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "address", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), address);
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->routes_address_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        gtk_size_group_add_widget (self->routes_address_sizegroup, widget);

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "prefix", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), prefix ? prefix : "");
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->routes_prefix_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        gtk_size_group_add_widget (self->routes_prefix_sizegroup, widget);

        widget = GTK_WIDGET (ce_ip_address_entry_new (AF_INET6));
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), gateway);
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->routes_gateway_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        gtk_size_group_add_widget (self->routes_gateway_sizegroup, widget);

        widget = gtk_entry_new ();
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "metric", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), metric ? metric : "");
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 5);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->routes_prefix_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        gtk_size_group_add_widget (self->routes_metric_sizegroup, widget);

        delete_button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
        g_signal_connect_object (delete_button, "clicked", G_CALLBACK (remove_row), self, G_CONNECT_SWAPPED);
        gtk_accessible_update_property (GTK_ACCESSIBLE (delete_button),
                                        GTK_ACCESSIBLE_PROPERTY_LABEL, _("Delete Route"),
                                        -1);
        gtk_widget_set_halign (delete_button, GTK_ALIGN_CENTER);
        gtk_widget_set_valign (delete_button, GTK_ALIGN_CENTER);
        gtk_box_append (GTK_BOX (row_box), delete_button);
        g_object_set_data (G_OBJECT (row), "delete-button", delete_button);

        gtk_size_group_add_widget (self->routes_sizegroup, delete_button);

        gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), row_box);
        gtk_list_box_append (GTK_LIST_BOX (self->routes_list), row);

        update_row_sensitivity (self, self->routes_list);
}

static void
ensure_empty_routes_row (CEPageIP6 *self)
{
        GtkWidget *child = gtk_widget_get_last_child (self->routes_list);

        /* Add the last, stub row if needed*/
        if (!child || validate_row (child))
                add_route_row (self, "", NULL, "", NULL);
}

static void
add_empty_route_row (CEPageIP6 *self)
{
        add_route_row (self, "", NULL, "", NULL);
}

static void
add_route_config_box (CEPageIP6 *self)
{
        GtkWidget *list;
        gint i;

        self->routes_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_box_append (self->route_config_box, list);
        gtk_switch_set_active (self->auto_routes_switch, !nm_setting_ip_config_get_ignore_auto_routes (self->setting));
        g_signal_connect_object (self->auto_routes_switch, "notify::active", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        for (i = 0; i < nm_setting_ip_config_get_num_routes (self->setting); i++) {
                NMIPRoute *route;
                g_autofree gchar *prefix = NULL;
                g_autofree gchar *metric = NULL;

                route = nm_setting_ip_config_get_route (self->setting, i);
                prefix = g_strdup_printf ("%u", nm_ip_route_get_prefix (route));
                metric = g_strdup_printf ("%" G_GINT64_FORMAT, nm_ip_route_get_metric (route));
                add_route_row (self, nm_ip_route_get_dest (route),
                               prefix,
                               nm_ip_route_get_next_hop (route),
                               metric);
        }
        if (nm_setting_ip_config_get_num_routes (self->setting) == 0)
                add_empty_route_row (self);
}

static void
connect_ip6_page (CEPageIP6 *self)
{
        const gchar *str_method;
        gchar *method;

        add_address_box (self);
        add_dns_section (self);
        add_route_config_box (self);

        str_method = nm_setting_ip_config_get_method (self->setting);

        method = "automatic";
        if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_DHCP) == 0) {
                method = "dhcp";
        } else if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_LINK_LOCAL) == 0) {
                method = "local";
        } else if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL) == 0) {
                method = "manual";
        } else if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_SHARED) == 0) {
                method = "shared";
        } else if (g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_DISABLED) == 0 ||
                   g_strcmp0 (str_method, NM_SETTING_IP6_CONFIG_METHOD_IGNORE) == 0) {
                method = "disabled";
        }

        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->never_default_check),
                                     nm_setting_ip_config_get_never_default (self->setting));
        g_signal_connect_object (self->never_default_check, "toggled", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        g_action_group_change_action_state (self->method_group, "ip6method", g_variant_new_string (method));

        method_changed (self);
}

static gboolean
ui_to_setting (CEPageIP6 *self)
{
        GtkWidget *child;
        g_autoptr(GVariant) method_variant = NULL;
        const gchar *method;
        gboolean ignore_auto_dns;
        gboolean ignore_auto_routes;
        gboolean never_default;
        gboolean add_addresses = FALSE;
        gboolean add_routes = FALSE;
        gboolean ret = TRUE;
        GStrv dns_addresses = NULL;
        gchar *dns_text = NULL;
        guint i;

        method_variant = g_action_group_get_action_state (self->method_group, "ip6method");
        method = g_variant_get_string (method_variant, NULL);
        if (g_str_equal (method, "disabled"))
                method = NM_SETTING_IP6_CONFIG_METHOD_DISABLED;
        else if (g_str_equal (method, "manual"))
                method = NM_SETTING_IP6_CONFIG_METHOD_MANUAL;
        else if (g_str_equal (method, "local"))
                method = NM_SETTING_IP6_CONFIG_METHOD_LINK_LOCAL;
        else if (g_str_equal (method, "dhcp"))
                method = NM_SETTING_IP6_CONFIG_METHOD_DHCP;
        else if (g_str_equal (method, "automatic"))
                method = NM_SETTING_IP6_CONFIG_METHOD_AUTO;
        else if (g_str_equal (method, "shared"))
                method = NM_SETTING_IP6_CONFIG_METHOD_SHARED;
        else
                g_assert_not_reached ();

        nm_setting_ip_config_clear_addresses (self->setting);
        if (g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL)) {
                add_addresses = TRUE;
        } else {
                g_object_set (G_OBJECT (self->setting),
                              NM_SETTING_IP_CONFIG_GATEWAY, NULL,
                              NULL);
       }

        for (child = gtk_widget_get_first_child (self->address_list);
             add_addresses && child;
             child = gtk_widget_get_next_sibling (child)) {
                GtkWidget *row = child;
                CEIPAddressEntry *address_entry;
                CEIPAddressEntry *gateway_entry;
                const gchar *text_prefix;
                guint32 prefix;
                gchar *end;
                NMIPAddress *addr;

                address_entry = CE_IP_ADDRESS_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!address_entry)
                        continue;

                text_prefix = gtk_editable_get_text (GTK_EDITABLE (g_object_get_data (G_OBJECT (row), "prefix")));
                gateway_entry = CE_IP_ADDRESS_ENTRY (g_object_get_data (G_OBJECT (row), "gateway"));

                if (ce_ip_address_entry_is_empty (address_entry) && !*text_prefix && ce_ip_address_entry_is_empty (gateway_entry)) {
                        /* ignore empty rows */
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "prefix"));
                        continue;
                }

                if (!ce_ip_address_entry_is_valid (address_entry))
                        ret = FALSE;

                prefix = strtoul (text_prefix, &end, 10);
                if (!end || *end || prefix == 0 || prefix > 128) {
                        widget_set_error (g_object_get_data (G_OBJECT (row), "prefix"));
                        ret = FALSE;
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "prefix"));
                }

                if (!ce_ip_address_entry_is_valid (gateway_entry))
                        ret = FALSE;

                if (!ret)
                        continue;

                addr = nm_ip_address_new (AF_INET6, gtk_editable_get_text (GTK_EDITABLE (address_entry)), prefix, NULL);
                if (!ce_ip_address_entry_is_empty (gateway_entry))
                        g_object_set (G_OBJECT (self->setting),
                                      NM_SETTING_IP_CONFIG_GATEWAY, gtk_editable_get_text (GTK_EDITABLE (gateway_entry)),
                                      NULL);
                nm_setting_ip_config_add_address (self->setting, addr);

                if (!gtk_widget_get_next_sibling (row))
                        ensure_empty_address_row (self);
        }

        nm_setting_ip_config_clear_dns (self->setting);
        dns_text = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->dns_entry))));

        if (g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_AUTO) ||
            g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_DHCP) ||
            g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL))
                dns_addresses = g_strsplit_set (dns_text, ", ", -1);
        else
                dns_addresses = NULL;

        for (i = 0; dns_addresses && dns_addresses[i]; i++) {
                const gchar *text;
                struct in6_addr tmp_addr;

                text = dns_addresses[i];

                if (!text || !*text)
                        continue;

                if (inet_pton (AF_INET6, text, &tmp_addr) <= 0) {
                        g_clear_pointer (&dns_addresses, g_strfreev);
                        widget_set_error (GTK_WIDGET (self->dns_entry));
                        ret = FALSE;
                        break;
                } else {
                        widget_unset_error (GTK_WIDGET (self->dns_entry));
                        nm_setting_ip_config_add_dns (self->setting, text);
                }
        }

        if (dns_text[0] == '\0')
                widget_unset_error (GTK_WIDGET (self->dns_entry));

        nm_setting_ip_config_clear_routes (self->setting);
        add_routes = g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_AUTO) ||
                     g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_DHCP) ||
                     g_str_equal (method, NM_SETTING_IP6_CONFIG_METHOD_MANUAL);

        for (child = gtk_widget_get_first_child (self->routes_list);
             add_routes && child;
             child = gtk_widget_get_next_sibling (child)) {
                GtkWidget *row = child;
                CEIPAddressEntry *address_entry;
                CEIPAddressEntry *gateway_entry;
                const gchar *text_prefix;
                const gchar *text_metric;
                guint32 prefix;
                gint64 metric;
                gchar *end;
                NMIPRoute *route;

                address_entry = CE_IP_ADDRESS_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!address_entry)
                        continue;

                text_prefix = gtk_editable_get_text (GTK_EDITABLE (g_object_get_data (G_OBJECT (row), "prefix")));
                gateway_entry = CE_IP_ADDRESS_ENTRY (g_object_get_data (G_OBJECT (row), "gateway"));
                text_metric = gtk_editable_get_text (GTK_EDITABLE (g_object_get_data (G_OBJECT (row), "metric")));

                if (ce_ip_address_entry_is_empty (address_entry) && !*text_prefix && ce_ip_address_entry_is_empty (gateway_entry) && !*text_metric) {
                        /* ignore empty rows */
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "prefix"));
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "metric"));
                        continue;
                }

                if (!ce_ip_address_entry_is_valid (address_entry))
                        ret = FALSE;

                prefix = strtoul (text_prefix, &end, 10);
                if (!end || *end || prefix == 0 || prefix > 128) {
                        widget_set_error (g_object_get_data (G_OBJECT (row), "prefix"));
                        ret = FALSE;
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "prefix"));
                }

                if (!ce_ip_address_entry_is_valid (gateway_entry))
                        ret = FALSE;

                metric = -1;
                if (*text_metric) {
                        errno = 0;
                        metric = g_ascii_strtoull (text_metric, NULL, 10);
                        if (errno) {
                                widget_set_error (g_object_get_data (G_OBJECT (row), "metric"));
                                ret = FALSE;
                        } else {
                                widget_unset_error (g_object_get_data (G_OBJECT (row), "metric"));
                        }
                } else {
                        widget_unset_error (g_object_get_data (G_OBJECT (row), "metric"));
                }

                if (!ret)
                        continue;

                route = nm_ip_route_new (AF_INET6,
                                         gtk_editable_get_text (GTK_EDITABLE (address_entry)),
                                         prefix,
                                         gtk_editable_get_text (GTK_EDITABLE (gateway_entry)),
                                         metric,
                                         NULL);
                nm_setting_ip_config_add_route (self->setting, route);
                nm_ip_route_unref (route);

                if (!gtk_widget_get_next_sibling (row))
                        ensure_empty_routes_row (self);
        }

        if (!ret)
                goto out;

        ignore_auto_dns = !gtk_switch_get_active (self->auto_dns_switch);
        ignore_auto_routes = !gtk_switch_get_active (self->auto_routes_switch);
        never_default = gtk_check_button_get_active (self->never_default_check);

        g_object_set (self->setting,
                      NM_SETTING_IP_CONFIG_METHOD, method,
                      NM_SETTING_IP_CONFIG_IGNORE_AUTO_DNS, ignore_auto_dns,
                      NM_SETTING_IP_CONFIG_IGNORE_AUTO_ROUTES, ignore_auto_routes,
                      NM_SETTING_IP_CONFIG_NEVER_DEFAULT, never_default,
                      NULL);

out:
        g_clear_pointer (&dns_addresses, g_strfreev);
        g_clear_pointer (&dns_text, g_free);

        return ret;
}

static void
on_ip6_method_activated_cb (GSimpleAction* action,
                            GVariant* parameter,
                            gpointer user_data)
{
        CEPageIP6 *self = CE_PAGE_IP6 (user_data);
        g_simple_action_set_state (action, parameter);

        method_changed (self);
}

static const gchar *
ce_page_ip6_get_title (CEPage *page)
{
        return _("IPv6");
}

static gboolean
ce_page_ip6_validate (CEPage        *self,
                      NMConnection  *connection,
                      GError       **error)
{
        if (!ui_to_setting (CE_PAGE_IP6 (self)))
                return FALSE;

        return nm_setting_verify (NM_SETTING (CE_PAGE_IP6 (self)->setting), NULL, error);
}

static void
ce_page_ip6_init (CEPageIP6 *self)
{
        const GActionEntry ip6_entries[] = {
                { "ip6method", on_ip6_method_activated_cb, "s", "'automatic'", NULL, { 0 } },
        };
        self->method_group = G_ACTION_GROUP (g_simple_action_group_new ());

        g_action_map_add_action_entries (G_ACTION_MAP (self->method_group), ip6_entries, G_N_ELEMENTS (ip6_entries), self);
        gtk_widget_insert_action_group (GTK_WIDGET (self), "ip6page", G_ACTION_GROUP (self->method_group));

        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_ip6_class_init (CEPageIP6Class *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/ip6-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, address_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, address_address_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, address_prefix_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, address_gateway_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, address_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, auto_dns_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, auto_dns_switch);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, auto_routes_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, auto_routes_switch);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, content_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, dns_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, dns_entry);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, main_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, never_default_check);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, route_config_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_address_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_address_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_prefix_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_prefix_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_gateway_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_gateway_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_metric_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_metric_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP6, routes_sizegroup);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_title = ce_page_ip6_get_title;
        iface->validate = ce_page_ip6_validate;
}

CEPageIP6 *
ce_page_ip6_new (NMConnection     *connection,
                 NMClient         *client)
{
        CEPageIP6 *self;

        self = CE_PAGE_IP6 (g_object_new (ce_page_ip6_get_type (), NULL));

        self->setting = nm_connection_get_setting_ip6_config (connection);
        if (!self->setting) {
                self->setting = NM_SETTING_IP_CONFIG (nm_setting_ip6_config_new ());
                nm_connection_add_setting (connection, NM_SETTING (self->setting));
        }

        connect_ip6_page (self);

        return self;
}
