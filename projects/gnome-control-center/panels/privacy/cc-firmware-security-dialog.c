/* cc-firmware-security-dialog.c
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

#include "config.h"

#include <glib/gi18n-lib.h>
#include <glibtop/fsusage.h>
#include <glibtop/mountlist.h>
#include <glibtop/mem.h>
#include <glibtop/sysinfo.h>

#include "cc-firmware-security-page.h"
#include "cc-firmware-security-dialog.h"
#include "cc-firmware-security-utils.h"

struct _CcFirmwareSecurityDialog
{
  AdwWindow            parent;

  GtkWidget           *firmware_security_dialog_icon;



  GtkWidget           *firmware_security_dialog_title_label;
  GtkWidget           *firmware_security_dialog_body_label;
  GtkWidget           *firmware_security_dialog_min_row;
  AdwToastOverlay     *toast_overlay;

  gboolean             is_created;

  GHashTable          *hsi1_dict;
  GHashTable          *hsi2_dict;
  GHashTable          *hsi3_dict;
  GHashTable          *hsi4_dict;
  GHashTable          *runtime_dict;

  GString             *event_log_str;

  guint                hsi_number;
};

G_DEFINE_TYPE (CcFirmwareSecurityDialog, cc_firmware_security_dialog, ADW_TYPE_WINDOW)

static void
set_dialog_item_layer1 (CcFirmwareSecurityDialog *self,
                        const gchar              *icon_name,
                        const gchar              *title,
                        const gchar              *body)
{
  g_autofree gchar *str = NULL;

  gtk_image_set_from_icon_name (GTK_IMAGE (self->firmware_security_dialog_icon), icon_name);
  gtk_label_set_text (GTK_LABEL (self->firmware_security_dialog_title_label), title);
  gtk_label_set_text (GTK_LABEL (self->firmware_security_dialog_body_label), body);

  if (self->hsi_number == G_MAXUINT)
    {
        gtk_widget_add_css_class (self->firmware_security_dialog_icon, "neutral");
        return;
    }

  switch (self->hsi_number)
    {
      case 0:
        gtk_widget_add_css_class (self->firmware_security_dialog_icon, "error");
        break;
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
        gtk_widget_add_css_class (self->firmware_security_dialog_icon, "good");
        break;
      default:
        gtk_widget_add_css_class (self->firmware_security_dialog_icon, "neutral");
    }
}

static void
update_dialog (CcFirmwareSecurityDialog *self)
{
  switch (self->hsi_number)
    {
    case 0:
      set_dialog_item_layer1 (self,
                              "dialog-warning-symbolic",
                              _("Checks Failed"),
                              /* TRANSLATORS: This is the description to describe the failure on
                                 checking the security items. */
                              _("Hardware does not pass checks. "
                                "This means that you are not protected against common hardware security issues."
                                "\n\n"
                                "It may be possible to resolve hardware security issues by updating your firmware or changing device configuration options. "
                                "However, failures can stem from the physical hardware itself, and may not be reversible."));
      break;

    case 1:
      gtk_window_set_default_size (GTK_WINDOW (&self->parent), 380, 380);
      set_dialog_item_layer1 (self,
                              "emblem-default-symbolic",
                              _("Checks Passed"),
                              /* TRANSLATORS: This description describes the device passing the
                                 minimum requirement of security check.*/
                              _("This device meets basic security requirements. "
                                "Its hardware has protection against some of the most common security threats."));
      break;

    case 2:
    case 3:
    case 4:
    case 5:
      gtk_window_set_default_size (GTK_WINDOW (&self->parent), 400, 390);
      set_dialog_item_layer1 (self,
                              "security-high-symbolic",
                              _("Protected"),
                              /* TRANSLATOR: This description describes the devices passing
                                 the extended security check. */
                              _("This device passes current security tests. "
                                "Its hardware is protected against the majority of security threats."));
      break;

    default:
      gtk_window_set_default_size (GTK_WINDOW (&self->parent), 400, 390);
      set_dialog_item_layer1 (self,
                              "dialog-question-symbolic",
                              _("Checks Unavailable"),
                              /* TRANSLATORS: When the security result is unavailable, this description is shown. */
                              _("Device security checks are not available for this device. "
                                "It is not possible to tell whether it meets security requirements."));
    }
}

