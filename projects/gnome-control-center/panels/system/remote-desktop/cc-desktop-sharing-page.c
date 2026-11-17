/*
 * Copyright 2024 Red Hat Inc
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-desktop-sharing-page"

#include "cc-desktop-sharing-page.h"
#include "cc-encryption-fingerprint-dialog.h"
#include "cc-gnome-remote-desktop.h"
#include "cc-hostname.h"
#include "cc-password-utils.h"
#include "cc-list-row.h"
#include "cc-tls-certificate.h"
#include "cc-systemd-service.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <locale.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif

#include <pwd.h>
#include <pwquality.h>
#include <unistd.h>

#include "org.gnome.RemoteDesktop.h"

#define GNOME_REMOTE_DESKTOP_SCHEMA_ID "org.gnome.desktop.remote-desktop"
#define GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID "org.gnome.desktop.remote-desktop.rdp"
#define REMOTE_DESKTOP_STORE_CREDENTIALS_TIMEOUT_S 1
#define REMOTE_DESKTOP_SERVICE "gnome-remote-desktop.service"
#define RDP_SERVER_DBUS_SERVICE "org.gnome.RemoteDesktop.User"
#define RDP_SERVER_OBJECT_PATH "/org/gnome/RemoteDesktop/Rdp/Server"

struct _CcDesktopSharingPage {
  AdwBin parent_instance;

  GtkWidget *toast_overlay;

  GsdRemoteDesktopRdpServer *rdp_server;

  AdwSwitchRow *desktop_sharing_row;
  AdwSwitchRow *remote_control_row;
  AdwActionRow *hostname_row;
  AdwActionRow *port_row;
  GtkWidget *username_entry;
  GtkWidget *password_entry;
  GtkWidget *generate_password_button;
  GtkWidget *verify_encryption_button;

  guint desktop_sharing_name_watch;
  guint store_credentials_id;
  GTlsCertificate *certificate;

  GSettings *rdp_settings;
  GCancellable *cancellable;
};

G_DEFINE_TYPE (CcDesktopSharingPage, cc_desktop_sharing_page, ADW_TYPE_BIN)

static void
on_generate_password_button_clicked (CcDesktopSharingPage *self)
{
  g_autofree char *new_password = cc_generate_password ();

  gtk_editable_set_text (GTK_EDITABLE (self->password_entry), new_password);
}

static void
on_verify_encryption_button_clicked (CcDesktopSharingPage *self)
{
  CcEncryptionFingerprintDialog *dialog;
  GtkNative *native;

  g_return_if_fail (self->certificate);

  dialog = g_object_new (CC_TYPE_ENCRYPTION_FINGERPRINT_DIALOG, NULL);
  cc_encryption_fingerprint_dialog_set_certificate (dialog, self->certificate);

  native = gtk_widget_get_native (GTK_WIDGET (self));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (native));
  gtk_window_present (GTK_WINDOW (dialog));
}

static char *
get_hostname (void)
{
  return cc_hostname_get_static_hostname (cc_hostname_get_default ());
}

static gboolean
check_schema_available (CcDesktopSharingPage *self,
                        const gchar *schema_id)
{
  GSettingsSchemaSource *source;
  g_autoptr(GSettingsSchema) schema = NULL;

  source = g_settings_schema_source_get_default ();
  if (!source)
    return FALSE;

  schema = g_settings_schema_source_lookup (source, schema_id, TRUE);
  if (!schema)
    return FALSE;

  return TRUE;
}

static gboolean
store_credentials_timeout (gpointer user_data)
{
  CcDesktopSharingPage *self = (CcDesktopSharingPage*) user_data;
  const char *username, *password;

  username = gtk_editable_get_text (GTK_EDITABLE (self->username_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->password_entry));

  if (username && password)
    {
      cc_grd_store_rdp_credentials (username, password, self->cancellable);
    }

  self->store_credentials_id = 0;

  return G_SOURCE_REMOVE;
}

static gboolean
is_desktop_sharing_enabled (CcDesktopSharingPage *self)
{
  if (!g_settings_get_boolean (self->rdp_settings, "enable"))
    return FALSE;

  return cc_is_service_active (REMOTE_DESKTOP_SERVICE, G_BUS_TYPE_SESSION);
}

static void
disable_gnome_desktop_sharing_service (CcDesktopSharingPage *self)
{
  g_autoptr(GError) error = NULL;

  g_settings_set_boolean (self->rdp_settings, "enable", FALSE);

  if (!cc_disable_service (REMOTE_DESKTOP_SERVICE,
                           G_BUS_TYPE_SESSION,
                           &error))
    g_warning ("Failed to enable remote desktop service: %s", error->message);
}

static void
enable_gnome_desktop_sharing_service (CcDesktopSharingPage *self)
{
  g_autoptr(GError) error = NULL;

  if (is_desktop_sharing_enabled (self))
    return;

  if (!cc_enable_service (REMOTE_DESKTOP_SERVICE, G_BUS_TYPE_SESSION, &error))
    {
      g_warning ("Failed to enable remote desktop service: %s", error->message);
      disable_gnome_desktop_sharing_service (self);
    }
}

static void
on_credentials_changed (CcDesktopSharingPage *self)
{
  g_clear_handle_id (&self->store_credentials_id,
                     g_source_remove);

  self->store_credentials_id =
    g_timeout_add_seconds (REMOTE_DESKTOP_STORE_CREDENTIALS_TIMEOUT_S,
                           store_credentials_timeout,
                           self);
}

static void
calc_default_tls_paths (char **out_dir_path,
                        char **out_cert_path,
                        char **out_key_path)
{
  g_autofree char *dir_path = NULL;

  dir_path = g_build_filename (g_get_user_data_dir(), "gnome-remote-desktop", "certificates", NULL);

  if (out_cert_path)
    *out_cert_path = g_build_filename(dir_path, "rdp-tls.crt", NULL);
  if (out_key_path)
    *out_key_path = g_build_filename(dir_path, "rdp-tls.key", NULL);

  if (out_dir_path)
    *out_dir_path = g_steal_pointer (&dir_path);
}

static void
set_tls_certificate (CcDesktopSharingPage  *self,
                     GTlsCertificate *tls_certificate)
{
  g_set_object (&self->certificate, tls_certificate);
  gtk_widget_set_sensitive (self->verify_encryption_button, TRUE);
}

static void
on_certificate_generated (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  CcDesktopSharingPage *self;
  g_autoptr(GTlsCertificate) tls_certificate = NULL;
  g_autoptr(GError) error = NULL;
  g_autofree char *cert_path = NULL;
  g_autofree char *key_path = NULL;

  tls_certificate = bonsai_tls_certificate_new_generate_finish (res, &error);
  if (!tls_certificate)
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        return;

      g_warning ("Failed to generate TLS certificate: %s", error->message);
      return;
    }

  self = (CcDesktopSharingPage *)user_data;

  calc_default_tls_paths (NULL, &cert_path, &key_path);

  g_settings_set_string (self->rdp_settings, "tls-cert", cert_path);
  g_settings_set_string (self->rdp_settings, "tls-key", key_path);

  set_tls_certificate (self, tls_certificate);

  enable_gnome_desktop_sharing_service (self);
}

static void
enable_gnome_desktop_sharing (CcDesktopSharingPage *self)
{
  g_autofree char *dir_path = NULL;
  g_autofree char *cert_path = NULL;
  g_autofree char *key_path = NULL;
  g_autoptr(GFile) dir = NULL;
  g_autoptr(GFile) cert_file = NULL;
  g_autoptr(GFile) key_file = NULL;
  g_autoptr(GError) error = NULL;

  g_settings_set_boolean (self->rdp_settings, "enable", TRUE);

  cert_path = g_settings_get_string (self->rdp_settings, "tls-cert");
  key_path = g_settings_get_string (self->rdp_settings, "tls-key");
  if (strlen (cert_path) > 0 &&
      strlen (key_path) > 0)
    {
      g_autoptr(GTlsCertificate) tls_certificate = NULL;

      tls_certificate = g_tls_certificate_new_from_file (cert_path, &error);
      if (tls_certificate)
        {
          set_tls_certificate (self, tls_certificate);

          enable_gnome_desktop_sharing_service (self);
          return;
        }

      g_warning ("Configured TLS certificate invalid: %s", error->message);
      return;
    }

  calc_default_tls_paths (&dir_path, &cert_path, &key_path);

  dir = g_file_new_for_path (dir_path);
  if (!g_file_query_exists (dir, NULL))
    {
      if (!g_file_make_directory_with_parents (dir, NULL, &error))
        {
          g_warning ("Failed to create remote desktop certificate directory: %s",
                     error->message);
          return;
        }
    }

  cert_file = g_file_new_for_path (cert_path);
  key_file = g_file_new_for_path (key_path);

  if (g_file_query_exists (cert_file, NULL) &&
      g_file_query_exists (key_file, NULL))
    {
      g_autoptr(GTlsCertificate) tls_certificate = NULL;

      tls_certificate = g_tls_certificate_new_from_file (cert_path, &error);
      if (tls_certificate)
        {
          g_settings_set_string (self->rdp_settings, "tls-cert", cert_path);
          g_settings_set_string (self->rdp_settings, "tls-key", key_path);

          set_tls_certificate (self, tls_certificate);

          enable_gnome_desktop_sharing_service (self);
          return;
        }

      g_warning ("Existing TLS certificate invalid: %s", error->message);
      return;
    }

  bonsai_tls_certificate_new_generate_async (cert_path,
                                             key_path,
                                             "US",
                                             "GNOME",
                                             self->cancellable,
                                             on_certificate_generated,
                                             self);
}

static void
on_desktop_sharing_active_changed (CcDesktopSharingPage *self)
{
  if (adw_switch_row_get_active (self->desktop_sharing_row))
    enable_gnome_desktop_sharing (self);
  else
    disable_gnome_desktop_sharing_service (self);
}

static void
add_toast (CcDesktopSharingPage *self,
           const char          *message)
{
  adw_toast_overlay_add_toast (ADW_TOAST_OVERLAY (self->toast_overlay),
                               adw_toast_new (message));
}

static void
on_address_copy_clicked (CcDesktopSharingPage *self,
                         GtkButton           *button)
{
  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          adw_action_row_get_subtitle (self->hostname_row));
  add_toast (self, _("Device address copied"));
}

static void
on_port_copy_clicked (CcDesktopSharingPage *self,
                      GtkButton            *button)
{
  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          adw_action_row_get_subtitle (self->port_row));
  add_toast (self, _("Port number copied"));
}

static void
on_username_copy_clicked (CcDesktopSharingPage *self,
                          GtkButton           *button)
{
  GtkEditable *editable = GTK_EDITABLE (self->username_entry);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_editable_get_text (editable));
  add_toast (self, _("Username copied"));
}

static void
on_password_copy_clicked (CcDesktopSharingPage *self,
                          GtkButton           *button)
{
  GtkEditable *editable = GTK_EDITABLE (self->password_entry);

  gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)),
                          gtk_editable_get_text (editable));
  add_toast (self, _("Password copied"));
}

static void
setup_desktop_sharing_page (CcDesktopSharingPage *self)
{
  const gchar *username = NULL;
  const gchar *password = NULL;
  g_autofree char *hostname = NULL;

  self->rdp_settings = g_settings_new (GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID);

  hostname = get_hostname ();
  adw_action_row_set_subtitle (self->hostname_row, hostname);

  username = cc_grd_lookup_rdp_username (self->cancellable);
  password = cc_grd_lookup_rdp_password (self->cancellable);
  if (username != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->username_entry), username);
  if (password != NULL)
    gtk_editable_set_text (GTK_EDITABLE (self->password_entry), password);

  g_signal_connect_swapped (self->username_entry,
                            "notify::text",
                            G_CALLBACK (on_credentials_changed),
                            self);
  g_signal_connect_swapped (self->password_entry,
                            "notify::text",
                            G_CALLBACK (on_credentials_changed),
                            self);
  if (username == NULL)
    {
      struct passwd *pw = getpwuid (getuid ());
      if (pw != NULL)
        username = g_strdup (pw->pw_name);
      else
        g_warning ("Failed to get username: %s", g_strerror (errno));
    }
  gtk_editable_set_text (GTK_EDITABLE (self->username_entry), username);

  if (password == NULL)
    {
      g_autofree gchar *pw = cc_generate_password ();
      if (pw != NULL)
        gtk_editable_set_text (GTK_EDITABLE (self->password_entry), pw);
    }

  g_signal_connect_object (self->desktop_sharing_row, "notify::active",
                           G_CALLBACK (on_desktop_sharing_active_changed), self,
                           G_CONNECT_SWAPPED);

  adw_switch_row_set_active (self->desktop_sharing_row, is_desktop_sharing_enabled (self));

  g_settings_bind (self->rdp_settings,
                   "enable",
                   self->desktop_sharing_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->rdp_settings,
                   "view-only",
                   self->remote_control_row,
                   "active",
                   G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);
  g_object_bind_property (self->desktop_sharing_row, "active",
                          self->remote_control_row, "sensitive",
                          G_BINDING_SYNC_CREATE);
  g_object_bind_property (self->password_entry, "sensitive",
		          self->generate_password_button, "sensitive",
			  G_BINDING_SYNC_CREATE);
}

static void
desktop_sharing_name_appeared (GDBusConnection *connection,
                               const gchar     *name,
                               const gchar     *name_owner,
                               gpointer         user_data)
{
  CcDesktopSharingPage *self = (CcDesktopSharingPage *)user_data;

  g_clear_handle_id (&self->desktop_sharing_name_watch, g_bus_unwatch_name);

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);

  setup_desktop_sharing_page (self);
}

static void
check_desktop_sharing_available (CcDesktopSharingPage *self)
{
  if (!check_schema_available (self, GNOME_REMOTE_DESKTOP_SCHEMA_ID) ||
      !check_schema_available (self, GNOME_REMOTE_DESKTOP_RDP_SCHEMA_ID))
    {
      gtk_widget_set_visible (GTK_WIDGET (self), FALSE);
      return;
    }

  self->desktop_sharing_name_watch = g_bus_watch_name (G_BUS_TYPE_SESSION,
                                                      "org.gnome.Mutter.RemoteDesktop",
                                                      G_BUS_NAME_WATCHER_FLAGS_NONE,
                                                      desktop_sharing_name_appeared,
                                                      NULL,
                                                      self,
                                                      NULL);
}

static void
cc_desktop_sharing_page_dispose (GObject *object)
{
  CcDesktopSharingPage *self = (CcDesktopSharingPage *)object;

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  g_clear_handle_id (&self->store_credentials_id, g_source_remove);

  g_clear_object (&self->rdp_server);
  g_clear_object (&self->rdp_settings);

  G_OBJECT_CLASS (cc_desktop_sharing_page_parent_class)->dispose (object);
}

static void
cc_desktop_sharing_page_class_init (CcDesktopSharingPageClass * klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = cc_desktop_sharing_page_dispose;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/remote-desktop/cc-desktop-sharing-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, toast_overlay);

  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, desktop_sharing_row);
  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, remote_control_row);
  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, hostname_row);
  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, port_row);
  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, username_entry);
  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, generate_password_button);
  gtk_widget_class_bind_template_child (widget_class, CcDesktopSharingPage, verify_encryption_button);

  gtk_widget_class_bind_template_callback (widget_class, on_address_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_port_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_username_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_password_copy_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_generate_password_button_clicked);
  gtk_widget_class_bind_template_callback (widget_class, on_verify_encryption_button_clicked);
}

static gboolean
format_port_for_row (GBinding     *binding,
                     const GValue *from_value,
                     GValue       *to_value,
                     gpointer      user_data)
{
  int port = g_value_get_int (from_value);

  if (port <= 0)
    g_value_set_string (to_value, " ");
  else
    g_value_take_string (to_value, g_strdup_printf ("%u", port));

  return TRUE;
}

static gboolean
sensitize_row_from_port (GBinding     *binding,
                         const GValue *from_value,
                         GValue       *to_value,
                         gpointer      user_data)
{
  int port = g_value_get_int (from_value);

  g_value_set_boolean (to_value, port > 0);

  return TRUE;
}

static void
on_connected_to_remote_desktop_rdp_server (GObject      *source_object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
  CcDesktopSharingPage *self = user_data;
  g_autoptr (GError) error = NULL;

  g_clear_object (&self->rdp_server);
  self->rdp_server = gsd_remote_desktop_rdp_server_proxy_new_finish (result, &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Failed to create remote desktop proxy: %s", error->message);
      return;
    }

  g_object_bind_property_full (self->rdp_server, "port",
                               self->port_row, "subtitle",
                               G_BINDING_SYNC_CREATE,
                               format_port_for_row,
                               NULL,
                               NULL,
                               NULL);
  g_object_bind_property_full (self->rdp_server, "port",
                               self->port_row, "sensitive",
                               G_BINDING_SYNC_CREATE,
                               sensitize_row_from_port,
                               NULL,
                               NULL,
                               NULL);
}

static void
connect_to_remote_desktop_rdp_server (CcDesktopSharingPage *self)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GDBusConnection) connection = NULL;

  connection = g_bus_get_sync (G_BUS_TYPE_SESSION, self->cancellable, &error);

  if (error)
    g_warning ("Could not connect to system message bus: %s", error->message);

  if (!connection)
    return;

  gsd_remote_desktop_rdp_server_proxy_new (connection,
                                           G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                                           RDP_SERVER_DBUS_SERVICE,
                                           RDP_SERVER_OBJECT_PATH,
                                           self->cancellable,
                                           (GAsyncReadyCallback)
                                           on_connected_to_remote_desktop_rdp_server,
                                           self);
}

static void
cc_desktop_sharing_page_init (CcDesktopSharingPage *self)
{
  g_autoptr(GtkCssProvider) provider = NULL;

  gtk_widget_init_template (GTK_WIDGET (self));

  self->cancellable = g_cancellable_new ();
  check_desktop_sharing_available (self);
  connect_to_remote_desktop_rdp_server (self);

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
                                       "/org/gnome/control-center/system/remote-desktop/remote-desktop.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}
