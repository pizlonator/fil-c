/* cc-firmware-security-dialog.h
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

#pragma once

#include <adwaita.h>

G_BEGIN_DECLS

#define CC_TYPE_FIRMWARE_SECURITY_DIALOG (cc_firmware_security_dialog_get_type ())
G_DECLARE_FINAL_TYPE (CcFirmwareSecurityDialog, cc_firmware_security_dialog, CC, FIRMWARE_SECURITY_DIALOG, AdwWindow)

GtkWidget * cc_firmware_security_dialog_new (guint       hsi_number,
                                             GHashTable *hsi1_dict,
                                             GHashTable *hsi2_dict,
                                             GHashTable *hsi3_dict,
                                             GHashTable *hsi4_dict,
                                             GHashTable *runtime_dict,
                                             GString    *event_log_str);

G_END_DECLS
