/* cc-firmware-security-dialog.c
 *
 * Copyright (C) 2022 Red Hat, Inc
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

#include "cc-firmware-security-help-dialog.h"

struct _CcFirmwareSecurityHelpDialog
{
  AdwWindow  parent;
};

G_DEFINE_TYPE (CcFirmwareSecurityHelpDialog, cc_firmware_security_help_dialog, ADW_TYPE_WINDOW)


static void
cc_firmware_security_help_dialog_class_init (CcFirmwareSecurityHelpDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_add_binding_action (widget_class, GDK_KEY_Escape, 0, "window.close", NULL);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/privacy/cc-firmware-security-help-dialog.ui");
}

static void
cc_firmware_security_help_dialog_init (CcFirmwareSecurityHelpDialog *dialog)
{
  gtk_widget_init_template (GTK_WIDGET (dialog));
}

GtkWidget *
cc_firmware_security_help_dialog_new (void)
{
  return g_object_new (CC_TYPE_FIRMWARE_SECURITY_HELP_DIALOG, NULL);
}
