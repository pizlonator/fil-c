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
#include "ce-netmask-entry.h"
#include "ce-page.h"
#include "ce-page-ip4.h"
#include "ui-helpers.h"

static void ensure_empty_address_row (CEPageIP4 *self);
static void ensure_empty_routes_row (CEPageIP4 *self);

struct _CEPageIP4
{
        AdwBin             parent;

        GtkLabel          *address_address_label;
        GtkBox            *address_box;
        GtkLabel          *address_gateway_label;
        GtkLabel          *address_netmask_label;
        GtkSizeGroup      *address_sizegroup;
        GtkLabel          *auto_dns_label;
        GtkSwitch         *auto_dns_switch;
        GtkLabel          *auto_routes_label;
        GtkSwitch         *auto_routes_switch;
        GtkBox            *content_box;
        GtkBox            *dns_box;
        GtkEntry          *dns_entry;
        GtkGrid           *main_box;
        GtkCheckButton    *never_default_check;
        GtkBox            *routes_box;
        GtkBox            *route_config_box;
        GtkLabel          *routes_address_label;
        GtkLabel          *routes_gateway_label;
        GtkLabel          *routes_netmask_label;
        GtkLabel          *routes_metric_label;
        GtkSizeGroup      *routes_address_sizegroup;
        GtkSizeGroup      *routes_gateway_sizegroup;
        GtkSizeGroup      *routes_netmask_sizegroup;
        GtkSizeGroup      *routes_metric_sizegroup;
        GtkSizeGroup      *routes_sizegroup;

        NMSettingIPConfig *setting;

        GtkWidget      *address_list;
        GtkWidget      *routes_list;

        GActionGroup      *method_group;
};

static void ce_page_iface_init (CEPageInterface *);

G_DEFINE_TYPE_WITH_CODE (CEPageIP4, ce_page_ip4, ADW_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE (ce_page_get_type (), ce_page_iface_init))

enum {
        METHOD_COL_NAME,
        METHOD_COL_METHOD
};

static void
sync_dns_entry_warning (CEPageIP4 *self)
{
        g_autoptr(GVariant) method_variant = NULL;
        const gchar *method;

        method_variant = g_action_group_get_action_state (self->method_group, "ip4method");
        method = g_variant_get_string (method_variant, NULL);

        if (gtk_entry_get_text_length (self->dns_entry) &&
            gtk_switch_get_active (self->auto_dns_switch) &&
            g_strcmp0 (method, "automatic") == 0) {
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
method_changed (CEPageIP4 *self)
{
        gboolean addr_enabled;
        gboolean dns_enabled;
        gboolean routes_enabled;
        gboolean auto_enabled;
        g_autoptr(GVariant) method_variant = NULL;
        const gchar *method;

        method_variant = g_action_group_get_action_state (self->method_group, "ip4method");
        method = g_variant_get_string (method_variant, NULL);

        if (g_str_equal (method, "disabled") ||
            g_str_equal (method, "shared")) {
                addr_enabled = FALSE;
                dns_enabled = FALSE;
                routes_enabled = FALSE;
                auto_enabled = FALSE;
        } else {
                addr_enabled = g_str_equal (method, "manual");
                dns_enabled = !g_str_equal (method, "local");
                routes_enabled = !g_str_equal (method, "local");
                auto_enabled = g_str_equal (method, "automatic");
        }

        gtk_widget_set_visible (GTK_WIDGET (self->address_box), addr_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->dns_box), dns_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->routes_box), routes_enabled);

        gtk_widget_set_sensitive (GTK_WIDGET (self->auto_dns_label), auto_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->auto_dns_switch), auto_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->auto_routes_label), auto_enabled);
        gtk_widget_set_sensitive (GTK_WIDGET (self->auto_routes_switch), auto_enabled);

        sync_dns_entry_warning (self);

        ce_page_changed (CE_PAGE (self));
}

