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

#include <glib-object.h>
#include <glib/gi18n.h>

#include <NetworkManager.h>

#include "net-connection-editor.h"
#include "net-connection-editor-resources.h"
#include "ce-page.h"
#include "ce-page-details.h"
#include "ce-page-wifi.h"
#include "ce-page-ip4.h"
#include "ce-page-ip6.h"
#include "ce-page-security.h"
#include "ce-page-ethernet.h"
#include "ce-page-bluetooth.h"
#include "ce-page-8021x-security.h"
#include "ce-page-vpn.h"
#include "ce-page-wireguard.h"
#include "vpn-helpers.h"

enum {
        DONE,
        LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _NetConnectionEditor
{
        AdwWindow parent;

        GtkBox           *add_connection_box;
        AdwBin           *add_connection_frame;
        GtkButton        *apply_button;
        GtkButton        *cancel_button;
        GtkNotebook      *notebook;
        AdwToastOverlay  *toast_overlay;
        GtkStack         *toplevel_stack;

        NMClient         *client;
        NMDevice         *device;

        NMConnection     *connection;
        NMConnection     *orig_connection;
        gboolean          is_new_connection;
        gboolean          is_changed;
        NMAccessPoint    *ap;
        GCancellable     *cancellable;

        GSList *initializing_pages;

        NMClientPermissionResult can_modify;

        gboolean          title_set;
};

G_DEFINE_TYPE (NetConnectionEditor, net_connection_editor, ADW_TYPE_WINDOW)

/* Used as both GSettings keys and GObject data tags */
#define IGNORE_CA_CERT_TAG "ignore-ca-cert"
#define IGNORE_PHASE2_CA_CERT_TAG "ignore-phase2-ca-cert"

static GSettings *
_get_ca_ignore_settings (NMConnection *connection)
{
        GSettings *settings;
        g_autofree gchar *path = NULL;
        const char *uuid;

        g_return_val_if_fail (connection, NULL);

        uuid = nm_connection_get_uuid (connection);
        g_return_val_if_fail (uuid && *uuid, NULL);

        path = g_strdup_printf ("/org/gnome/nm-applet/eap/%s/", uuid);
        settings = g_settings_new_with_path ("org.gnome.nm-applet.eap", path);

        return settings;
}

/**
 * eap_method_ca_cert_ignore_save:
 * @connection: the connection for which to save CA cert ignore values to GSettings
 *
 * Reads the CA cert ignore tags from the 802.1x setting GObject data and saves
 * then to GSettings if present, using the connection UUID as the index.
 */
static void
eap_method_ca_cert_ignore_save (NMConnection *connection)
{
        NMSetting8021x *s_8021x;
        g_autoptr(GSettings) settings = NULL;
        gboolean ignore = FALSE, phase2_ignore = FALSE;

        g_return_if_fail (connection);

        s_8021x = nm_connection_get_setting_802_1x (connection);
        if (s_8021x) {
                ignore = !!g_object_get_data (G_OBJECT (s_8021x), IGNORE_CA_CERT_TAG);
                phase2_ignore = !!g_object_get_data (G_OBJECT (s_8021x), IGNORE_PHASE2_CA_CERT_TAG);
        }

        settings = _get_ca_ignore_settings (connection);
        if (!settings)
                return;

        g_settings_set_boolean (settings, IGNORE_CA_CERT_TAG, ignore);
        g_settings_set_boolean (settings, IGNORE_PHASE2_CA_CERT_TAG, phase2_ignore);
}

/**
 * eap_method_ca_cert_ignore_load:
 * @connection: the connection for which to load CA cert ignore values to GSettings
 *
 * Reads the CA cert ignore tags from the 802.1x setting GObject data and saves
 * then to GSettings if present, using the connection UUID as the index.
 */
static void
eap_method_ca_cert_ignore_load (NMConnection *connection)
{
        g_autoptr(GSettings) settings = NULL;
        NMSetting8021x *s_8021x;
        gboolean ignore, phase2_ignore;

        g_return_if_fail (connection);

        s_8021x = nm_connection_get_setting_802_1x (connection);
        if (!s_8021x)
                return;

        settings = _get_ca_ignore_settings (connection);
        if (!settings)
                return;

        ignore = g_settings_get_boolean (settings, IGNORE_CA_CERT_TAG);
        phase2_ignore = g_settings_get_boolean (settings, IGNORE_PHASE2_CA_CERT_TAG);

        g_object_set_data (G_OBJECT (s_8021x),
                           IGNORE_CA_CERT_TAG,
                           GUINT_TO_POINTER (ignore));
        g_object_set_data (G_OBJECT (s_8021x),
                           IGNORE_PHASE2_CA_CERT_TAG,
                           GUINT_TO_POINTER (phase2_ignore));
}

static void page_changed (NetConnectionEditor *self);

static void
cancel_editing (NetConnectionEditor *self)
{
        g_signal_emit (self, signals[DONE], 0, FALSE);
        gtk_window_destroy (GTK_WINDOW (self));
}

static gboolean
net_connection_editor_close_request (GtkWindow *window)
{
        cancel_editing (NET_CONNECTION_EDITOR (window));

        return GTK_WINDOW_CLASS (net_connection_editor_parent_class)->close_request (window);
}

static void
cancel_clicked_cb (NetConnectionEditor *self)
{
        cancel_editing (self);
}

static void
update_connection (NetConnectionEditor *self)
{
        g_autoptr(GVariant) settings = NULL;

        settings = nm_connection_to_dbus (self->connection, NM_CONNECTION_SERIALIZE_ALL);
        nm_connection_replace_settings (self->orig_connection, settings, NULL);
}

static void
update_complete (NetConnectionEditor *self,
                 gboolean             success)
{
        g_signal_emit (self, signals[DONE], 0, success);
        gtk_window_destroy (GTK_WINDOW (self));
}

static void
device_reapply_cb (GObject      *source_object,
                   GAsyncResult *res,
                   gpointer      user_data)
{
        NetConnectionEditor *self = user_data;
        g_autoptr(GError) error = NULL;
        gboolean success = TRUE;

        if (!nm_device_reapply_finish (NM_DEVICE (source_object), res, &error)) {
                if (!g_error_matches (error, NM_DEVICE_ERROR, NM_DEVICE_ERROR_NOT_ACTIVE))
                        g_warning ("Failed to reapply changes on device: %s", error->message);
                success = FALSE;
        }

        update_complete (self, success);
        g_object_unref (self);
}

static void
updated_connection_cb (GObject            *source_object,
                       GAsyncResult       *res,
                       gpointer            user_data)
{
        NetConnectionEditor *self = user_data;
        g_autoptr(GError) error = NULL;

        if (!nm_remote_connection_commit_changes_finish (NM_REMOTE_CONNECTION (source_object),
                                                         res, &error)) {
                g_warning ("Failed to commit changes: %s", error->message);
                update_complete (self, FALSE);
                g_object_unref (self);
                return;
        }

        nm_connection_clear_secrets (NM_CONNECTION (source_object));

        if (!self->device) {
                update_complete (self, TRUE);
                g_object_unref (self);
                return;
        }

        nm_device_reapply_async (self->device, NM_CONNECTION (self->orig_connection),
                                 0, 0, NULL, device_reapply_cb, self /* owned */);
}

static void
added_connection_cb (GObject            *source_object,
                     GAsyncResult       *res,
                     gpointer            user_data)
{
        NetConnectionEditor *self = user_data;
        g_autoptr(GError) error = NULL;

        if (!nm_client_add_connection_finish (NM_CLIENT (source_object), res, &error)) {
                g_warning ("Failed to add connection: %s", error->message);
                update_complete (self, FALSE);
                g_object_unref (self);
                return;
        }

        if (!self->device) {
                update_complete (self, TRUE);
                g_object_unref (self);
                return;
        }

        nm_device_reapply_async (self->device, NM_CONNECTION (self->orig_connection),
                                 0, 0, NULL, device_reapply_cb, self /* owned */);
}

static void
apply_clicked_cb (NetConnectionEditor *self)
{
        update_connection (self);

        eap_method_ca_cert_ignore_save (self->connection);

        if (self->is_new_connection) {
                nm_client_add_connection_async (self->client,
                                                self->orig_connection,
                                                TRUE,
                                                NULL,
                                                added_connection_cb,
                                                g_object_ref (self));
        } else {
                nm_remote_connection_commit_changes_async (NM_REMOTE_CONNECTION (self->orig_connection),
                                                           TRUE,
                                                           NULL,
                                                           updated_connection_cb,
                                                           g_object_ref (self));
        }

        gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
}

static void
net_connection_editor_init (NetConnectionEditor *self)
{
        gtk_widget_init_template (GTK_WIDGET (self));
}

static void
net_connection_editor_finalize (GObject *object)
{
        NetConnectionEditor *self = NET_CONNECTION_EDITOR (object);

        g_clear_object (&self->connection);
        g_clear_object (&self->orig_connection);
        g_clear_object (&self->device);
        g_clear_object (&self->client);
        g_clear_object (&self->ap);
        g_cancellable_cancel (self->cancellable);
        g_clear_object (&self->cancellable);

        G_OBJECT_CLASS (net_connection_editor_parent_class)->finalize (object);
}

static void
net_connection_editor_class_init (NetConnectionEditorClass *class)
{
        GObjectClass *object_class = G_OBJECT_CLASS (class);
        GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);
        GtkWindowClass *window_class = GTK_WINDOW_CLASS (class);

