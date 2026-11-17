/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2012 Red Hat, Inc
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
 * Author: Cosimo Cecchi <cosimoc@gnome.org>
 */

#pragma once

#include <adwaita.h>

#define CC_SEARCH_LOCATIONS_DIALOG_TYPE (cc_search_locations_dialog_get_type ())
G_DECLARE_FINAL_TYPE (CcSearchLocationsDialog, cc_search_locations_dialog, CC, SEARCH_LOCATIONS_DIALOG, AdwWindow)

CcSearchLocationsDialog *cc_search_locations_dialog_new (void);

gboolean cc_search_locations_dialog_is_available        (void);