static gchar *
get_os_name (void)
{
  g_autofree gchar *name = NULL;
  g_autofree gchar *version_id = NULL;
  g_autofree gchar *pretty_name = NULL;

  name = g_get_os_info (G_OS_INFO_KEY_NAME);
  version_id = g_get_os_info (G_OS_INFO_KEY_VERSION_ID);
  pretty_name = g_get_os_info (G_OS_INFO_KEY_PRETTY_NAME);

  if (pretty_name)
    return g_steal_pointer (&pretty_name);
  else if (name && version_id)
    return g_strdup_printf ("%s %s", name, version_id);
  else
    return g_strdup (_("Unknown"));
}

static gchar*
cpu_get_model ()
{
  gchar *model;
  const glibtop_sysinfo * sysinfo;

  glibtop_init();
  sysinfo = glibtop_get_sysinfo ();
  model = g_strdup (g_hash_table_lookup (sysinfo->cpuinfo [1].values, "model name"));
  glibtop_close ();

  return model;
}

static gchar*
fwupd_get_property (const char *property_name)
{
  g_autoptr(GDBusConnection) connection = NULL;
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) inner = NULL;
  g_autoptr(GVariant) variant = NULL;
  const gchar *ret_property;

  connection = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, &error);
  if (!connection)
    {
      g_warning ("system bus not available: %s", error->message);
      return NULL;
    }
  variant = g_dbus_connection_call_sync (connection,
                                         "org.freedesktop.fwupd",
                                         "/",
                                         "org.freedesktop.DBus.Properties",
                                         "Get",
                                         g_variant_new ("(ss)",
                                                        "org.freedesktop.fwupd",
                                                        property_name),
                                         NULL,
                                         G_DBUS_CALL_FLAGS_NONE,
                                         -1,
                                         NULL,
                                         &error);
  if (!variant)
    {
      g_warning ("Cannot get org.freedesktop.fwupd: %s", error->message);
      return NULL;
    }
  g_variant_get (variant, "(v)", &inner);
  ret_property = g_variant_get_string (inner, NULL);

  return g_strdup (ret_property);
}

