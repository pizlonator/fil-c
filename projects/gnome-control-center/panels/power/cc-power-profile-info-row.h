/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* cc-list-row.h
 *
 * Copyright 2020 Red Hat Inc
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
 *   Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define CC_TYPE_POWER_PROFILE_INFO_ROW (cc_power_profile_info_row_get_type())
G_DECLARE_FINAL_TYPE (CcPowerProfileInfoRow, cc_power_profile_info_row, CC, POWER_PROFILE_INFO_ROW, GtkListBoxRow)

CcPowerProfileInfoRow *cc_power_profile_info_row_new           (const char *text);

G_END_DECLS
