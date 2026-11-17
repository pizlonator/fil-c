/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2020 Canonical Ltd.
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
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Marco Trevisan <marco.trevisan@canonical.com>
 */

#pragma once

#include <glib-object.h>
#include <act/act.h>

G_BEGIN_DECLS

#define CC_TYPE_FINGERPRINT_MANAGER (cc_fingerprint_manager_get_type ())

G_DECLARE_FINAL_TYPE (CcFingerprintManager, cc_fingerprint_manager, CC, FINGERPRINT_MANAGER, GObject)

/**
 * CcFingerprintManager:
 * @CC_FINGERPRINT_STATE_NONE: Fingerprint recognition is not available
 * @CC_FINGERPRINT_STATE_UPDATING: Fingerprint recognition is being fetched
 * @CC_FINGERPRINT_STATE_ENABLED: Fingerprint recognition is enabled
 * @CC_FINGERPRINT_STATE_DISABLED: Fingerprint recognition is disabled
 *
 * The status of the fingerprint support.
 */
typedef enum {
  CC_FINGERPRINT_STATE_NONE,
  CC_FINGERPRINT_STATE_UPDATING,
  CC_FINGERPRINT_STATE_ENABLED,
  CC_FINGERPRINT_STATE_DISABLED,
} CcFingerprintState;

typedef void (*CcFingerprintStateUpdated) (CcFingerprintManager *fp_manager,
                                           CcFingerprintState    state,
                                           gpointer              user_data,
                                           GError               *error);

CcFingerprintManager * cc_fingerprint_manager_new (ActUser *user);

CcFingerprintState cc_fingerprint_manager_get_state (CcFingerprintManager *fp_manager);

ActUser * cc_fingerprint_manager_get_user (CcFingerprintManager *fp_manager);

void cc_fingerprint_manager_update_state (CcFingerprintManager     *fp_manager,
                                          CcFingerprintStateUpdated callback,
                                          gpointer                  user_data);

void cc_fingerprint_manager_get_devices (CcFingerprintManager *fp_manager,
                                         GCancellable         *cancellable,
                                         GAsyncReadyCallback   res,
                                         gpointer              user_data);

GList *cc_fingerprint_manager_get_devices_finish (CcFingerprintManager *fp_manager,
                                                  GAsyncResult         *res,
                                                  GError              **error);

G_END_DECLS