static void
on_hsi_detail_button_clicked_cb (CcFirmwareSecurityDialog *self)
{
  GdkClipboard *clip_board;
  GdkDisplay *display; 
  g_autoptr (GList) hash_keys;
  g_autoptr (GString) result_str;
  g_autofree gchar *date_string = NULL;
  g_autoptr (GDateTime) date = NULL;
  g_autofree gchar *fwupd_ver = NULL;
  g_autofree gchar *vendor = NULL;
  g_autofree gchar *product = NULL;
  g_autofree gchar *os_name = NULL;
  g_autofree gchar *hsi_level = NULL;
  g_autofree gchar *cpu_model = NULL;
  const gchar *hsi_result;
  g_autoptr (GString) tmp_str;
  
  GHashTable *hsi_dict = NULL;

  tmp_str = g_string_new (NULL);

  result_str = g_string_new (NULL);

  // TRANSLATORS: device security report fields are left untranslated as developers expect bug reports in English
  g_string_append (result_str, "Device Security Report");
  g_string_append (result_str, "\n======================\n\n");

  g_string_append (result_str, "Report details");
  g_string_append (result_str, "\n");

  g_string_append (result_str, "  ");
  hsi_report_title_print_padding ("Date generated:", result_str, 0);
  date = g_date_time_new_now_local ();
  date_string = g_date_time_format (date, "%Y-%m-%d %H:%M:%S");

  g_string_append_printf (result_str, "%s\n", date_string);

  g_string_append (result_str, "  ");
  hsi_report_title_print_padding ("fwupd version:", result_str, 00);
  fwupd_ver = fwupd_get_property ("DaemonVersion");
  g_string_append_printf (result_str, "%s", fwupd_ver);
  g_string_append (result_str, "\n\n");

  g_string_append (result_str, "System details");
  g_string_append (result_str, "\n");

  g_string_append (result_str, "  ");
  hsi_report_title_print_padding ("Hardware model:", result_str, 0);
  vendor = fwupd_get_property ("HostVendor");
  product = fwupd_get_property ("HostProduct");
  g_string_append_printf (result_str, "%s %s\n", vendor, product);

  g_string_append (result_str, "  ");
  hsi_report_title_print_padding ("Processor:", result_str, 0);
  cpu_model = cpu_get_model ();
  g_string_append_printf (result_str, "%s\n", cpu_model);

  g_string_append (result_str, "  ");
  hsi_report_title_print_padding ("OS:", result_str, 0);
  os_name = get_os_name ();
  g_string_append_printf (result_str, "%s\n", os_name);

  g_string_append (result_str, "  ");
  hsi_report_title_print_padding ("Security level:", result_str, 0);
  hsi_level = fwupd_get_property ("HostSecurityId");
  g_string_append_printf (result_str, "%s\n", hsi_level);
  g_string_append (result_str, "\n");

  for (int i = 1; i <=5; i++)
    {
      switch (i)
      {
        case 1:
          hsi_dict = self->hsi1_dict;
          break;
        case 2:
          hsi_dict = self->hsi2_dict;
          break;
        case 3:
          hsi_dict = self->hsi3_dict;
          break;
        case 4:
          hsi_dict = self->hsi4_dict;
          break;
        case 5:
          hsi_dict = self->runtime_dict;
      }

      if (i <= 4)
        {
          g_string_append_printf (result_str, "HSI-");
          g_string_append_printf (result_str, "%i ", i);
          g_string_append (result_str, "Tests");
          g_string_append (result_str, "\n");
        } 
      else
        {
          g_string_append (result_str, "Runtime Tests");
          g_string_append (result_str, "\n");
        }

      hash_keys = g_hash_table_get_keys (hsi_dict);
      for (GList *item = g_list_first (hash_keys); item != NULL; item = g_list_next (item))
        {
          FwupdSecurityAttr *attr = g_hash_table_lookup (hsi_dict, item->data);
          if (g_strcmp0 (attr->appstream_id, FWUPD_SECURITY_ATTR_ID_SUPPORTED_CPU) == 0)
            continue;
          if (attr->title == NULL)
            continue;
          g_string_printf (tmp_str, "%s:", attr->title);
          g_string_append (result_str, "  ");
          hsi_report_title_print_padding (tmp_str->str, result_str, 0);
          if (firmware_security_attr_has_flag (attr, FWUPD_SECURITY_ATTR_FLAG_SUCCESS))
            {
              /* Passed */
              g_string_append (result_str, "Pass");
              g_string_append (result_str, " ");
            }
          else
            {
              /* Failed */
              result_str = g_string_overwrite (result_str, result_str->len-2, "! Fail");
              g_string_append (result_str, " ");
            }
          hsi_result = fwupd_security_attr_result_to_string (attr->result);
          if (hsi_result) {
            g_string_append_printf (result_str, "(%s)", hsi_result);
          }
          g_string_append (result_str, "\n");
        }
        g_string_append (result_str, "\n");
    }

    g_string_append (result_str, "Host security events");
    g_string_append (result_str, "\n");
    g_string_append (result_str, self->event_log_str->str);
    g_string_append (result_str, "\n");
    g_string_append (result_str, "For information on the contents of this report, see https://fwupd.github.io/hsi.html");

    display = gdk_display_get_default ();
    clip_board = gdk_display_get_clipboard (display);
    gdk_clipboard_set_text (clip_board, result_str->str);
    adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (_("Report copied to clipboard")));
}

static void
cc_firmware_security_dialog_class_init (CcFirmwareSecurityDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-firmware-security-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_icon);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_title_label);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, firmware_security_dialog_body_label);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityDialog, toast_overlay);

  gtk_widget_class_bind_template_callback (widget_class, on_hsi_detail_button_clicked_cb);
}

static void
cc_firmware_security_dialog_init (CcFirmwareSecurityDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
  load_custom_css ("/org/gnome/control-center/privacy/security-level.css");
}

GtkWidget *
cc_firmware_security_dialog_new (guint       hsi_number,
                                 GHashTable *hsi1_dict,
                                 GHashTable *hsi2_dict,
                                 GHashTable *hsi3_dict,
                                 GHashTable *hsi4_dict,
                                 GHashTable *runtime_dict,
                                 GString    *event_log_str)
{
  CcFirmwareSecurityDialog *dialog;

  dialog = g_object_new (CC_TYPE_FIRMWARE_SECURITY_DIALOG, NULL);
  dialog->hsi_number = hsi_number;
  dialog->is_created = FALSE;
  dialog->hsi1_dict = hsi1_dict;
  dialog->hsi2_dict = hsi2_dict;
  dialog->hsi3_dict = hsi3_dict;
  dialog->hsi4_dict = hsi4_dict;
  dialog->runtime_dict = runtime_dict;
  dialog->event_log_str = event_log_str;
  update_dialog (dialog);

  return GTK_WIDGET (dialog);
}
