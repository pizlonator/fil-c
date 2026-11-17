/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-data.h
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

#pragma once

#include <glib-object.h>
#include <libmm-glib.h>
#include <NetworkManager.h>

G_BEGIN_DECLS

#define CC_WWAN_APN_PRIORITY_LOW  (1)
#define CC_WWAN_APN_PRIORITY_HIGH (2)

#define CC_TYPE_WWAN_DATA_APN (cc_wwan_data_apn_get_type())
G_DECLARE_FINAL_TYPE (CcWwanDataApn, cc_wwan_data_apn, CC, WWAN_DATA_APN, GObject)

#define CC_TYPE_WWAN_DATA (cc_wwan_data_get_type())
G_DECLARE_FINAL_TYPE (CcWwanData, cc_wwan_data, CC, WWAN_DATA, GObject)

CcWwanData    *cc_wwan_data_new                   (MMObject             *mm_object,
                                                   NMClient             *nm_client);
GError        *cc_wwan_data_get_error             (CcWwanData           *self);
const gchar   *cc_wwan_data_get_simple_html_error (CcWwanData           *self);
GListModel    *cc_wwan_data_get_apn_list          (CcWwanData           *self);
void           cc_wwan_data_save_apn              (CcWwanData           *self,
                                                   CcWwanDataApn        *apn,
                                                   GCancellable         *cancellable,
                                                   GAsyncReadyCallback   callback,
                                                   gpointer              user_data);
CcWwanDataApn *cc_wwan_data_save_apn_finish       (CcWwanData           *self,
                                                   GAsyncResult         *result,
                                                   GError              **error);
void           cc_wwan_data_save_settings         (CcWwanData          *self,
                                                   GCancellable        *cancellable,
                                                   GAsyncReadyCallback  callback,
                                                   gpointer             user_data);
gboolean       cc_wwan_data_save_settings_finish  (CcWwanData          *self,
                                                   GAsyncResult        *result,
                                                   GError             **error);
gboolean       cc_wwan_data_delete_apn            (CcWwanData          *self,
                                                   CcWwanDataApn       *apn,
                                                   GCancellable        *cancellable,
                                                   GError             **error);
gboolean       cc_wwan_data_set_default_apn       (CcWwanData          *self,
                                                   CcWwanDataApn       *apn);
CcWwanDataApn *cc_wwan_data_get_default_apn       (CcWwanData          *self);
gboolean       cc_wwan_data_get_enabled           (CcWwanData          *self);
void           cc_wwan_data_set_enabled           (CcWwanData          *self,
                                                   gboolean             enabled);
gboolean       cc_wwan_data_get_roaming_enabled   (CcWwanData          *self);
void           cc_wwan_data_set_roaming_enabled   (CcWwanData          *self,
                                                   gboolean             enable_roaming);

CcWwanDataApn *cc_wwan_data_apn_new          (void);
const gchar   *cc_wwan_data_apn_get_name     (CcWwanDataApn *apn);
void           cc_wwan_data_apn_set_name     (CcWwanDataApn *apn,
                                              const gchar   *name);
const gchar   *cc_wwan_data_apn_get_apn      (CcWwanDataApn *apn);
void           cc_wwan_data_apn_set_apn      (CcWwanDataApn *apn,
                                              const gchar   *apn_name);
const gchar   *cc_wwan_data_apn_get_username (CcWwanDataApn *apn);
void           cc_wwan_data_apn_set_username (CcWwanDataApn *apn,
                                              const gchar   *username);
const gchar   *cc_wwan_data_apn_get_password (CcWwanDataApn *apn);
void           cc_wwan_data_apn_set_password (CcWwanDataApn *apn,
                                              const gchar   *password);
gint           cc_wwan_data_get_priority     (CcWwanData    *self);
void           cc_wwan_data_set_priority     (CcWwanData    *self,
                                              int            priority);

G_END_DECLS