static void
update_row_sensitivity (CEPageIP4 *self, GtkWidget *list)
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
update_row_gateway_sensitivity (CEPageIP4 *self)
{
        GtkWidget *child;
        gint rows = 0;

        for (child = gtk_widget_get_first_child (self->address_list);
             child;
             child = gtk_widget_get_next_sibling (child)) {
                GtkWidget *entry;

                entry = GTK_WIDGET (g_object_get_data (G_OBJECT (child), "gateway"));

                gtk_widget_set_sensitive (entry, (rows == 0));

                rows++;
        }
}

static void
remove_row (CEPageIP4 *self, GtkButton *button)
{
        GtkWidget *list;
        GtkWidget *row;
        GtkWidget *row_box;

        row_box = gtk_widget_get_parent (GTK_WIDGET (button));
        row = gtk_widget_get_parent (row_box);
        list = gtk_widget_get_parent (row);

        gtk_list_box_remove (GTK_LIST_BOX (list), row);

        ce_page_changed (CE_PAGE (self));

        update_row_sensitivity (self, list);
        if (list == self->address_list)
                update_row_gateway_sensitivity (self);
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
add_address_row (CEPageIP4   *self,
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

        widget = GTK_WIDGET (ce_ip_address_entry_new (AF_INET));
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

        widget = GTK_WIDGET (ce_netmask_entry_new ());
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_address_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "netmask", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), network);
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->address_netmask_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        widget = GTK_WIDGET (ce_ip_address_entry_new (AF_INET));
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

        update_row_gateway_sensitivity (self);
        update_row_sensitivity (self, self->address_list);
}

static void
ensure_empty_address_row (CEPageIP4 *self)
{
        GtkWidget *child = gtk_widget_get_last_child (self->address_list);

        /* Add the last, stub row if needed*/
        if (!child || validate_row (child))
                add_address_row (self, "", "", "");
}

static void
add_address_box (CEPageIP4 *self)
{
        GtkWidget *list;
        gint i;

        self->address_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_box_append (self->address_box, list);

        for (i = 0; i < nm_setting_ip_config_get_num_addresses (self->setting); i++) {
                NMIPAddress *addr;
                struct in_addr tmp_addr;
                gchar network[INET_ADDRSTRLEN + 1];

                addr = nm_setting_ip_config_get_address (self->setting, i);
                if (!addr)
                        continue;

                tmp_addr.s_addr = nm_utils_ip4_prefix_to_netmask (nm_ip_address_get_prefix (addr));
                (void) inet_ntop (AF_INET, &tmp_addr, &network[0], sizeof (network));

                add_address_row (self,
                                 nm_ip_address_get_address (addr),
                                 network,
                                 i == 0 ? nm_setting_ip_config_get_gateway (self->setting) : "");
        }
        if (nm_setting_ip_config_get_num_addresses (self->setting) == 0)
                ensure_empty_address_row (self);
}

