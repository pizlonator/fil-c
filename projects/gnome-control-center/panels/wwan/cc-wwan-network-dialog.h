/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-wwan-network-dialog.h
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

#include <adwaita.h>
#include <shell/cc-panel.h>

#include "cc-wwan-device.h"

G_BEGIN_DECLS

#define CC_TYPE_WWAN_NETWORK_DIALOG (cc_wwan_network_dialog_get_type())
G_DECLARE_FINAL_TYPE (CcWwanNetworkDialog, cc_wwan_network_dialog, CC, WWAN_NETWORK_DIALOG, GtkDialog)

GtkWindow *cc_wwan_network_dialog_new (GtkWindow    *parent_window,
                                       CcWwanDevice *device);

G_END_DECLS
