/* cc-firmware-security-boot-dialog.c
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

#include "cc-firmware-security-boot-dialog.h"

struct _CcFirmwareSecurityBootDialog
{
  AdwWindow         parent;

  GtkWidget        *secure_boot_icon;
  GtkWidget        *secure_boot_title;
  GtkWidget        *secure_boot_description;
};

G_DEFINE_TYPE (CcFirmwareSecurityBootDialog, cc_firmware_security_boot_dialog, ADW_TYPE_WINDOW)

static void
cc_firmware_security_boot_dialog_class_init (CcFirmwareSecurityBootDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-firmware-security-boot-dialog.ui");
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityBootDialog, secure_boot_title);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityBootDialog, secure_boot_icon);
  gtk_widget_class_bind_template_child (widget_class, CcFirmwareSecurityBootDialog, secure_boot_description);
}

static void
cc_firmware_security_boot_dialog_init (CcFirmwareSecurityBootDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
  load_custom_css ("/org/gnome/control-center/privacy/security-level.css");
}

GtkWidget *
cc_firmware_security_boot_dialog_new (SecureBootState secure_boot_state)
{
  CcFirmwareSecurityBootDialog *dialog;
  g_autofree gchar *status_description = NULL;

  dialog = g_object_new (CC_TYPE_FIRMWARE_SECURITY_BOOT_DIALOG, NULL);

  switch (secure_boot_state)
    {
    case SECURE_BOOT_STATE_ACTIVE:
      /* TRANSLATORS: secure boot refers to the system firmware security mode */
      gtk_label_set_text (GTK_LABEL(dialog->secure_boot_title), _("Secure Boot is Active"));
      gtk_image_set_from_icon_name (GTK_IMAGE (dialog->secure_boot_icon), "channel-secure-symbolic");
      gtk_widget_add_css_class (dialog->secure_boot_icon, "good");
      status_description = g_strdup_printf ("%s",
                                             /* TRANSLATORS: this is the first section of the decription */
                                            _("Secure boot prevents malicious software from being loaded when the device starts. "
                                              "It is currently turned on and is functioning correctly."));
      break;

    case SECURE_BOOT_STATE_PROBLEMS:
      /* TRANSLATORS: secure boot refers to the system firmware security mode */
      gtk_label_set_text (GTK_LABEL (dialog->secure_boot_title), _("Secure Boot Has Problems"));
      gtk_widget_add_css_class (dialog->secure_boot_icon, "error");
      status_description = g_strdup_printf ("%s\n\n%s\n\n%s",
                                            /* TRANSLATORS: this is the first section of the decription. */
                                            _("Secure boot prevents malicious software from being loaded when the device "
                                              "starts. It is currently turned on, but will not work due to having an invalid key."),
                                            /* TRANSLATORS: this is the second section of description. */
                                            _("Secure boot problems can often be resolved from your computer's UEFI firmware settings "
                                              "(BIOS) and your hardware manufacturer may provide information on how to do this."),
                                            /* TRANSLATORS: this is the third section of description. */
                                            _("For help, contact your hardware manufacturer or IT support provider."));
      break;

    case SECURE_BOOT_STATE_INACTIVE:
    case SECURE_BOOT_STATE_UNKNOWN:
      /* TRANSLATORS: secure boot refers to the system firmware security mode */
      gtk_label_set_text (GTK_LABEL (dialog->secure_boot_title), _("Secure Boot is Turned Off"));
      gtk_widget_add_css_class (dialog->secure_boot_icon, "error");
      status_description = g_strdup_printf ("%s\n\n%s",
                                            /* TRANSLATORS: this is the first section of the description. */
                                            _("Secure boot prevents malicious software from being loaded when the device starts. It is "
                                              "currently turned off."),
                                            /* TRANSLATORS: this is the second section of the description. */
                                            _("Secure boot can often be turned on from your computer's UEFI firmware settings (BIOS). "
                                              "For help, contact your hardware manufacturer or IT support provider."));
      break;
    }
  gtk_label_set_text (GTK_LABEL(dialog->secure_boot_description), status_description);

  return GTK_WIDGET (dialog);
}
