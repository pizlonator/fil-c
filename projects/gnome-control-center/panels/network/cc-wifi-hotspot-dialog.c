/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wifi-hotspot-dialog.c
 *
 * Copyright 2019 Purism SPC
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
 * Author(s):
 *   Mohammed Sadiq <sadiq@sadiqpk.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#undef G_LOG_DOMAIN
#define G_LOG_DOMAIN "cc-wifi-hotspot-dialog"

#include <config.h>
#include <glib/gi18n.h>
#include <libmm-glib.h>

#include "cc-wifi-hotspot-dialog.h"
#include "cc-network-resources.h"
#include "ui-helpers.h"

/**
 * @short_description: WWAN network type selection dialog
 */

struct _CcWifiHotspotDialog
{
  GtkDialog        parent_instance;

  GtkLabel        *connection_label;
  GtkEntry        *name_entry;
  GtkLabel        *name_error_label;
  GtkEntry        *password_entry;
  GtkLabel        *password_error_label;
  GtkButton       *ok_button;

  GCancellable    *cancellable;

  NMDeviceWifi    *device;
  NMConnection    *connection;
  gchar           *host_name;
  gboolean         wpa_supported; /* WPA/WPA2 supported */
};

G_DEFINE_TYPE (CcWifiHotspotDialog, cc_wifi_hotspot_dialog, GTK_TYPE_DIALOG)

static gchar *
get_random_wpa_key (void)
{
  gchar *key;
  gint i;

  key = g_malloc (10 * sizeof (key));
  for (i = 0; i < 8; i++)
    {
      gint c = 0;
      /* too many non alphanumeric characters are hard to remember for humans */
      while (!g_ascii_isalnum (c))
        c = g_random_int_range (33, 126);

    key[i] = (gchar) c;
  }
  key[i] = '\0';

  return key;
}

static gchar *
get_random_wep_key (void)
{
  const gchar *hexdigits = "0123456789abcdef";
  gchar *key;
  gint i;

  key = g_malloc (12 * sizeof (key));

  /* generate a 10-digit hex WEP key */
  for (i = 0; i < 10; i++)
    {
      gint digit;
      digit = g_random_int_range (0, 16);
      key[i] = hexdigits[digit];
    }

  key[i] = '\0';

  return key;
}

static void
wifi_hotspot_dialog_update_main_label (CcWifiHotspotDialog *self)
{
  NMAccessPoint *ap;
  GBytes *ssid = NULL;
  g_autofree gchar *active_ssid = NULL;
  g_autofree gchar *escape = NULL;
  g_autofree gchar *ssid_text = NULL;
  g_autofree gchar *label = NULL;

  g_assert (CC_IS_WIFI_HOTSPOT_DIALOG (self));

  gtk_label_set_markup (self->connection_label, "");

  if (!self->device)
    return;

  ap = nm_device_wifi_get_active_access_point (self->device);

  if (ap)
    ssid = nm_access_point_get_ssid (ap);
  if (ssid)
    active_ssid = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));

  if (!active_ssid || !*active_ssid)
    return;

  escape = g_markup_escape_text (active_ssid, -1);
  ssid_text = g_strdup_printf ("<b>%s</b>", escape);
  /* TRANSLATORS: ‘%s’ is a Wi-Fi Network(SSID) name */
  label = g_strdup_printf (_("Turning on the hotspot will disconnect from %s, "
                             "and it will not be possible to access the internet through Wi-Fi."), ssid_text);
  gtk_label_set_markup (self->connection_label, label);
}

static void
get_secrets_cb (GObject            *source_object,
                GAsyncResult       *res,
                gpointer            data)
{
  CcWifiHotspotDialog *self;
  g_autoptr(GVariant) secrets = NULL;
  NMSettingWirelessSecurity *security_setting;
  const gchar *key;
  g_autoptr(GError) error = NULL;

  secrets = nm_remote_connection_get_secrets_finish (NM_REMOTE_CONNECTION (source_object), res, &error);
  if (!secrets)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("Could not get secrets: %s", error->message);
      return;
    }

  self = CC_WIFI_HOTSPOT_DIALOG (data);

  nm_connection_update_secrets (self->connection,
                                NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                secrets, &error);
  if (error)
    {
      g_warning ("Error updating secrets: %s", error->message);
      return;
    }

  security_setting = nm_connection_get_setting_wireless_security (self->connection);
  if (self->wpa_supported)
    key = nm_setting_wireless_security_get_psk (security_setting);
  else
    key = nm_setting_wireless_security_get_wep_key (security_setting, 0);

  if (key)
    gtk_editable_set_text (GTK_EDITABLE (self->password_entry), key);

  nm_connection_clear_secrets (self->connection);
}

