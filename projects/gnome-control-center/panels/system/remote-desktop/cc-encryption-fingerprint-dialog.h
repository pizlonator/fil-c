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

#pragma once

#include <adwaita.h>

#define GCR_API_SUBJECT_TO_CHANGE
#include <gcr/gcr.h>

G_BEGIN_DECLS

#define CC_TYPE_ENCRYPTION_FINGERPRINT_DIALOG (cc_encryption_fingerprint_dialog_get_type ())

G_DECLARE_FINAL_TYPE (CcEncryptionFingerprintDialog, cc_encryption_fingerprint_dialog, CC, ENCRYPTION_FINGERPRINT_DIALOG, AdwWindow)

void cc_encryption_fingerprint_dialog_set_certificate (CcEncryptionFingerprintDialog *self,
                                                       GTlsCertificate                *certificate);
void cc_encryption_fingerprint_dialog_set_fingerprint (CcEncryptionFingerprintDialog *self,
                                                       const gchar                   *fingerprint,
                                                       const gchar                    *separator);

G_END_DECLS
