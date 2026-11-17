/*
 * Copyright 2024 Red Hat, Inc
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

#include "cc-encryption-fingerprint-dialog.h"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

struct _CcEncryptionFingerprintDialog {
  AdwWindow parent_instance;

  GtkLabel *fingerprint_left_label;
  GtkLabel *fingerprint_right_label;
};

G_DEFINE_TYPE (CcEncryptionFingerprintDialog, cc_encryption_fingerprint_dialog, ADW_TYPE_WINDOW)

static void
cc_encryption_fingerprint_dialog_class_init (CcEncryptionFingerprintDialogClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/control-center/system/remote-desktop/cc-encryption-fingerprint-dialog.ui");

  gtk_widget_class_bind_template_child (widget_class, CcEncryptionFingerprintDialog, fingerprint_left_label);
  gtk_widget_class_bind_template_child (widget_class, CcEncryptionFingerprintDialog, fingerprint_right_label);
}

static void
cc_encryption_fingerprint_dialog_init (CcEncryptionFingerprintDialog *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

void
cc_encryption_fingerprint_dialog_set_fingerprint (CcEncryptionFingerprintDialog *self,
                                                  const gchar                   *fingerprint,
                                                  const gchar                   *separator)
{
  g_auto(GStrv) fingerprintv = NULL;
  g_autofree char *left_string = NULL;
  g_autofree char *right_string = NULL;

  fingerprintv = g_strsplit (fingerprint, separator, -1);
  g_return_if_fail (g_strv_length (fingerprintv) == 32);

  left_string = g_strdup_printf (
    "%s:%s:%s:%s\n"
    "%s:%s:%s:%s\n"
    "%s:%s:%s:%s\n"
    "%s:%s:%s:%s\n",
    fingerprintv[0], fingerprintv[1], fingerprintv[2], fingerprintv[3],
    fingerprintv[8], fingerprintv[9], fingerprintv[10], fingerprintv[11],
    fingerprintv[16], fingerprintv[17], fingerprintv[18], fingerprintv[19],
    fingerprintv[24], fingerprintv[25], fingerprintv[26], fingerprintv[27]);

 right_string = g_strdup_printf (
   "%s:%s:%s:%s\n"
   "%s:%s:%s:%s\n"
   "%s:%s:%s:%s\n"
   "%s:%s:%s:%s\n",
   fingerprintv[4], fingerprintv[5], fingerprintv[6], fingerprintv[7],
   fingerprintv[12], fingerprintv[13], fingerprintv[14], fingerprintv[15],
   fingerprintv[20], fingerprintv[21], fingerprintv[22], fingerprintv[23],
   fingerprintv[28], fingerprintv[29], fingerprintv[30], fingerprintv[31]);

  gtk_label_set_label (GTK_LABEL (self->fingerprint_left_label), left_string);
  gtk_label_set_label (GTK_LABEL (self->fingerprint_right_label), right_string);
}

void
cc_encryption_fingerprint_dialog_set_certificate (CcEncryptionFingerprintDialog *self,
                                                  GTlsCertificate               *certificate)
{
  g_autofree char *fingerprint = NULL;
  g_autoptr(GByteArray) der = NULL;
  g_autoptr(GcrCertificate) gcr_cert = NULL;
  g_return_if_fail (self);
  g_return_if_fail (certificate);

  g_object_get (certificate, "certificate", &der, NULL);
  gcr_cert = gcr_simple_certificate_new (der->data, der->len);
  if (!gcr_cert)
    {
      g_warning ("Failed to load GCR TLS certificate representation");
      return;
    }

  fingerprint = gcr_certificate_get_fingerprint_hex (gcr_cert, G_CHECKSUM_SHA256);
  cc_encryption_fingerprint_dialog_set_fingerprint (self, fingerprint, " ");
}