static void
add_dns_section (CEPageIP4 *self)
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
add_route_row (CEPageIP4   *self,
               const gchar *address,
               const gchar *netmask,
               const gchar *gateway,
               gint         metric)
{
        GtkWidget *row;
        GtkWidget *row_box;
        GtkWidget *widget;
        GtkWidget *delete_button;

        row = gtk_list_box_row_new ();
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), FALSE);

        row_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class (row_box, "linked");

        widget = GTK_WIDGET (ce_ip_address_entry_new (AF_INET));
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

        widget = GTK_WIDGET (ce_netmask_entry_new ());
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "netmask", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), netmask);
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 16);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->routes_netmask_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        gtk_size_group_add_widget (self->routes_netmask_sizegroup, widget);

        widget = GTK_WIDGET (ce_ip_address_entry_new (AF_INET));
        g_signal_connect_object (widget, "changed", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (widget, "activate", G_CALLBACK (ensure_empty_routes_row), self, G_CONNECT_SWAPPED);
        g_object_set_data (G_OBJECT (row), "gateway", widget);
        gtk_editable_set_text (GTK_EDITABLE (widget), gateway ? gateway : "");
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
        if (metric >= 0) {
                g_autofree gchar *s = g_strdup_printf ("%d", metric);
                gtk_editable_set_text (GTK_EDITABLE (widget), s);
        }
        gtk_editable_set_width_chars (GTK_EDITABLE (widget), 5);
        gtk_widget_set_hexpand (widget, TRUE);
        gtk_accessible_update_relation (GTK_ACCESSIBLE (widget),
                                        GTK_ACCESSIBLE_RELATION_LABELLED_BY, self->routes_metric_label, NULL,
                                        -1);
        gtk_box_append (GTK_BOX (row_box), widget);

        gtk_size_group_add_widget (self->routes_metric_sizegroup, widget);

        delete_button = gtk_button_new_from_icon_name ("edit-delete-symbolic");
        gtk_widget_set_sensitive (delete_button, FALSE);
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
ensure_empty_routes_row (CEPageIP4 *self)
{
        GtkWidget *child = gtk_widget_get_last_child (self->routes_list);

        /* Add the last, stub row if needed*/
        if (!child || validate_row (child))
                add_route_row (self, "", "", "", -1);
}

static void
add_route_config_box (CEPageIP4 *self)
{
        GtkWidget *list;
        gint i;

        self->routes_list = list = gtk_list_box_new ();
        gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_NONE);
        gtk_box_append (GTK_BOX (self->route_config_box), list);
        gtk_switch_set_active (self->auto_routes_switch, !nm_setting_ip_config_get_ignore_auto_routes (self->setting));
        g_signal_connect_object (self->auto_routes_switch, "notify::active", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        for (i = 0; i < nm_setting_ip_config_get_num_routes (self->setting); i++) {
                NMIPRoute *route;
                struct in_addr tmp_addr;
                gchar netmask[INET_ADDRSTRLEN + 1];

                route = nm_setting_ip_config_get_route (self->setting, i);
                if (!route)
                        continue;

                tmp_addr.s_addr = nm_utils_ip4_prefix_to_netmask (nm_ip_route_get_prefix (route));
                (void) inet_ntop (AF_INET, &tmp_addr, &netmask[0], sizeof (netmask));

                add_route_row (self,
                               nm_ip_route_get_dest (route),
                               netmask,
                               nm_ip_route_get_next_hop (route),
                               nm_ip_route_get_metric (route));
        }
        if (nm_setting_ip_config_get_num_routes (self->setting) == 0)
                ensure_empty_routes_row (self);
}

static void
connect_ip4_page (CEPageIP4 *self)
{
        const gchar *str_method;
        gchar *method;

        add_address_box (self);
        add_dns_section (self);
        add_route_config_box (self);

        str_method = nm_setting_ip_config_get_method (self->setting);

        method = "automatic";
        if (g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_LINK_LOCAL) == 0) {
                method = "local";
        } else if (g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL) == 0) {
                method = "manual";
        } else if (g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_SHARED) == 0) {
                method = "shared";
        } else if (g_strcmp0 (str_method, NM_SETTING_IP4_CONFIG_METHOD_DISABLED) == 0) {
                method = "disabled";
        }

        gtk_check_button_set_active (GTK_CHECK_BUTTON (self->never_default_check),
                                     nm_setting_ip_config_get_never_default (self->setting));
        g_signal_connect_object (self->never_default_check, "toggled", G_CALLBACK (ce_page_changed), self, G_CONNECT_SWAPPED);

        g_action_group_change_action_state (self->method_group, "ip4method", g_variant_new_string (method));

        method_changed (self);
}