        g_resources_register (net_connection_editor_get_resource ());

        object_class->finalize = net_connection_editor_finalize;

        window_class->close_request = net_connection_editor_close_request;

        signals[DONE] = g_signal_new ("done",
                                      G_OBJECT_CLASS_TYPE (object_class),
                                      G_SIGNAL_RUN_FIRST,
                                      0,
                                      NULL, NULL,
                                      NULL,
                                      G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

        gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

        gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/network/connection-editor.ui");

        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, add_connection_box);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, add_connection_frame);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, apply_button);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, cancel_button);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, notebook);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, toast_overlay);
        gtk_widget_class_bind_template_child (widget_class, NetConnectionEditor, toplevel_stack);

        gtk_widget_class_bind_template_callback (widget_class, cancel_clicked_cb);
        gtk_widget_class_bind_template_callback (widget_class, apply_clicked_cb);
}

static void
nm_connection_editor_watch_cb (GPid pid,
                               gint status,
                               gpointer user_data)
{
        g_debug ("Child %d" G_PID_FORMAT " exited %s", pid,
                 g_spawn_check_wait_status (status, NULL) ? "normally" : "abnormally");

        g_spawn_close_pid (pid);
        /* Close the dialog when nm-connection-editor exits. */
        gtk_window_destroy (GTK_WINDOW (user_data));
}

