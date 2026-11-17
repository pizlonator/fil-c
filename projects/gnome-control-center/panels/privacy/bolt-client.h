/*
 * Copyright © 2017 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Christian J. Kellner <christian@kellner.me>
 */

#pragma once

#include "bolt-enums.h"
#include "bolt-device.h"
#include "bolt-proxy.h"

G_BEGIN_DECLS

#define BOLT_TYPE_CLIENT bolt_client_get_type ()
G_DECLARE_FINAL_TYPE (BoltClient, bolt_client, BOLT, CLIENT, BoltProxy);

BoltClient *    bolt_client_new (GError **error);

void            bolt_client_new_async (GCancellable       *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer            user_data);
BoltClient *    bolt_client_new_finish (GAsyncResult *res,
                                        GError      **error);

GPtrArray *     bolt_client_list_devices (BoltClient   *client,
                                          GCancellable *cancellable,
                                          GError      **error);

BoltDevice *    bolt_client_get_device (BoltClient   *client,
                                        const char   *uid,
                                        GCancellable *cancellable,
                                        GError      **error);

BoltDevice *    bolt_client_enroll_device (BoltClient  *client,
                                           const char  *uid,
                                           BoltPolicy   policy,
                                           BoltAuthCtrl flags,
                                           GError     **error);

void            bolt_client_enroll_device_async (BoltClient         *client,
                                                 const char         *uid,
                                                 BoltPolicy          policy,
                                                 BoltAuthCtrl        flags,
                                                 GCancellable       *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer            user_data);

gboolean        bolt_client_enroll_device_finish (BoltClient   *client,
                                                  GAsyncResult *res,
                                                  char        **path,
                                                  GError      **error);

void            bolt_client_enroll_all_async (BoltClient         *client,
                                              GPtrArray          *uuids,
                                              BoltPolicy          policy,
                                              BoltAuthCtrl        flags,
                                              GCancellable       *cancellable,
                                              GAsyncReadyCallback callback,
                                              gpointer            user_data);

gboolean        bolt_client_enroll_all_finish (BoltClient   *client,
                                               GAsyncResult *res,
                                               GError      **error);

void            bolt_client_authorize_all_async (BoltClient         *client,
                                                 GPtrArray          *uuids,
                                                 BoltAuthCtrl        flags,
                                                 GCancellable       *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer            user_data);

gboolean        bolt_client_authorize_all_finish (BoltClient   *client,
                                                  GAsyncResult *res,
                                                  GError      **error);

void            bolt_client_connect_all_async (BoltClient         *client,
                                               GPtrArray          *devices,
                                               BoltPolicy          policy,
                                               BoltAuthCtrl        flags,
                                               GCancellable       *cancellable,
                                               GAsyncReadyCallback callback,
                                               gpointer            user_data);

gboolean        bolt_client_connect_all_finish (BoltClient   *client,
                                                GAsyncResult *res,
                                                GError      **error);


gboolean        bolt_client_forget_device (BoltClient *client,
                                           const char *uid,
                                           GError    **error);

void            bolt_client_forget_device_async (BoltClient         *client,
                                                 const char         *uid,
                                                 GCancellable       *cancellable,
                                                 GAsyncReadyCallback callback,
                                                 gpointer            user_data);

gboolean        bolt_client_forget_device_finish (BoltClient   *client,
                                                  GAsyncResult *res,
                                                  GError      **error);

/* getter */
guint           bolt_client_get_version (BoltClient *client);

gboolean        bolt_client_is_probing (BoltClient *client);

BoltSecurity    bolt_client_get_security (BoltClient *client);

BoltAuthMode    bolt_client_get_authmode (BoltClient *client);

/* setter */

void            bolt_client_set_authmode_async (BoltClient         *client,
                                                BoltAuthMode        mode,
                                                GCancellable       *cancellable,
                                                GAsyncReadyCallback callback,
                                                gpointer            user_data);

gboolean        bolt_client_set_authmode_finish (BoltClient   *client,
                                                 GAsyncResult *res,
                                                 GError      **error);

/* utility functions */
void            bolt_devices_sort_by_syspath (GPtrArray *devices,
                                              gboolean   reverse);

G_END_DECLS
