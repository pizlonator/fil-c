/* cc-firmware-security-page.c
 *
 * Copyright (C) 2021 Red Hat, Inc
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Kate Hsuan <hpa@redhat.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "shell/cc-application.h"

#include "cc-firmware-security-page.h"
#include "cc-firmware-security-dialog.h"
#include "cc-firmware-security-boot-dialog.h"
#include "cc-firmware-security-help-dialog.h"
#include "cc-firmware-security-utils.h"
#include "cc-hostname.h"
#include "cc-util.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gi18n.h>

struct _CcFirmwareSecurityPage
{
  AdwNavigationPage parent_instance;

  GtkButton        *hsi_button;
  GtkButton        *secure_boot_button;

  /* Stack */
  GtkWidget        *panel_stack;

  /* HSI button */
  GtkWidget        *hsi_grid;

  GtkWidget        *hsi_circle_box;
  GtkWidget        *hsi_circle_number;
  GtkWidget        *hsi_icon;

  GtkWidget        *hsi_label;
  GtkWidget        *hsi_description;

  /* secure boot button */
  GtkWidget        *secure_boot_button_grid;
  GtkWidget        *secure_boot_icon;
  GtkWidget        *secure_boot_label;
  GtkWidget        *secure_boot_description;

  /* event listbox */
  GtkWidget        *firmware_security_log_listbox;
  GtkWidget        *firmware_security_log_stack;
  GtkWidget        *firmware_security_log_pgroup;

  GCancellable     *cancellable;
  guint             timeout_id;

  GDBusProxy       *bus_proxy;
  GDBusProxy       *properties_bus_proxy;

  GHashTable       *hsi1_dict;
  GHashTable       *hsi2_dict;
  GHashTable       *hsi3_dict;
  GHashTable       *hsi4_dict;
  GHashTable       *runtime_dict;
  GString          *event_log_output;

  guint             hsi_number;
  SecureBootState   secure_boot_state;
};

G_DEFINE_TYPE (CcFirmwareSecurityPage, cc_firmware_security_page, ADW_TYPE_NAVIGATION_PAGE)

static void
set_hsi_button_view (CcFirmwareSecurityPage *self);

static void
set_secure_boot_button_view (CcFirmwareSecurityPage *self)
{
  FwupdSecurityAttr *attr;
  guint64 sb_flags = 0;
  guint64 pk_flags = 0;

  /* get HSI-1 flags if set */
  attr = g_hash_table_lookup (self->hsi1_dict, FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT);
  if (attr != NULL)
    sb_flags = attr->flags;
  attr = g_hash_table_lookup (self->hsi1_dict, FWUPD_SECURITY_ATTR_ID_UEFI_PK);
  if (attr != NULL)
    pk_flags = attr->flags;

  /* enabled and valid */
  if ((sb_flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS) > 0 &&
      (pk_flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS) > 0)
    {
      self->secure_boot_state = SECURE_BOOT_STATE_ACTIVE;
    }
  else if ((sb_flags & FWUPD_SECURITY_ATTR_RESULT_ENABLED) > 0)
    {
      self->secure_boot_state = SECURE_BOOT_STATE_PROBLEMS;
    }
  else
    {
      self->secure_boot_state = SECURE_BOOT_STATE_INACTIVE;
    }

  /* update UI */
  if (self->secure_boot_state == SECURE_BOOT_STATE_ACTIVE)
   {
      gtk_label_set_text (GTK_LABEL (self->secure_boot_label), _("Secure Boot is Active"));
      gtk_label_set_text (GTK_LABEL (self->secure_boot_description), _("Protected against malicious software when the device starts."));
      gtk_image_set_from_icon_name (GTK_IMAGE (self->secure_boot_icon), "channel-secure-symbolic");
      gtk_widget_add_css_class (self->secure_boot_icon, "good");
    }
  else if (self->secure_boot_state == SECURE_BOOT_STATE_PROBLEMS)
   {
      gtk_label_set_text (GTK_LABEL (self->secure_boot_label), _("Secure Boot has Problems"));
      gtk_label_set_text (GTK_LABEL (self->secure_boot_description), _("Some protection when the device is started."));
      gtk_widget_add_css_class (self->secure_boot_icon, "error");
    }
  else
    {
      gtk_label_set_text (GTK_LABEL (self->secure_boot_label), _("Secure Boot is Off"));
      gtk_label_set_text (GTK_LABEL (self->secure_boot_description), _("No protection when the device is started."));
      gtk_widget_add_css_class (self->secure_boot_icon, "error");
    }
}