static void
net_connection_editor_do_fallback (NetConnectionEditor *self, const gchar *type)
{
        g_autoptr(GError) error = NULL;
        g_autoptr(GStrvBuilder) builder = NULL;
        g_auto(GStrv) argv = NULL;
        GPid child_pid;

        builder = g_strv_builder_new ();
        g_strv_builder_add (builder, "nm-connection-editor");

        if (self->is_new_connection) {
                g_autofree gchar *type_str = NULL;

                type_str = g_strdup_printf ("--type=%s", type);
                g_strv_builder_add (builder, type_str);
                g_strv_builder_add (builder, "--create");
        } else {
                g_autofree gchar *edit_str = NULL;

                edit_str = g_strdup_printf ("--edit=%s", nm_connection_get_uuid (self->connection));
                g_strv_builder_add (builder, edit_str);
        }

        g_strv_builder_add (builder, NULL);
        argv = g_strv_builder_end (builder);

        g_spawn_async_with_pipes (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                                  NULL, NULL, &child_pid, NULL, NULL, NULL, &error);

        if (error) {
                AdwToast *toast;
                g_autofree gchar *message = NULL;

                message = g_strdup_printf (_("Unable to open connection editor: %s"), error->message);
                toast = adw_toast_new (message);

                adw_toast_overlay_add_toast (self->toast_overlay, toast);
        } else {
                g_child_watch_add (child_pid, nm_connection_editor_watch_cb, self);
        }

        g_signal_emit (self, signals[DONE], 0, FALSE);
}

