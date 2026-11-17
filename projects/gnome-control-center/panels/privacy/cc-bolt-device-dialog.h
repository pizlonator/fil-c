/* Copyright (C) 2018 Red Hat, Inc
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
 * Authors: Christian J. Kellner <ckellner@redhat.com>
 *
 */

#pragma once

#include <adwaita.h>

#include "bolt-client.h"
#include "bolt-device.h"

G_BEGIN_DECLS

#define CC_TYPE_BOLT_DEVICE_DIALOG cc_bolt_device_dialog_get_type ()

G_DECLARE_FINAL_TYPE (CcBoltDeviceDialog, cc_bolt_device_dialog, CC, BOLT_DEVICE_DIALOG, AdwWindow);

CcBoltDeviceDialog * cc_bolt_device_dialog_new (void);

void                 cc_bolt_device_dialog_set_client (CcBoltDeviceDialog *dialog,
                                                       BoltClient         *client);

void                 cc_bolt_device_dialog_set_device (CcBoltDeviceDialog *dialog,
                                                       BoltDevice         *device,
						       GPtrArray          *parents);

BoltDevice *         cc_bolt_device_dialog_peek_device (CcBoltDeviceDialog *dialog);

gboolean             cc_bolt_device_dialog_device_equal (CcBoltDeviceDialog *dialog,
                                                         BoltDevice         *device);

G_END_DECLS