static gboolean
ui_to_setting (CEPageIP4 *self)
{
        const gchar *method;
        g_autoptr(GVariant) method_variant = NULL;
        gboolean ignore_auto_dns;
        gboolean ignore_auto_routes;
        gboolean never_default;
        GPtrArray *addresses = NULL;
        GPtrArray *dns_servers = NULL;
        GPtrArray *routes = NULL;
        GtkWidget *child;
        GStrv dns_addresses = NULL;
        gboolean ret = TRUE;
        const char *default_gateway = NULL;
        gboolean add_addresses = FALSE;
        gboolean add_routes = FALSE;
        gchar *dns_text = NULL;
        guint i;

        method_variant = g_action_group_get_action_state (self->method_group, "ip4method");
        method = g_variant_get_string (method_variant, NULL);
        if (g_str_equal (method, "disabled"))
                method = NM_SETTING_IP4_CONFIG_METHOD_DISABLED;
        else if (g_str_equal (method, "automatic"))
                method = NM_SETTING_IP4_CONFIG_METHOD_AUTO;
        else if (g_str_equal (method, "local"))
                method = NM_SETTING_IP4_CONFIG_METHOD_LINK_LOCAL;
        else if (g_str_equal (method, "manual"))
                method = NM_SETTING_IP4_CONFIG_METHOD_MANUAL;
        else if (g_str_equal (method, "shared"))
                method = NM_SETTING_IP4_CONFIG_METHOD_SHARED;
        else
                g_assert_not_reached ();

        addresses = g_ptr_array_new_with_free_func ((GDestroyNotify) nm_ip_address_unref);
        add_addresses = g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL);

        for (child = gtk_widget_get_first_child (self->address_list);
             add_addresses && child;
             child = gtk_widget_get_next_sibling (child)) {
                GtkWidget *row = child;
                CEIPAddressEntry *address_entry;
                CENetmaskEntry *netmask_entry;
                CEIPAddressEntry *gateway_entry;
                NMIPAddress *addr;

                address_entry = CE_IP_ADDRESS_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!address_entry)
                        continue;

                netmask_entry = CE_NETMASK_ENTRY (g_object_get_data (G_OBJECT (row), "netmask"));
                gateway_entry = CE_IP_ADDRESS_ENTRY (g_object_get_data (G_OBJECT (row), "gateway"));

                if (ce_ip_address_entry_is_empty (address_entry) && ce_netmask_entry_is_empty (netmask_entry) && ce_ip_address_entry_is_empty (gateway_entry)) {
                        /* ignore empty rows */
                        continue;
                }

                if (!ce_ip_address_entry_is_valid (address_entry))
                        ret = FALSE;

                if (!ce_netmask_entry_is_valid (netmask_entry))
                        ret = FALSE;

                if (!ce_ip_address_entry_is_valid (gateway_entry)) {
                        ret = FALSE;
                } else {
                         if (!ce_ip_address_entry_is_empty (gateway_entry)) {
                                 g_assert (default_gateway == NULL);
                                 default_gateway = gtk_editable_get_text (GTK_EDITABLE (gateway_entry));
                         }
                }

                if (!ret)
                        continue;

                addr = nm_ip_address_new (AF_INET, gtk_editable_get_text (GTK_EDITABLE (address_entry)), ce_netmask_entry_get_prefix (netmask_entry), NULL);
                if (addr)
                        g_ptr_array_add (addresses, addr);

                if (!gtk_widget_get_next_sibling (row))
                        ensure_empty_address_row (self);
        }

        if (addresses->len == 0) {
                g_ptr_array_free (addresses, TRUE);
                addresses = NULL;
        }

        dns_servers = g_ptr_array_new_with_free_func (g_free);
        dns_text = g_strstrip (g_strdup (gtk_editable_get_text (GTK_EDITABLE (self->dns_entry))));
        if (g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_AUTO) ||
            g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL))
                dns_addresses = g_strsplit_set (dns_text, ", ", -1);
        else
                dns_addresses = NULL;

        for (i = 0; dns_addresses && dns_addresses[i]; i++) {
                const gchar *text;

                text = dns_addresses[i];

                if (!text || !*text)
                        continue;

                if (!nm_utils_ipaddr_valid (AF_INET, text)) {
                        g_ptr_array_remove_range (dns_servers, 0, dns_servers->len);
                        widget_set_error (GTK_WIDGET (self->dns_entry));
                        ret = FALSE;
                        break;
                } else {
                        widget_unset_error (GTK_WIDGET (self->dns_entry));
                        g_ptr_array_add (dns_servers, g_strdup (text));
                }
        }
        g_clear_pointer (&dns_addresses, g_strfreev);

        if (dns_text[0] == '\0')
                widget_unset_error (GTK_WIDGET (self->dns_entry));

        if (dns_servers->len == 0) {
                g_ptr_array_free (dns_servers, TRUE);
                dns_servers = NULL;
        } else {
                g_ptr_array_add (dns_servers, NULL);
        }

        routes = g_ptr_array_new_with_free_func ((GDestroyNotify) nm_ip_route_unref);
        add_routes = g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_AUTO) ||
                     g_str_equal (method, NM_SETTING_IP4_CONFIG_METHOD_MANUAL);

        for (child = gtk_widget_get_first_child (self->routes_list);
             add_routes && child;
             child = gtk_widget_get_next_sibling (child)) {
                GtkWidget *row = child;
                CEIPAddressEntry *address_entry;
                CENetmaskEntry *netmask_entry;
                CEIPAddressEntry *gateway_entry;
                const gchar *text_metric;
                gint64 metric;
                NMIPRoute *route;

                address_entry = CE_IP_ADDRESS_ENTRY (g_object_get_data (G_OBJECT (row), "address"));
                if (!address_entry)
                        continue;

                netmask_entry = CE_NETMASK_ENTRY (g_object_get_data (G_OBJECT (row), "netmask"));
                gateway_entry = CE_IP_ADDRESS_ENTRY (g_object_get_data (G_OBJECT (row), "gateway"));
                text_metric = gtk_editable_get_text (GTK_EDITABLE (g_object_get_data (G_OBJECT (row), "metric")));

                if (ce_ip_address_entry_is_empty (address_entry) && ce_netmask_entry_is_empty (netmask_entry) && ce_ip_address_entry_is_empty (gateway_entry) && !*text_metric) {
                        /* ignore empty rows */
                        continue;
                }

                if (!ce_ip_address_entry_is_valid (address_entry))
                        ret = FALSE;

                if (!ce_netmask_entry_is_valid (netmask_entry))
                        ret = FALSE;

                if (!ce_ip_address_entry_is_valid (gateway_entry))
                        ret = FALSE;

                metric = -1;
                if (*text_metric) {
                        errno = 0;
                        metric = g_ascii_strtoull (text_metric, NULL, 10);
                        if (errno || metric < 0 || metric > G_MAXUINT32) {
                                widget_set_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "metric")));
                                ret = FALSE;
                        } else {
                                widget_unset_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "metric")));
                        }
                } else {
                        widget_unset_error (GTK_WIDGET (g_object_get_data (G_OBJECT (row), "metric")));
                }

                if (!ret)
                        continue;

                route = nm_ip_route_new (AF_INET,
                                         gtk_editable_get_text (GTK_EDITABLE (address_entry)),
                                         ce_netmask_entry_get_prefix (netmask_entry),
                                         gtk_editable_get_text (GTK_EDITABLE (gateway_entry)),
                                         metric, NULL);
                if (route)
                        g_ptr_array_add (routes, route);

                if (!gtk_widget_get_next_sibling (row))
                        ensure_empty_routes_row (self);
        }

        if (routes->len == 0) {
                g_ptr_array_free (routes, TRUE);
                routes = NULL;
        }

        if (!ret)
                goto out;

        ignore_auto_dns = !gtk_switch_get_active (self->auto_dns_switch);
        ignore_auto_routes = !gtk_switch_get_active (self->auto_routes_switch);
        never_default = gtk_check_button_get_active (GTK_CHECK_BUTTON (self->never_default_check));

        g_object_set (self->setting,
                      NM_SETTING_IP_CONFIG_METHOD, method,
                      NM_SETTING_IP_CONFIG_ADDRESSES, addresses,
                      NM_SETTING_IP_CONFIG_GATEWAY, default_gateway,
                      NM_SETTING_IP_CONFIG_DNS, dns_servers ? dns_servers->pdata : NULL,
                      NM_SETTING_IP_CONFIG_ROUTES, routes,
                      NM_SETTING_IP_CONFIG_IGNORE_AUTO_DNS, ignore_auto_dns,
                      NM_SETTING_IP_CONFIG_IGNORE_AUTO_ROUTES, ignore_auto_routes,
                      NM_SETTING_IP_CONFIG_NEVER_DEFAULT, never_default,
                      NULL);