static void
net_connection_editor_update_title (NetConnectionEditor *self)
{
        g_autofree gchar *id = NULL;

        if (self->title_set)
                return;

        if (self->is_new_connection) {
                if (self->device) {
                        id = g_strdup (_("New Profile"));
                } else {
                        /* Leave it set to "Add New Connection" */
                        return;
                }
        } else {
                NMSettingWireless *sw;
                sw = nm_connection_get_setting_wireless (self->connection);
                if (sw) {
                        GBytes *ssid;
                        ssid = nm_setting_wireless_get_ssid (sw);
                        id = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));
                } else {
                        id = g_strdup (nm_connection_get_id (self->connection));
                }
        }
        gtk_window_set_title (GTK_WINDOW (self), id);
}

static gboolean
editor_is_initialized (NetConnectionEditor *self)
{
        return self->initializing_pages == NULL;
}

static void
update_sensitivity (NetConnectionEditor *self)
{
        NMSettingConnection *sc;
        gboolean sensitive;
        gint i;

        if (!editor_is_initialized (self))
                return;

        sc = nm_connection_get_setting_connection (self->connection);

        if (nm_setting_connection_get_read_only (sc)) {
                sensitive = FALSE;
        } else {
                sensitive = self->can_modify;
        }

        for (i = 0; i < gtk_notebook_get_n_pages (self->notebook); i++) {
                GtkWidget *page = gtk_notebook_get_nth_page (self->notebook, i);
                gtk_widget_set_sensitive (page, sensitive);
        }
}

static void
validate (NetConnectionEditor *self)
{
        gboolean valid = FALSE;
        g_autofree gchar *apply_tooltip = NULL;
        gint i;

        if (!editor_is_initialized (self))
                goto done;

        valid = TRUE;
        for (i = 0; i < gtk_notebook_get_n_pages (self->notebook); i++) {
                CEPage *page = CE_PAGE (gtk_notebook_get_nth_page (self->notebook, i));
                g_autoptr(GError) error = NULL;

                if (!ce_page_validate (page, self->connection, &error)) {
                        valid = FALSE;
                        if (error) {
                                apply_tooltip = g_strdup_printf (_("Invalid setting %s: %s"), ce_page_get_title (page), error->message);
                                g_debug ("%s", apply_tooltip);
                        } else {
                                apply_tooltip = g_strdup_printf (_("Invalid setting %s"), ce_page_get_title (page));
                                g_debug ("%s", apply_tooltip);
                        }
                }
        }

        update_sensitivity (self);
done:
        if (apply_tooltip != NULL)
                gtk_widget_set_tooltip_text(GTK_WIDGET (self->apply_button), apply_tooltip);

        gtk_widget_set_sensitive (GTK_WIDGET (self->apply_button), valid && self->is_changed);
}

static void
page_changed (NetConnectionEditor *self)
{
        if (editor_is_initialized (self))
                self->is_changed = TRUE;
        validate (self);
}

static gboolean
idle_validate (gpointer user_data)
{
        validate (NET_CONNECTION_EDITOR (user_data));

        return G_SOURCE_REMOVE;
}

static void
recheck_initialization (NetConnectionEditor *self)
{
        if (!editor_is_initialized (self))
                return;

        gtk_stack_set_visible_child (self->toplevel_stack, GTK_WIDGET (self->notebook));
        gtk_notebook_set_current_page (self->notebook, 0);

        g_idle_add (idle_validate, self);

        if (self->is_new_connection)
                adw_bin_set_child (self->add_connection_frame, NULL);

}