static gchar *
fu_security_attr_get_description_for_eventlog (FwupdSecurityAttr *attr)
{
  GString *str = g_string_new (attr->description);

  /* nothing to do */
  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS)
    return g_string_free (str, FALSE);

  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONTACT_OEM &&
      attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW)
    {
      g_string_append_printf (str, "\n\n%s",
                              /* TRANSLATORS: this is to explain an event that has already happened */
                              _("This issue could have been caused by a change in UEFI firmware "
                                "settings, an operating system configuration change, or because of "
                                "malicious software on this system."));
    }
  else if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_FW)
    {
      g_string_append_printf (str, "\n\n%s",
                              /* TRANSLATORS: this is to explain an event that has already happened */
                              _("This issue could have been caused by a change in the UEFI firmware "
                                "settings, or because of malicious software on this system."));
    }
  else if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_ACTION_CONFIG_OS)
    {
      g_string_append_printf (str, "\n\n%s",
                              /* TRANSLATORS: this is to explain an event that has already happened */
                              _("This issue could have been caused by an operating system configuration "
                                "change, or because of malicious software on this system."));
    }

  return g_string_free (str, FALSE);
}

static void
parse_event_variant_iter (CcFirmwareSecurityPage *self,
                          GVariantIter           *iter)
{
  g_autofree gchar *date_string = NULL;
  g_autoptr (GDateTime) date = NULL;
  g_autoptr (FwupdSecurityAttr) attr = fu_security_attr_new_from_variant(iter);
  GtkWidget *row;

  /* unknown to us */
  if (attr->appstream_id == NULL || attr->title == NULL)
    return;

  /* skip events that have either been added or removed with no prior value */
  if (attr->result == FWUPD_SECURITY_ATTR_RESULT_UNKNOWN ||
      attr->result_fallback == FWUPD_SECURITY_ATTR_RESULT_UNKNOWN)
    return;

  /* build new row */
  row = adw_expander_row_new ();
  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS)
    {
      adw_expander_row_set_icon_name (ADW_EXPANDER_ROW (row), "emblem-ok");
      gtk_widget_add_css_class (row, "success-icon");
    }
  else
    {
      adw_expander_row_set_icon_name (ADW_EXPANDER_ROW (row), "process-stop");
      gtk_widget_add_css_class (row, "error-icon");
    }

  if (attr->description != NULL)
    {
      GtkWidget *subrow = adw_action_row_new ();
      g_autofree gchar *str = fu_security_attr_get_description_for_eventlog (attr);
      adw_action_row_set_subtitle (ADW_ACTION_ROW (subrow), str);
      adw_expander_row_add_row (ADW_EXPANDER_ROW (row), subrow);
    }
  else
    {
      adw_expander_row_set_enable_expansion (ADW_EXPANDER_ROW (row), FALSE);
      gtk_widget_add_css_class (row, "hide-arrow");
    }

  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), attr->title);

  g_string_append (self->event_log_output, "  ");
  date = g_date_time_new_from_unix_local (attr->timestamp);
  date_string = g_date_time_format (date, "%Y-%m-%d %H:%M:%S");
  /* TRANSLATOR: this is the date in "%Y-%m-%d %H:%M:%S" format,
     for example: 2022-08-01 22:48:00 */
  g_string_append_printf (self->event_log_output, _("%1$s"), date_string);
  g_string_append (self->event_log_output, "   ");
  hsi_report_title_print_padding (attr->title, self->event_log_output, 30);

  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS)
    /* TRANSLATOR: This is the text event status output when the event status is "success" */
    g_string_append (self->event_log_output, _("Pass"));
  else
    /* TRANSLATOR: This is the text event status output when the event status is not "success" */
    g_string_overwrite (self->event_log_output, self->event_log_output->len-2, _("! Fail"));

  g_string_append (self->event_log_output, " ");
  g_string_append_printf (self->event_log_output, _("(%1$s → %2$s)"),
                          fwupd_security_attr_result_to_string (attr->result_fallback),
                          fwupd_security_attr_result_to_string (attr->result));
  g_string_append (self->event_log_output, "\n");

  adw_expander_row_set_subtitle (ADW_EXPANDER_ROW (row), date_string);
  adw_preferences_group_add (ADW_PREFERENCES_GROUP (self->firmware_security_log_pgroup), GTK_WIDGET (row));

  adw_view_stack_set_visible_child_name (ADW_VIEW_STACK (self->firmware_security_log_stack), "page2");
}