static void
wifi_hotspot_dialog_update_entries (CcWifiHotspotDialog *self)
{
  NMSettingWireless *setting;
  GBytes *ssid;
  g_autofree gchar *ssid_text = NULL;

  g_assert (CC_IS_WIFI_HOTSPOT_DIALOG (self));

  gtk_editable_set_text (GTK_EDITABLE (self->name_entry), "");
  gtk_editable_set_text (GTK_EDITABLE (self->password_entry), "");

  if (!self->connection)
    return;

  setting = nm_connection_get_setting_wireless (self->connection);

  ssid = nm_setting_wireless_get_ssid (setting);
  ssid_text = nm_utils_ssid_to_utf8 (g_bytes_get_data (ssid, NULL), g_bytes_get_size (ssid));

  if (!ssid_text && self->host_name)
    ssid_text = g_strdup (self->host_name);

  if (ssid_text)
    gtk_editable_set_text (GTK_EDITABLE (self->name_entry), ssid_text);

  if (!NM_IS_REMOTE_CONNECTION (self->connection))
    return;

  /* Secrets may not be already loaded, we have to manually load it. */
  nm_remote_connection_get_secrets_async (NM_REMOTE_CONNECTION (self->connection),
                                          NM_SETTING_WIRELESS_SECURITY_SETTING_NAME,
                                          self->cancellable,
                                          get_secrets_cb,
                                          self);
}

static gboolean
hotspot_password_is_valid (CcWifiHotspotDialog *self,
                           const gchar         *password)
{
  g_assert (CC_IS_WIFI_HOTSPOT_DIALOG (self));

  if (!self->device)
    return FALSE;

  if (!password || !*password)
    return TRUE;

  if (self->wpa_supported)
    return nm_utils_wpa_psk_valid (password);
  else
    return nm_utils_wep_key_valid (password, NM_WEP_KEY_TYPE_KEY);
}

static void
hotspot_entry_changed_cb (CcWifiHotspotDialog *self)
{
  const gchar *ssid, *ssid_error_label, *password, *password_error_label;
  gboolean valid_ssid, valid_password;

  g_assert (CC_IS_WIFI_HOTSPOT_DIALOG (self));

  valid_ssid = valid_password = FALSE;
  ssid = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));
  password = gtk_editable_get_text (GTK_EDITABLE (self->password_entry));

  if (!ssid || !*ssid)
    {
      ssid_error_label = _("Network name cannot be empty");
      widget_set_error (GTK_WIDGET (self->name_entry));
    }
  else if (strlen (ssid) > 32)
    {
      /* SSID length needs to be in the 1-32 byte range */
      ssid_error_label = _("Network name is too long");
      widget_set_error (GTK_WIDGET (self->name_entry));
    }
  else
    {
      valid_ssid = TRUE;
      ssid_error_label = "";
      widget_unset_error (GTK_WIDGET (self->name_entry));
    }

  valid_password = hotspot_password_is_valid (self, password);

  if (valid_password)
    {
      password_error_label = "";
      widget_unset_error (GTK_WIDGET (self->password_entry));
    }
  else
    {
      if (strlen (password) < 8)
        {
          password_error_label = _("Must have a minimum of 8 characters");
        }
      else
        {
          guint max_chars = self->wpa_supported ? 63 : 16;
          password_error_label = g_strdup_printf (ngettext ("Must have a maximum of %d character",
                                                            "Must have a maximum of %d characters", max_chars), max_chars);
        }

      widget_set_error (GTK_WIDGET(self->password_entry));
    }

  gtk_label_set_label (self->name_error_label, ssid_error_label);
  gtk_label_set_label (self->password_error_label, password_error_label);
  gtk_widget_set_sensitive (GTK_WIDGET (self->ok_button),
                            valid_ssid && valid_password);
}

static void
generate_password_clicked_cb (CcWifiHotspotDialog *self)
{
  g_autofree gchar *key = NULL;

  g_assert (CC_IS_WIFI_HOTSPOT_DIALOG (self));

  if (self->wpa_supported)
    key = get_random_wpa_key ();
  else
    key = get_random_wep_key ();

  gtk_editable_set_text (GTK_EDITABLE (self->password_entry), key);
}