static void
page_initialized (NetConnectionEditor *self, GError *error, CEPage *page)
{
        GtkWidget *label;
        gint position;
        gint i;

        position = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (page), "position"));
        g_object_set_data (G_OBJECT (page), "position", GINT_TO_POINTER (position));
        for (i = 0; i < gtk_notebook_get_n_pages (self->notebook); i++) {
                GtkWidget *page = gtk_notebook_get_nth_page (self->notebook, i);
                gint pos = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (page), "position"));
                if (pos > position)
                        break;
        }

        label = gtk_label_new (ce_page_get_title (page));

        gtk_notebook_insert_page (self->notebook, GTK_WIDGET (page), label, i);

        self->initializing_pages = g_slist_remove (self->initializing_pages, page);

        recheck_initialization (self);
}

typedef struct {
        NetConnectionEditor *editor;
        CEPage *page;
        const gchar *setting_name;
} GetSecretsInfo;

static void
get_secrets_cb (GObject *source_object,
                GAsyncResult *res,
                gpointer user_data)
{
        NMRemoteConnection *connection;
        g_autofree GetSecretsInfo *info = user_data;
        g_autoptr(GError) error = NULL;
        g_autoptr(GVariant) variant = NULL;

        connection = NM_REMOTE_CONNECTION (source_object);
        variant = nm_remote_connection_get_secrets_finish (connection, res, &error);

        if (!variant) {
                if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        return;

                g_warning ("Failed to get secrets: %s", error->message);
        }

        ce_page_complete_init (info->page, info->editor->connection, info->setting_name, variant, g_steal_pointer (&error));
}

static void
get_secrets_for_page (NetConnectionEditor *self,
                      CEPage              *page,
                      const gchar         *setting_name)
{
        GetSecretsInfo *info;

        info = g_new0 (GetSecretsInfo, 1);
        info->editor = self;
        info->page = page;
        info->setting_name = setting_name;

        nm_remote_connection_get_secrets_async (NM_REMOTE_CONNECTION (self->orig_connection),
                                                setting_name,
                                                self->cancellable,
                                                get_secrets_cb,
                                                info);
}

static void
add_page (NetConnectionEditor *self, CEPage *page)
{
        gint position;

        position = g_slist_length (self->initializing_pages);
        g_object_set_data (G_OBJECT (page), "position", GINT_TO_POINTER (position));

        self->initializing_pages = g_slist_append (self->initializing_pages, page);

        g_signal_connect_object (page, "changed", G_CALLBACK (page_changed), self, G_CONNECT_SWAPPED);
        g_signal_connect_object (page, "initialized", G_CALLBACK (page_initialized), self, G_CONNECT_SWAPPED);
}