static void
parse_variant_iter (CcFirmwareSecurityPage *self,
                    GVariantIter           *iter)
{
  g_autoptr (FwupdSecurityAttr) attr = fu_security_attr_new_from_variant(iter);
  const gchar *appstream_id = attr->appstream_id;

  /* invalid */
  if (appstream_id == NULL)
    return;

  /* skip obsoleted */
  if (attr->flags & FWUPD_SECURITY_ATTR_FLAG_OBSOLETED)
    return;

  /* in fwupd <= 1.8.3 org.fwupd.hsi.Uefi.SecureBoot was incorrectly marked as HSI-0,
   * so lower the HSI number forcefully if this attribute failed -- the correct thing
   * to do of course is to update fwupd to a newer build */
  if (g_strcmp0 (attr->appstream_id, FWUPD_SECURITY_ATTR_ID_UEFI_SECUREBOOT) == 0 &&
      (attr->flags & FWUPD_SECURITY_ATTR_FLAG_SUCCESS) == 0)
    {
      self->hsi_number = 0;
      set_hsi_button_view (self);
    }

  /* insert into correct hash table */
  switch (attr->hsi_level)
    {
      case 1:
        g_hash_table_insert (self->hsi1_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
        break;
      case 2:
        g_hash_table_insert (self->hsi2_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
        break;
      case 3:
        g_hash_table_insert (self->hsi3_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
        break;
      case 4:
        g_hash_table_insert (self->hsi4_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
        break;
      default:
        g_hash_table_insert (self->runtime_dict,
                             g_strdup (appstream_id),
                             g_steal_pointer (&attr));
    }
}

static void
parse_data_from_variant (CcFirmwareSecurityPage *self,
                         GVariant               *value,
                         const gboolean          is_event)
{
  const gchar *type_string;
  g_autoptr (GVariantIter) iter = NULL;

  type_string = g_variant_get_type_string (value);
  if (g_strcmp0 (type_string, "(a{sv})") == 0)
    {
      g_variant_get (value, "(a{sv})", &iter);
      if (is_event)
        parse_event_variant_iter (self, iter);
      else
        parse_variant_iter (self, iter);
    }
  else if (g_strcmp0 (type_string, "a{sv}") == 0)
    {
      g_variant_get (value, "a{sv}", &iter);
      if (is_event)
        parse_event_variant_iter (self, iter);
      else
        parse_variant_iter (self, iter);
    }
  else
    {
      g_warning ("type %s not known", type_string);
    }
}

static void
parse_array_from_variant (CcFirmwareSecurityPage *self,
                          GVariant               *value,
                          const gboolean          is_event)
{
  gsize sz;
  g_autoptr (GVariant) untuple = NULL;

  untuple = g_variant_get_child_value (value, 0);
  sz = g_variant_n_children (untuple);
  for (guint i = 0; i < sz; i++)
    {
      g_autoptr (GVariant) data = NULL;
      data = g_variant_get_child_value (untuple, i);
      if (is_event)
        parse_data_from_variant (self, data, TRUE);
      else
        parse_data_from_variant (self, data, FALSE);
    }
}

static void
on_bus_event_done_cb (GObject      *source,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) val = NULL;
  CcFirmwareSecurityPage *self = CC_FIRMWARE_SECURITY_PAGE (user_data);

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (val == NULL)
    {
      g_warning ("failed to get Security Attribute Event: %s", error->message);
      return;
    }

  parse_array_from_variant (self, val, TRUE);
}

static void
show_loading_page (CcFirmwareSecurityPage *self, const gchar *page_name)
{
  gtk_stack_set_visible_child_name (GTK_STACK (self->panel_stack), page_name);
}

static int
on_timeout_cb (gpointer user_data)
{
  CcFirmwareSecurityPage *self = CC_FIRMWARE_SECURITY_PAGE (user_data);
  show_loading_page (self, "panel_show");
  self->timeout_id = 0;
  return 0;
}

static int
on_timeout_unavaliable (gpointer user_data)
{
  CcFirmwareSecurityPage *self = CC_FIRMWARE_SECURITY_PAGE (user_data);
  show_loading_page (self, "panel_unavaliable");
  self->timeout_id = 0;
  return 0;
}

static void
on_bus_done (GObject      *source,
             GAsyncResult *res,
             gpointer      user_data)
{
  CcFirmwareSecurityPage *self = CC_FIRMWARE_SECURITY_PAGE (user_data);
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) val = NULL;

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (val == NULL)
    {
      self->timeout_id = g_timeout_add (1500, on_timeout_unavaliable, self);
      return;
    }

  parse_array_from_variant (self, val, FALSE);
  set_secure_boot_button_view (self);
  self->timeout_id = g_timeout_add (1500, on_timeout_cb, self);
}

static void
on_bus_ready_cb (GObject       *source_object,
                 GAsyncResult  *res,
                 gpointer       user_data)
{
  g_autoptr (GError) error = NULL;
  CcFirmwareSecurityPage *self = CC_FIRMWARE_SECURITY_PAGE (user_data);

  self->bus_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (self->bus_proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
          g_warning ("failed to connect fwupd: %s", error->message);

      return;
    }

  g_dbus_proxy_call (self->bus_proxy,
                     "GetHostSecurityAttrs",
                     NULL,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     on_bus_done,
                     self);
  g_dbus_proxy_call (self->bus_proxy,
                     "GetHostSecurityEvents",
                     g_variant_new ("(u)",
                                    100),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     on_bus_event_done_cb,
                     self);
}

static void
on_hsi_button_clicked_cb (CcFirmwareSecurityPage *self)
{
  GtkWindow *toplevel;
  GtkWidget *dialog;

  dialog = cc_firmware_security_dialog_new (self->hsi_number,
                                            self->hsi1_dict,
                                            self->hsi2_dict,
                                            self->hsi3_dict,
                                            self->hsi4_dict,
                                            self->runtime_dict,
                                            self->event_log_output);
  toplevel = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  gtk_window_set_transient_for (GTK_WINDOW (dialog), toplevel);
  gtk_window_present (GTK_WINDOW (dialog));
}

static void
on_secure_boot_button_clicked_cb (CcFirmwareSecurityPage *self)
{
  GtkWindow *toplevel;
  GtkWidget *boot_dialog;

  boot_dialog = cc_firmware_security_boot_dialog_new (self->secure_boot_state);
  toplevel = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  gtk_window_set_transient_for (GTK_WINDOW (boot_dialog), toplevel);
  gtk_window_present (GTK_WINDOW (boot_dialog));
}

static void
on_fw_help_button_clicked_cb (CcFirmwareSecurityPage *self)
{
  GtkWidget *help_dialog;
  GtkWindow *toplevel;

  help_dialog = cc_firmware_security_help_dialog_new ();
  toplevel = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (self)));
  gtk_window_set_transient_for (GTK_WINDOW (help_dialog), toplevel);
  gtk_window_present (GTK_WINDOW (help_dialog));
}

