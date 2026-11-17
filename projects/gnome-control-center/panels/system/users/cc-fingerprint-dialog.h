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

#include <gtk/gtk.h>
#include "cc-fingerprint-manager.h"

G_BEGIN_DECLS

#define CC_TYPE_FINGERPRINT_DIALOG (cc_fingerprint_dialog_get_type ())

G_DECLARE_FINAL_TYPE (CcFingerprintDialog, cc_fingerprint_dialog,
                      CC, FINGERPRINT_DIALOG, GtkWindow)

CcFingerprintDialog *cc_fingerprint_dialog_new (CcFingerprintManager *manager);

G_END_DECLS