out:
        if (addresses)
                g_ptr_array_free (addresses, TRUE);

        if (dns_servers)
                g_ptr_array_free (dns_servers, TRUE);

        if (routes)
                g_ptr_array_free (routes, TRUE);

        g_clear_pointer (&dns_text, g_free);

        return ret;
}

static void
on_ip4_method_activated_cb (GSimpleAction* action,
                            GVariant* parameter,
                            gpointer user_data)
{
        CEPageIP4 *self = CE_PAGE_IP4 (user_data);
        g_simple_action_set_state (action, parameter);

        method_changed (self);
}

static const gchar *
ce_page_ip4_get_title (CEPage *page)
{
        return _("IPv4");
}

static gboolean
ce_page_ip4_validate (CEPage        *self,
                      NMConnection  *connection,
                      GError       **error)
{
        if (!ui_to_setting (CE_PAGE_IP4 (self)))
                return FALSE;

        return nm_setting_verify (NM_SETTING (CE_PAGE_IP4 (self)->setting), NULL, error);
}

static void
ce_page_ip4_init (CEPageIP4 *self)
{
        const GActionEntry ip4_entries[] = {
                { "ip4method", on_ip4_method_activated_cb, "s", "'automatic'", NULL, { 0 } },
        };
        self->method_group = G_ACTION_GROUP (g_simple_action_group_new ());

        g_action_map_add_action_entries (G_ACTION_MAP (self->method_group), ip4_entries, G_N_ELEMENTS (ip4_entries), self);
        gtk_widget_insert_action_group (GTK_WIDGET (self), "ip4page", G_ACTION_GROUP (self->method_group));

        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
ce_page_ip4_class_init (CEPageIP4Class *klass)
{
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/ip4-page.ui");

        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, address_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, address_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, auto_dns_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, auto_dns_switch);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, auto_routes_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, auto_routes_switch);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, content_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, dns_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, dns_entry);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, main_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, never_default_check);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, address_address_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, address_netmask_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, address_gateway_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, route_config_box);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_address_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_netmask_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_gateway_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_metric_label);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_address_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_netmask_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_gateway_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_metric_sizegroup);
        gtk_widget_class_bind_template_child (widget_class, CEPageIP4, routes_sizegroup);
}

static void
ce_page_iface_init (CEPageInterface *iface)
{
        iface->get_title = ce_page_ip4_get_title;
        iface->validate = ce_page_ip4_validate;
}

CEPageIP4 *
ce_page_ip4_new (NMConnection     *connection,
                 NMClient         *client)
{
        CEPageIP4 *self;

        self = CE_PAGE_IP4 (g_object_new (ce_page_ip4_get_type (), NULL));

        self->setting = nm_connection_get_setting_ip4_config (connection);
        if (!self->setting) {
                self->setting = NM_SETTING_IP_CONFIG (nm_setting_ip4_config_new ());
                nm_connection_add_setting (connection, NM_SETTING (self->setting));
        }

        connect_ip4_page (self);

        return self;
}