static void
net_connection_editor_set_connection (NetConnectionEditor *self,
                                      NMConnection        *connection)
{
        GSList *pages, *l;
        NMSettingConnection *sc;
        const gchar *type;
        gboolean is_wired;
        gboolean is_wifi;
        gboolean is_vpn;
        gboolean is_wireguard;
        gboolean is_bluetooth;

        self->is_new_connection = !nm_client_get_connection_by_uuid (self->client,
                                                                       nm_connection_get_uuid (connection));

        if (self->is_new_connection) {
                gtk_button_set_label (self->apply_button, _("_Add"));
                self->is_changed = TRUE;
        }

        self->connection = nm_simple_connection_new_clone (connection);
        self->orig_connection = g_object_ref (connection);

        net_connection_editor_update_title (self);

        eap_method_ca_cert_ignore_load (self->connection);

        sc = nm_connection_get_setting_connection (connection);
        type = nm_setting_connection_get_connection_type (sc);

        is_wired = g_str_equal (type, NM_SETTING_WIRED_SETTING_NAME);
        is_wifi = g_str_equal (type, NM_SETTING_WIRELESS_SETTING_NAME);
        is_vpn = g_str_equal (type, NM_SETTING_VPN_SETTING_NAME);
        is_wireguard = g_str_equal (type, NM_SETTING_WIREGUARD_SETTING_NAME);
        is_bluetooth = g_str_equal (type, NM_SETTING_BLUETOOTH_SETTING_NAME);

        add_page (self, CE_PAGE (ce_page_details_new (self->connection, self->device, self->ap, self, self->is_new_connection)));

        if (is_wifi)
                add_page (self, CE_PAGE (ce_page_wifi_new (self->connection, self->client)));
        else if (is_wired)
                add_page (self, CE_PAGE (ce_page_ethernet_new (self->connection, self->client)));
        else if (is_vpn)
                add_page (self, CE_PAGE (ce_page_vpn_new (self->connection)));
        else if (is_wireguard)
                add_page (self, CE_PAGE (ce_page_wireguard_new (self->connection)));
        else if (is_bluetooth)
                add_page (self, CE_PAGE (ce_page_bluetooth_new (self->connection)));
        else {
                /* Unsupported type */
                net_connection_editor_do_fallback (self, type);
                return;
        }

        add_page (self, CE_PAGE (ce_page_ip4_new (self->connection, self->client)));
        add_page (self, CE_PAGE (ce_page_ip6_new (self->connection, self->client)));

        if (is_wifi)
                add_page (self, CE_PAGE (ce_page_security_new (self->connection)));
        else if (is_wired)
                add_page (self, CE_PAGE (ce_page_8021x_security_new (self->connection)));

        pages = g_slist_copy (self->initializing_pages);
        for (l = pages; l; l = l->next) {
                CEPage *page = l->data;
                const gchar *security_setting;

                security_setting = ce_page_get_security_setting (page);
                if (!security_setting || self->is_new_connection) {
                        ce_page_complete_init (page, NULL, NULL, NULL, NULL);
                } else {
                        get_secrets_for_page (self, page, security_setting);
                }
        }
        g_slist_free (pages);
}

static NMConnection *
complete_vpn_connection (NetConnectionEditor *self,
                         NMConnection *connection,
                         GType setting_type)
{
        NMSettingConnection *s_con;
        NMSetting *s_type;

        if (!connection)
                connection = nm_simple_connection_new ();

        s_con = nm_connection_get_setting_connection (connection);
        if (!s_con) {
                s_con = NM_SETTING_CONNECTION (nm_setting_connection_new ());
                nm_connection_add_setting (connection, NM_SETTING (s_con));
        }

        if (!nm_setting_connection_get_uuid (s_con)) {
                g_autofree gchar *uuid = nm_utils_uuid_generate ();
                g_object_set (s_con,
                              NM_SETTING_CONNECTION_UUID, uuid,
                              NULL);
        }

        if (!nm_setting_connection_get_id (s_con)) {
                const GPtrArray *connections;
                g_autofree gchar *id = NULL;

                connections = nm_client_get_connections (self->client);
                id = ce_page_get_next_available_name (connections, NAME_FORMAT_TYPE, _("VPN"));
                g_object_set (s_con,
                              NM_SETTING_CONNECTION_ID, id,
                              NULL);
        }

        s_type = nm_connection_get_setting (connection, setting_type);
        if (!s_type) {
                s_type = g_object_new (setting_type, NULL);
                nm_connection_add_setting (connection, s_type);
        }

        if (!nm_setting_connection_get_connection_type (s_con)) {
                g_object_set (s_con,
                              NM_SETTING_CONNECTION_TYPE, nm_setting_get_name (s_type),
                              NULL);
        }

        return connection;
}

static void
finish_add_connection (NetConnectionEditor *self, NMConnection *connection)
{
        gtk_widget_set_visible (GTK_WIDGET (self->apply_button), TRUE);

        if (connection)
                net_connection_editor_set_connection (self, connection);
}

