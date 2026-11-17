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

#include <adwaita.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef enum
{
  CC_POWER_PROFILE_PERFORMANCE = 0,
  CC_POWER_PROFILE_BALANCED    = 1,
  CC_POWER_PROFILE_POWER_SAVER = 2,
  NUM_CC_POWER_PROFILES
} CcPowerProfile;

#define CC_TYPE_POWER_PROFILE_ROW (cc_power_profile_row_get_type())
G_DECLARE_FINAL_TYPE (CcPowerProfileRow, cc_power_profile_row, CC, POWER_PROFILE_ROW, AdwActionRow)

CcPowerProfileRow *cc_power_profile_row_new           (CcPowerProfile  power_profile);
CcPowerProfile cc_power_profile_row_get_profile       (CcPowerProfileRow *row);
GtkCheckButton *cc_power_profile_row_get_radio_button (CcPowerProfileRow *row);
void cc_power_profile_row_set_active                  (CcPowerProfileRow *row, gboolean active);
gboolean cc_power_profile_row_get_active              (CcPowerProfileRow *row);

CcPowerProfile cc_power_profile_from_str (const char *profile);
const char *cc_power_profile_to_str      (CcPowerProfile profile);

G_END_DECLS
