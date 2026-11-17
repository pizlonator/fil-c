/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-device.h
 *
 * Copyright 2019-2020 Purism SPC
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

#pragma once

#include <glib-object.h>
#include <libmm-glib.h>

#if defined(HAVE_NETWORK_MANAGER) && defined(BUILD_NETWORK)
# include "cc-wwan-data.h"
#endif

G_BEGIN_DECLS

typedef enum
{
  CC_WWAN_REGISTRATION_STATE_UNKNOWN,
  CC_WWAN_REGISTRATION_STATE_IDLE,
  CC_WWAN_REGISTRATION_STATE_REGISTERED,
  CC_WWAN_REGISTRATION_STATE_ROAMING,
  CC_WWAN_REGISTRATION_STATE_SEARCHING,
  CC_WWAN_REGISTRATION_STATE_DENIED
} CcWwanState;

typedef struct _CcWwanData CcWwanData;

#define CC_TYPE_WWAN_DEVICE (cc_wwan_device_get_type())
G_DECLARE_FINAL_TYPE (CcWwanDevice, cc_wwan_device, CC, WWAN_DEVICE, GObject)

CcWwanDevice  *cc_wwan_device_new                (MMObject            *mm_object,
                                                  GObject             *nm_client);
gboolean       cc_wwan_device_has_sim            (CcWwanDevice        *self);
MMModemLock    cc_wwan_device_get_lock           (CcWwanDevice        *self);
gboolean       cc_wwan_device_get_sim_lock       (CcWwanDevice        *self);
guint          cc_wwan_device_get_unlock_retries (CcWwanDevice        *self,
                                                  MMModemLock          lock);
void           cc_wwan_device_enable_pin         (CcWwanDevice        *self,
                                                  const gchar         *pin,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
gboolean       cc_wwan_device_enable_pin_finish  (CcWwanDevice        *self,
                                                  GAsyncResult        *result,
                                                  GError             **error);
void           cc_wwan_device_disable_pin        (CcWwanDevice        *self,
                                                  const gchar         *pin,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
gboolean       cc_wwan_device_disable_pin_finish (CcWwanDevice        *self,
                                                  GAsyncResult        *result,
                                                  GError             **error);
void           cc_wwan_device_send_pin           (CcWwanDevice        *self,
                                                  const gchar         *pin,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
gboolean       cc_wwan_device_send_pin_finish    (CcWwanDevice        *self,
                                                  GAsyncResult        *result,
                                                  GError             **error);
void          cc_wwan_device_send_puk            (CcWwanDevice        *self,
                                                  const gchar         *puk,
                                                  const gchar         *pin,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
gboolean      cc_wwan_device_send_puk_finish     (CcWwanDevice        *self,
                                                  GAsyncResult        *result,
                                                  GError             **error);
void           cc_wwan_device_change_pin         (CcWwanDevice        *self,
                                                  const gchar         *old_pin,
                                                  const gchar         *new_pin,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
gboolean       cc_wwan_device_change_pin_finish  (CcWwanDevice        *self,
                                                  GAsyncResult        *result,
                                                  GError             **error);
const gchar   *cc_wwan_device_get_operator_name  (CcWwanDevice        *self);
gchar         *cc_wwan_device_dup_own_numbers    (CcWwanDevice        *self);
gchar         *cc_wwan_device_dup_network_type_string (CcWwanDevice   *self);
gchar         *cc_wwan_device_dup_signal_string  (CcWwanDevice        *self);
const gchar   *cc_wwan_device_get_manufacturer   (CcWwanDevice        *self);
const gchar   *cc_wwan_device_get_model          (CcWwanDevice        *self);
const gchar   *cc_wwan_device_get_firmware_version (CcWwanDevice      *self);
const gchar   *cc_wwan_device_get_identifier     (CcWwanDevice        *self);
gboolean       cc_wwan_device_get_current_mode   (CcWwanDevice        *self,
                                                  MMModemMode         *allowed,
                                                  MMModemMode         *preferred);
gboolean       cc_wwan_device_is_auto_network    (CcWwanDevice        *self);
CcWwanState    cc_wwan_device_get_network_state  (CcWwanDevice        *self);
gboolean       cc_wwan_device_get_supported_modes (CcWwanDevice       *self,
                                                   MMModemMode        *allowed,
                                                   MMModemMode        *preferred);
void           cc_wwan_device_set_current_mode   (CcWwanDevice        *self,
                                                  MMModemMode          allowed,
                                                  MMModemMode          preferred,
                                                  GCancellable        *cancellable,
                                                  GAsyncReadyCallback  callback,
                                                  gpointer             user_data);
gboolean       cc_wwan_device_set_current_mode_finish (CcWwanDevice        *self,
                                                       GAsyncResult        *result,
                                                       GError             **error);
gchar         *cc_wwan_device_get_string_from_mode    (CcWwanDevice        *self,
                                                       MMModemMode          allowed,
                                                       MMModemMode          preferred);
void           cc_wwan_device_scan_networks           (CcWwanDevice        *self,
                                                       GCancellable        *cancellable,
                                                       GAsyncReadyCallback  callback,
                                                       gpointer             user_data);
GList         *cc_wwan_device_scan_networks_finish    (CcWwanDevice        *self,
                                                       GAsyncResult        *result,
                                                       GError             **error);
void           cc_wwan_device_register_network        (CcWwanDevice        *self,
                                                       const gchar         *network_id,
                                                       GCancellable        *cancellable,
                                                       GAsyncReadyCallback  callback,
                                                       gpointer             user_data);
gboolean       cc_wwan_device_register_network_finish (CcWwanDevice        *self,
                                                       GAsyncResult        *result,
                                                       GError             **error);
const gchar   *cc_wwan_device_get_simple_error        (CcWwanDevice        *self);
GSList        *cc_wwan_device_get_apn_list            (CcWwanDevice        *self);
gboolean       cc_wwan_device_is_nm_device            (CcWwanDevice        *self,
                                                       GObject             *nm_device);
const gchar   *cc_wwan_device_get_path                (CcWwanDevice        *self);
CcWwanData    *cc_wwan_device_get_data                (CcWwanDevice        *self);
gboolean       cc_wwan_device_pin_valid               (const gchar         *password,
                                                       MMModemLock          lock);

G_END_DECLS