static void
vpn_import_complete (NMConnection *connection, gpointer user_data)
{
        NetConnectionEditor *self = user_data;
        NMSetting *s_type = NULL;
        NMSettingConnection *s_con;

        if (!connection) {
                AdwToast *toast;
                g_autofree gchar *message = NULL;

                message = g_strdup_printf (_("Invalid VPN configuration file"));
                toast = adw_toast_new (message);
                adw_toast_overlay_add_toast (self->toast_overlay, toast);

                g_signal_emit (self, signals[DONE], 0, FALSE);
                return;
        }

        s_type = nm_connection_get_setting (connection, NM_TYPE_SETTING_WIREGUARD);
        if (s_type)
                complete_vpn_connection (self, connection, NM_TYPE_SETTING_WIREGUARD);
        else
                complete_vpn_connection (self, connection, NM_TYPE_SETTING_VPN);

        /* Mark the connection as private to this user, and non-autoconnect */
        s_con = nm_connection_get_setting_connection (connection);
        g_object_set (s_con, NM_SETTING_CONNECTION_AUTOCONNECT, FALSE, NULL);
        nm_setting_connection_add_permission (s_con, "user", g_get_user_name (), NULL);

        finish_add_connection (self, connection);
}

static void
vpn_type_activated (NetConnectionEditor *self, GtkWidget *row)
{
        const char *service_name = g_object_get_data (G_OBJECT (row), "service_name");
        NMConnection *connection;
        NMSettingVpn *s_vpn = NULL;
        NMSettingConnection *s_con;
        GType s_type = NM_TYPE_SETTING_VPN;

        if (!strcmp (service_name, "import")) {
                vpn_import (GTK_WINDOW (self), vpn_import_complete, self);
                return;
        } else if (!strcmp (service_name, "wireguard")) {
                s_type = NM_TYPE_SETTING_WIREGUARD;
        }

        connection = complete_vpn_connection (self, NULL, s_type);
        if (s_type == NM_TYPE_SETTING_VPN) {
                s_vpn = nm_connection_get_setting_vpn (connection);
                g_object_set (s_vpn, NM_SETTING_VPN_SERVICE_TYPE, service_name, NULL);
        }

        /* Mark the connection as private to this user, and non-autoconnect */
        s_con = nm_connection_get_setting_connection (connection);
        g_object_set (s_con, NM_SETTING_CONNECTION_AUTOCONNECT, FALSE, NULL);
        nm_setting_connection_add_permission (s_con, "user", g_get_user_name (), NULL);

        finish_add_connection (self, connection);
}

static void
select_vpn_type (NetConnectionEditor *self, GtkListBox *list)
{
        GSList *vpn_plugins, *iter;
        GtkWidget *row;
        GtkWidget *child;

        /* Get the available VPN types */
        vpn_plugins = vpn_get_plugins ();

        /* Remove the previous menu contents */
        while ((child = gtk_widget_get_first_child (GTK_WIDGET (list))) != NULL)
                gtk_list_box_remove (list, child);

        /* Add the VPN types */
        for (iter = vpn_plugins; iter; iter = iter->next) {
                NMVpnEditorPlugin *plugin = nm_vpn_plugin_info_get_editor_plugin (iter->data);
                g_autofree gchar *name = NULL;
                g_autofree gchar *desc = NULL;
                g_autofree gchar *desc_markup = NULL;
                g_autofree gchar *service_name = NULL;

                g_object_get (plugin,
                              NM_VPN_EDITOR_PLUGIN_NAME, &name,
                              NM_VPN_EDITOR_PLUGIN_DESCRIPTION, &desc,
                              NM_VPN_EDITOR_PLUGIN_SERVICE, &service_name,
                              NULL);
                desc_markup = g_markup_printf_escaped ("<span size='smaller'>%s</span>", desc);

                row = adw_action_row_new ();
                gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
                adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), name);
                adw_action_row_set_subtitle (ADW_ACTION_ROW (row), desc_markup);

                g_object_set_data_full (G_OBJECT (row), "service_name", g_steal_pointer (&service_name), g_free);
                gtk_list_box_append (list, row);
        }

        /*  Translators: VPN add dialog Wireguard description */
        gchar *desc = _("Free and open-source VPN solution designed for ease "
                        "of use, high speed performance and low attack surface.");
        gchar *desc_markup = g_markup_printf_escaped ("<span size='smaller'>%s</span>", desc);

        row = adw_action_row_new ();
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("WireGuard"));
        adw_action_row_set_subtitle (ADW_ACTION_ROW (row), desc_markup);

        g_object_set_data (G_OBJECT (row), "service_name", "wireguard");
        gtk_list_box_append (list, row);

        /* Import */
        row = adw_action_row_new ();
        gtk_list_box_row_set_activatable (GTK_LIST_BOX_ROW (row), TRUE);
        adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), _("Import from file…"));

        g_object_set_data (G_OBJECT (row), "service_name", "import");
        gtk_list_box_append (list, row);

        g_signal_connect_object (list, "row-activated",
                                 G_CALLBACK (vpn_type_activated), self, G_CONNECT_SWAPPED);
}

