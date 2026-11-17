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
#include "bolt-proxy.h"

G_BEGIN_DECLS

#define BOLT_TYPE_DEVICE bolt_device_get_type ()
G_DECLARE_FINAL_TYPE (BoltDevice, bolt_device, BOLT, DEVICE, BoltProxy);

BoltDevice *  bolt_device_new_for_object_path (GDBusConnection *bus,
                                               const char      *path,
                                               GCancellable    *cancellable,
                                               GError         **error);

gboolean      bolt_device_authorize (BoltDevice   *dev,
                                     BoltAuthCtrl  flags,
                                     GCancellable *cancellable,
                                     GError      **error);

void          bolt_device_authorize_async (BoltDevice         *dev,
                                           BoltAuthCtrl        flags,
                                           GCancellable       *cancellable,
                                           GAsyncReadyCallback callback,
                                           gpointer            user_data);

gboolean      bolt_device_authorize_finish (BoltDevice   *dev,
                                            GAsyncResult *res,
                                            GError      **error);

/* getter */
const char *      bolt_device_get_uid (BoltDevice *dev);

const char *      bolt_device_get_name (BoltDevice *dev);

const char *      bolt_device_get_vendor (BoltDevice *dev);

BoltDeviceType    bolt_device_get_device_type (BoltDevice *dev);

BoltStatus        bolt_device_get_status (BoltDevice *dev);

BoltAuthFlags     bolt_device_get_authflags (BoltDevice *dev);

const char *      bolt_device_get_parent (BoltDevice *dev);

const char *      bolt_device_get_syspath (BoltDevice *dev);

guint64           bolt_device_get_conntime (BoltDevice *dev);

guint64           bolt_device_get_authtime (BoltDevice *dev);

gboolean          bolt_device_is_stored (BoltDevice *dev);

BoltPolicy        bolt_device_get_policy (BoltDevice *dev);

BoltKeyState      bolt_device_get_keystate (BoltDevice *dev);

guint64           bolt_device_get_storetime (BoltDevice *dev);

const char *      bolt_device_get_label (BoltDevice *dev);

/* derived getter */
char *            bolt_device_get_display_name (BoltDevice *dev);

guint64           bolt_device_get_timestamp (BoltDevice *dev);

G_END_DECLS