static void
hotspot_update_wireless_settings (CcWifiHotspotDialog *self)
{
  NMSettingWireless *setting;
  g_autoptr(GBytes) ssid = NULL;
  const gchar *ssid_text;
  NMDeviceWifiCapabilities capabilities;

  g_assert (CC_IS_WIFI_HOTSPOT_DIALOG (self));

  if (nm_connection_get_setting_wireless (self->connection) == NULL)
    nm_connection_add_setting (self->connection, nm_setting_wireless_new ());

  setting = nm_connection_get_setting_wireless (self->connection);

  capabilities = nm_device_wifi_get_capabilities (self->device);
  if (capabilities & NM_WIFI_DEVICE_CAP_AP)
    g_object_set (setting, "mode", "ap", NULL);
  else
    g_object_set (setting, "mode", "adhoc", NULL);

  ssid_text = gtk_editable_get_text (GTK_EDITABLE (self->name_entry));
  ssid = g_bytes_new (ssid_text, strlen (ssid_text));
  g_object_set (setting, "ssid", ssid, NULL);
}

static void
hotspot_update_wireless_security_settings (CcWifiHotspotDialog *self)
{
  NMSettingWirelessSecurity *setting;
  const gchar *value, *key_type;

  g_assert (CC_IS_WIFI_HOTSPOT_DIALOG (self));

  if (nm_connection_get_setting_wireless_security (self->connection) == NULL)
    nm_connection_add_setting (self->connection, nm_setting_wireless_security_new ());

  setting = nm_connection_get_setting_wireless_security (self->connection);
  nm_setting_wireless_security_clear_protos (setting);
  nm_setting_wireless_security_clear_pairwise (setting);
  nm_setting_wireless_security_clear_groups (setting);
  value = gtk_editable_get_text (GTK_EDITABLE (self->password_entry));

  if (self->wpa_supported)
    key_type = "psk";
  else
    key_type = "wep-key0";

  if (self->wpa_supported)
    g_object_set (setting, "key-mgmt", "wpa-psk", NULL);
  else
    g_object_set (setting,
                  "key-mgmt", "none",
                  "wep-key-type", NM_WEP_KEY_TYPE_KEY,
                  NULL);

  if (!value || !*value)
    {
      g_autofree gchar *key = NULL;

      if (self->wpa_supported)
        key = get_random_wpa_key ();
      else
        key = get_random_wep_key ();

      g_object_set (setting, key_type, key, NULL);
    }
  else
    g_object_set (setting, key_type, value, NULL);

  if (self->wpa_supported)
    {
      NMDeviceWifiCapabilities caps;

      caps = nm_device_wifi_get_capabilities (self->device);

      if (caps & NM_WIFI_DEVICE_CAP_RSN)
        {
          nm_setting_wireless_security_add_proto (setting, "rsn");
          nm_setting_wireless_security_add_pairwise (setting, "ccmp");
          nm_setting_wireless_security_add_group (setting, "ccmp");
        }
      else if (caps & NM_WIFI_DEVICE_CAP_WPA)
        {
          nm_setting_wireless_security_add_proto (setting, "wpa");
          nm_setting_wireless_security_add_pairwise (setting, "tkip");
          nm_setting_wireless_security_add_group (setting, "tkip");
        }
    }
}

static void
cc_wifi_hotspot_dialog_finalize (GObject *object)
{
  CcWifiHotspotDialog *self = (CcWifiHotspotDialog *)object;

  g_cancellable_cancel(self->cancellable);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->host_name, g_free);
  g_clear_object (&self->device);
  g_clear_object (&self->connection);

  G_OBJECT_CLASS (cc_wifi_hotspot_dialog_parent_class)->finalize (object);
}

static void
cc_wifi_hotspot_dialog_show (GtkWidget *widget)
{
  CcWifiHotspotDialog *self = (CcWifiHotspotDialog *)widget;
  g_warn_if_fail (self->device != NULL);

  gtk_widget_grab_focus (GTK_WIDGET (self->ok_button));
  wifi_hotspot_dialog_update_entries (self);

  if (!self->connection)
    if (self->host_name)
      gtk_editable_set_text (GTK_EDITABLE (self->name_entry), self->host_name);

  GTK_WIDGET_CLASS (cc_wifi_hotspot_dialog_parent_class)->show (widget);
}