static void
net_connection_editor_add_connection (NetConnectionEditor *self)
{
        GtkListBox *list;

        list = GTK_LIST_BOX (gtk_list_box_new ());
        gtk_list_box_set_selection_mode (list, GTK_SELECTION_NONE);
        gtk_widget_add_css_class (GTK_WIDGET (list), "boxed-list");

        select_vpn_type (self, list);

        adw_bin_set_child (self->add_connection_frame, GTK_WIDGET (list));

        gtk_stack_set_visible_child (self->toplevel_stack, GTK_WIDGET (self->add_connection_box));
        gtk_widget_set_visible (GTK_WIDGET (self->apply_button), FALSE);
        gtk_window_set_title (GTK_WINDOW (self), _("Add VPN"));
}

static void
permission_changed (NetConnectionEditor      *self,
                    NMClientPermission        permission,
                    NMClientPermissionResult  result)
{
        if (permission != NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM)
                return;

        if (result == NM_CLIENT_PERMISSION_RESULT_YES || result == NM_CLIENT_PERMISSION_RESULT_AUTH)
                self->can_modify = TRUE;
        else
                self->can_modify = FALSE;

        validate (self);
}

NetConnectionEditor *
net_connection_editor_new (NMConnection     *connection,
                           NMDevice         *device,
                           NMAccessPoint    *ap,
                           NMClient         *client)
{
        NetConnectionEditor *self;

        self = g_object_new (net_connection_editor_get_type (), NULL);

        self->cancellable = g_cancellable_new ();

        if (ap)
                self->ap = g_object_ref (ap);
        if (device)
                self->device = g_object_ref (device);
        self->client = g_object_ref (client);

        self->can_modify = nm_client_get_permission_result (client, NM_CLIENT_PERMISSION_SETTINGS_MODIFY_SYSTEM);
        g_signal_connect_object (self->client, "permission-changed",
                                 G_CALLBACK (permission_changed), self, G_CONNECT_SWAPPED);

        if (connection)
                net_connection_editor_set_connection (self, connection);
        else
                net_connection_editor_add_connection (self);

        return self;
}

static void
forgotten_cb (GObject *source_object,
              GAsyncResult *res,
              gpointer user_data)
{
        NMRemoteConnection *connection = NM_REMOTE_CONNECTION (source_object);
        NetConnectionEditor *self = user_data;
        g_autoptr(GError) error = NULL;

        if (!nm_remote_connection_delete_finish (connection, res, &error)) {
                if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
                        g_warning ("Failed to delete connection %s: %s",
                                   nm_connection_get_id (NM_CONNECTION (connection)),
                                   error->message);
                return;
        }

        cancel_editing (self);
}

void
net_connection_editor_forget (NetConnectionEditor *self)
{
        nm_remote_connection_delete_async (NM_REMOTE_CONNECTION (self->orig_connection),
                                           self->cancellable, forgotten_cb, self);
}

void
net_connection_editor_set_title (NetConnectionEditor *self,
                                 const gchar         *title)
{
        gtk_window_set_title (GTK_WINDOW (self), title);
        self->title_set = TRUE;
}