static void
set_hsi_button_view_contain (CcFirmwareSecurityPage *self,
                             guint                   hsi_number,
                             gchar                  *title,
                             const gchar            *description)
{
  switch (hsi_number)
    {
      case 0:
        gtk_image_set_from_icon_name (GTK_IMAGE (self->hsi_icon), "dialog-warning-symbolic");
        gtk_widget_add_css_class (self->hsi_icon, "error");
        break;
      case 1:
        gtk_image_set_from_icon_name (GTK_IMAGE (self->hsi_icon), "emblem-default-symbolic");
        gtk_widget_add_css_class (self->hsi_icon, "good");
        break;
      case 2:
      case 3:
      case 4:
        gtk_image_set_from_icon_name (GTK_IMAGE (self->hsi_icon), "security-high-symbolic");
        gtk_widget_add_css_class (self->hsi_icon, "good");
        break;
      default:
        gtk_image_set_from_icon_name (GTK_IMAGE (self->hsi_icon), "dialog-question-symbolic");
        gtk_widget_add_css_class (self->hsi_icon, "neutral");
        break;
    }

  gtk_label_set_text (GTK_LABEL (self->hsi_label), title);
  gtk_label_set_text (GTK_LABEL (self->hsi_description), description);
}

static void
set_hsi_button_view (CcFirmwareSecurityPage *self)
{
  switch (self->hsi_number)
    {
      case 0:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: 0/4 stars */
                                     _("Checks Failed"),
                                     _("Hardware does not pass checks."));
        break;
      case 1:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: 1/4 stars */
                                     _("Checks Passed"),
                                     _("Hardware meets basic security requirements."));
        break;
      case 2:
      case 3:
      case 4:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: 2~4 stars */
                                     _("Protected"),
                                     _("Hardware has a good level of protection."));
        break;
      case G_MAXUINT:
        set_hsi_button_view_contain (self,
                                     self->hsi_number,
                                     /* TRANSLATORS: in reference to firmware protection: ??? stars */
                                     _("Checks Unavailable"),
                                     _("Security levels are not available for this device."));
        break;
      default:
        g_warning ("incorrect HSI number %u", self->hsi_number);
    }
}