static void
cc_wifi_hotspot_dialog_response (GtkDialog *dialog,
                                 gint       response_id)
{
  CcWifiHotspotDialog *self = CC_WIFI_HOTSPOT_DIALOG (dialog);
  NMSetting *setting;

  if (response_id != GTK_RESPONSE_APPLY)
    return;

  if (!self->connection)
    self->connection = NM_CONNECTION (nm_simple_connection_new ());

  if (nm_connection_get_setting_connection (self->connection) == NULL)
    {
      setting = nm_setting_connection_new ();
      g_object_set (setting,
                    "type", "802-11-wireless",
                    "id", "Hotspot",
                    "autoconnect", FALSE,
                    NULL);
      nm_connection_add_setting (self->connection, setting);
    }

  if (nm_connection_get_setting_ip4_config (self->connection) == NULL)
    {
      setting = nm_setting_ip4_config_new ();
      g_object_set (setting, "method", "shared", NULL);
      nm_connection_add_setting (self->connection, setting);
    }

  hotspot_update_wireless_settings (self);
  hotspot_update_wireless_security_settings (self);
}

static void
cc_wifi_hotspot_dialog_class_init (CcWifiHotspotDialogClass *klass)
{
  GtkDialogClass *dialog_class = GTK_DIALOG_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_wifi_hotspot_dialog_finalize;

  widget_class->show = cc_wifi_hotspot_dialog_show;
  dialog_class->response = cc_wifi_hotspot_dialog_response;

  gtk_widget_class_set_template_from_resource (widget_class,
                                               "/org/gnome/control-center/network/cc-wifi-hotspot-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotDialog, connection_label);
  gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotDialog, name_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotDialog, name_error_label);
  gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotDialog, password_entry);
  gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotDialog, password_error_label);
  gtk_widget_class_bind_template_child (widget_class, CcWifiHotspotDialog, ok_button);

  gtk_widget_class_bind_template_callback (widget_class, hotspot_entry_changed_cb);
  gtk_widget_class_bind_template_callback (widget_class, generate_password_clicked_cb);
}

static void
cc_wifi_hotspot_dialog_init (CcWifiHotspotDialog *self)
{
  self->cancellable = g_cancellable_new ();

  gtk_widget_init_template (GTK_WIDGET (self));
}

CcWifiHotspotDialog *
cc_wifi_hotspot_dialog_new (GtkWindow *parent_window)
{
  g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);

  return g_object_new (CC_TYPE_WIFI_HOTSPOT_DIALOG,
                       "use-header-bar", TRUE,
                       "transient-for", parent_window,
                       NULL);
}

void
cc_wifi_hotspot_dialog_set_hostname (CcWifiHotspotDialog *self,
                                     const gchar         *host_name)
{
  g_return_if_fail (CC_IS_WIFI_HOTSPOT_DIALOG (self));

  g_clear_pointer (&self->host_name, g_free);
  self->host_name = g_strdup (host_name);
}

void
cc_wifi_hotspot_dialog_set_device (CcWifiHotspotDialog *self,
                                   NMDeviceWifi        *device)
{
  g_return_if_fail (CC_IS_WIFI_HOTSPOT_DIALOG (self));
  g_return_if_fail (NM_IS_DEVICE_WIFI (device));

  g_set_object (&self->device, device);

  if (device)
    {
      NMDeviceWifiCapabilities caps;

      caps = nm_device_wifi_get_capabilities (device);
      self->wpa_supported = FALSE;

      if (caps & NM_WIFI_DEVICE_CAP_AP)
        if (caps & (NM_WIFI_DEVICE_CAP_RSN | NM_WIFI_DEVICE_CAP_WPA))
          self->wpa_supported = TRUE;
    }

  wifi_hotspot_dialog_update_main_label (self);
}

NMConnection *
cc_wifi_hotspot_dialog_get_connection (CcWifiHotspotDialog *self)
{
  g_return_val_if_fail (CC_IS_WIFI_HOTSPOT_DIALOG (self), NULL);

  return self->connection;
}

void
cc_wifi_hotspot_dialog_set_connection (CcWifiHotspotDialog *self,
                                       NMConnection        *connection)
{
  NMSettingWireless *setting;

  g_return_if_fail (CC_IS_WIFI_HOTSPOT_DIALOG (self));
  g_return_if_fail (NM_IS_CONNECTION (connection));

  setting = nm_connection_get_setting_wireless (connection);
  g_return_if_fail (setting);

  g_set_object (&self->connection, connection);
}