static void
on_properties_bus_done_cb (GObject      *source,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GVariant) val = NULL;
  const gchar *hsi_str = NULL;
  CcFirmwareSecurityPage *self = CC_FIRMWARE_SECURITY_PAGE (user_data);

  val = g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);
  if (val == NULL)
    {
      g_warning ("failed to get HSI number");
      return;
    }

  /* parse value */
  hsi_str = g_variant_get_data (val);
  if (hsi_str != NULL && g_str_has_prefix (hsi_str, "HSI:INVALID"))
    {
      self->hsi_number = G_MAXUINT;
    }
  else if (hsi_str != NULL && g_str_has_prefix (hsi_str, "HSI:"))
    {
      self->hsi_number = g_ascii_strtoll (hsi_str + 4, NULL, 10);
    }
  set_hsi_button_view (self);
}

static void
on_properties_bus_ready_cb (GObject      *source_object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
  CcFirmwareSecurityPage *self = CC_FIRMWARE_SECURITY_PAGE (user_data);
  g_autoptr (GError) error = NULL;

  self->properties_bus_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);
  if (self->properties_bus_proxy == NULL)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_warning ("failed to connect fwupd: %s", error->message);

      return;
    }

  g_dbus_proxy_call (self->properties_bus_proxy,
                     "Get",
                     g_variant_new ("(ss)",
                                    "org.freedesktop.fwupd",
                                    "HostSecurityId"),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     self->cancellable,
                     on_properties_bus_done_cb,
                     self);
}

static void
update_page_visibility (CcFirmwareSecurityPage *self)
{
  CcHostname *hostname;
  gboolean visible = TRUE;
  g_autofree gchar *chassis_type = NULL;

  hostname = cc_hostname_get_default ();
  chassis_type = cc_hostname_get_chassis_type (hostname);
  if (cc_hostname_is_vm_chassis (hostname) || g_strcmp0 (chassis_type, "") == 0)
    visible = FALSE;

  gtk_widget_set_visible (GTK_WIDGET (self), visible);
  g_debug ("Firmware Security page visible: %s as chassis was %s",
           visible ? "yes" : "no",
           chassis_type);
}

static void
cc_firmware_security_page_finalize (GObject *object)
{
  CcFirmwareSecurityPage *self = CC_FIRMWARE_SECURITY_PAGE (object);

  g_clear_pointer (&self->hsi1_dict, g_hash_table_unref);
  g_clear_pointer (&self->hsi2_dict, g_hash_table_unref);
  g_clear_pointer (&self->hsi3_dict, g_hash_table_unref);
  g_clear_pointer (&self->hsi4_dict, g_hash_table_unref);
  g_clear_pointer (&self->runtime_dict, g_hash_table_unref);
  g_string_free (self->event_log_output, TRUE);

  g_clear_object (&self->bus_proxy);
  g_clear_object (&self->properties_bus_proxy);

  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);

  if (self->timeout_id)
    g_clear_handle_id (&self->timeout_id, g_source_remove);

  G_OBJECT_CLASS (cc_firmware_security_page_parent_class)->finalize (object);
}


static void
cc_firmware_security_page_class_init (CcFirmwareSecurityPageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = cc_firmware_security_page_finalize;

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-firmware-security-page.ui");

  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, firmware_security_log_pgroup);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, firmware_security_log_stack);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, hsi_button);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, hsi_description);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, hsi_icon);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, hsi_label);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, secure_boot_button);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, secure_boot_description);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, secure_boot_icon);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, secure_boot_label);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityPage, panel_stack);

  gtk_widget_class_bind_template_callback (widget_class, on_hsi_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_secure_boot_button_clicked_cb);
  gtk_widget_class_bind_template_callback (widget_class, on_fw_help_button_clicked_cb);
}

static void
cc_firmware_security_page_init (CcFirmwareSecurityPage *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  update_page_visibility (self);

  self->hsi1_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);
  self->hsi2_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);
  self->hsi3_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);
  self->hsi4_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);
  self->runtime_dict = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) fu_security_attr_free);
  self->event_log_output = g_string_new (NULL);
  self->cancellable = g_cancellable_new ();

  load_custom_css ("/org/gnome/control-center/privacy/security-level.css");

  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.fwupd",
                            "/",
                            "org.freedesktop.DBus.Properties",
                            self->cancellable,
                            on_properties_bus_ready_cb,
                            self);
  g_dbus_proxy_new_for_bus (G_BUS_TYPE_SYSTEM,
                            G_DBUS_PROXY_FLAGS_NONE,
                            NULL,
                            "org.freedesktop.fwupd",
                            "/",
                            "org.freedesktop.fwupd",
                            self->cancellable,
                            on_bus_ready_cb,
                            self);
}
